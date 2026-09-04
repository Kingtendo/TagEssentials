#include <windows.h>
#include <jni.h>
#include <jvmti.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {

HMODULE g_module = nullptr;
jvmtiEnv* g_jvmti = nullptr;
SRWLOCK g_outputLock = SRWLOCK_INIT;
HANDLE g_manifest = INVALID_HANDLE_VALUE;
HANDLE g_log = INVALID_HANDLE_VALUE;
std::wstring g_snapshotDirectory;
std::wstring g_classesDirectory;
std::atomic<LONG> g_nextClassId{ 0 };
std::atomic<LONG> g_dumpedClasses{ 0 };
std::atomic<LONG> g_failedWrites{ 0 };
volatile LONG g_captureEnabled = 0;

using JNIGetCreatedJavaVMsFn = jint(JNICALL*)(JavaVM**, jsize, jsize*);

std::wstring ParentDirectory(std::wstring path) {
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L".";
    path.resize(slash);
    return path;
}

std::wstring GetModulePath(HMODULE module) {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return L"";
    path.resize(length);
    return path;
}

bool EnsureDirectory(const std::wstring& path) {
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

void WriteAll(HANDLE file, const void* bytes, DWORD byteCount) {
    if (file == INVALID_HANDLE_VALUE || !bytes || byteCount == 0) return;
    const auto* cursor = static_cast<const unsigned char*>(bytes);
    DWORD remaining = byteCount;
    while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(file, cursor, remaining, &written, nullptr) || written == 0) return;
        cursor += written;
        remaining -= written;
    }
}

void Log(const char* format, ...) {
    char line[4096] = {};
    SYSTEMTIME time = {};
    GetLocalTime(&time);
    int prefix = snprintf(
        line,
        sizeof(line),
        "[%02u:%02u:%02u.%03u pid=%lu tid=%lu] ",
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        GetCurrentProcessId(),
        GetCurrentThreadId());
    if (prefix < 0) prefix = 0;

    va_list args;
    va_start(args, format);
    const int body = vsnprintf(line + prefix, sizeof(line) - static_cast<size_t>(prefix), format, args);
    va_end(args);

    size_t length = static_cast<size_t>(prefix);
    if (body > 0) length += static_cast<size_t>(body);
    if (length > sizeof(line) - 3) length = sizeof(line) - 3;
    line[length++] = '\r';
    line[length++] = '\n';

    AcquireSRWLockExclusive(&g_outputLock);
    WriteAll(g_log, line, static_cast<DWORD>(length));
    if (g_log != INVALID_HANDLE_VALUE) FlushFileBuffers(g_log);
    ReleaseSRWLockExclusive(&g_outputLock);
}

bool StartsWith(const char* value, const char* prefix) {
    if (!value || !prefix) return false;
    while (*prefix) {
        if (*value++ != *prefix++) return false;
    }
    return true;
}

bool IsApplicationClass(const char* name) {
    if (!name || !*name || name[0] == '[') return false;
    static const char* excludedPrefixes[] = {
        "java/",
        "javax/",
        "jdk/",
        "sun/",
        "com/sun/",
        "org/w3c/",
        "org/xml/"
    };
    for (const char* prefix : excludedPrefixes) {
        if (StartsWith(name, prefix)) return false;
    }
    return true;
}

std::string CleanManifestName(const char* name) {
    std::string result = name ? name : "<unnamed>";
    for (char& ch : result) {
        if (ch == '\t' || ch == '\r' || ch == '\n') ch = '_';
    }
    return result;
}

void JNICALL ClassFileLoadHook(
    jvmtiEnv*,
    JNIEnv*,
    jclass classBeingRedefined,
    jobject,
    const char* name,
    jobject,
    jint classDataLength,
    const unsigned char* classData,
    jint*,
    unsigned char**) {
    if (InterlockedCompareExchange(&g_captureEnabled, 0, 0) == 0) return;
    if (!IsApplicationClass(name) || classDataLength <= 0 || !classData) return;

    const LONG classId = g_nextClassId.fetch_add(1) + 1;
    wchar_t fileName[64] = {};
    swprintf_s(fileName, L"class_%07ld.class", classId);
    const std::wstring outputPath = g_classesDirectory + L"\\" + fileName;

    HANDLE output = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    bool writeOk = output != INVALID_HANDLE_VALUE;
    if (writeOk) {
        DWORD written = 0;
        writeOk = WriteFile(output, classData, static_cast<DWORD>(classDataLength), &written, nullptr) &&
            written == static_cast<DWORD>(classDataLength);
        CloseHandle(output);
    }

    if (writeOk) {
        g_dumpedClasses.fetch_add(1);
    }
    else {
        g_failedWrites.fetch_add(1);
    }

    const std::string cleanName = CleanManifestName(name);
    char manifestLine[8192] = {};
    const int lineLength = snprintf(
        manifestLine,
        sizeof(manifestLine),
        "%ld\t%d\t%s\t%s\t%s\r\n",
        classId,
        static_cast<int>(classDataLength),
        classBeingRedefined ? "retransform" : "load",
        writeOk ? "ok" : "write_failed",
        cleanName.c_str());

    if (lineLength > 0) {
        const DWORD safeLength = static_cast<DWORD>(
            lineLength < static_cast<int>(sizeof(manifestLine)) ? lineLength : sizeof(manifestLine));
        AcquireSRWLockExclusive(&g_outputLock);
        WriteAll(g_manifest, manifestLine, safeLength);
        ReleaseSRWLockExclusive(&g_outputLock);
    }
}

std::string SignatureToName(const char* signature) {
    if (!signature || signature[0] != 'L') return "";
    const size_t length = strlen(signature);
    if (length < 3 || signature[length - 1] != ';') return "";
    return std::string(signature + 1, length - 2);
}

bool PrepareOutput() {
    const std::wstring modulePath = GetModulePath(g_module);
    if (modulePath.empty()) return false;

    const std::wstring toolsDirectory = ParentDirectory(modulePath);
    const std::wstring dumpRoot = ParentDirectory(toolsDirectory);

    SYSTEMTIME time = {};
    GetLocalTime(&time);
    wchar_t snapshotName[128] = {};
    swprintf_s(
        snapshotName,
        L"runtime-%04u%02u%02u-%02u%02u%02u-pid%lu",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        GetCurrentProcessId());

    g_snapshotDirectory = dumpRoot + L"\\" + snapshotName;
    g_classesDirectory = g_snapshotDirectory + L"\\classes";
    if (!EnsureDirectory(g_snapshotDirectory) || !EnsureDirectory(g_classesDirectory)) return false;

    g_log = CreateFileW(
        (g_snapshotDirectory + L"\\dump.log").c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    g_manifest = CreateFileW(
        (g_snapshotDirectory + L"\\classes.tsv").c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (g_log == INVALID_HANDLE_VALUE || g_manifest == INVALID_HANDLE_VALUE) return false;

    static const char header[] = "id\tbytes\tevent\tstatus\tbinary_name\r\n";
    WriteAll(g_manifest, header, static_cast<DWORD>(sizeof(header) - 1));
    return true;
}

void CloseOutput() {
    AcquireSRWLockExclusive(&g_outputLock);
    if (g_manifest != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_manifest);
        CloseHandle(g_manifest);
        g_manifest = INVALID_HANDLE_VALUE;
    }
    if (g_log != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(g_log);
        CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&g_outputLock);
}

DWORD WINAPI DumpThread(void*) {
    if (!PrepareOutput()) return 1;
    Log("Lunar runtime class snapshot starting");

    const std::wstring javaPath = GetModulePath(nullptr);
    Log("Java executable=%ls", javaPath.c_str());

    HMODULE jvmModule = GetModuleHandleW(L"jvm.dll");
    if (!jvmModule) {
        Log("Failed: jvm.dll is not loaded");
        CloseOutput();
        return 2;
    }

    auto getCreatedJavaVMs = reinterpret_cast<JNIGetCreatedJavaVMsFn>(
        GetProcAddress(jvmModule, "JNI_GetCreatedJavaVMs"));
    if (!getCreatedJavaVMs) {
        Log("Failed: JNI_GetCreatedJavaVMs is unavailable");
        CloseOutput();
        return 3;
    }

    JavaVM* vm = nullptr;
    jsize vmCount = 0;
    const jint vmResult = getCreatedJavaVMs(&vm, 1, &vmCount);
    if (vmResult != JNI_OK || vmCount < 1 || !vm) {
        Log("Failed: no created JVM result=%d count=%d", static_cast<int>(vmResult), static_cast<int>(vmCount));
        CloseOutput();
        return 4;
    }

    JNIEnv* env = nullptr;
    bool attachedHere = false;
    jint envResult = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8);
    if (envResult == JNI_EDETACHED) {
        envResult = vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr);
        attachedHere = envResult == JNI_OK;
    }
    if (envResult != JNI_OK || !env) {
        Log("Failed: could not attach native dump thread result=%d", static_cast<int>(envResult));
        CloseOutput();
        return 5;
    }

    const jint jvmtiResult = vm->GetEnv(reinterpret_cast<void**>(&g_jvmti), JVMTI_VERSION_1_2);
    if (jvmtiResult != JNI_OK || !g_jvmti) {
        Log("Failed: JVMTI unavailable result=%d", static_cast<int>(jvmtiResult));
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 6;
    }

    jvmtiCapabilities potential = {};
    jvmtiError error = g_jvmti->GetPotentialCapabilities(&potential);
    if (error != JVMTI_ERROR_NONE) {
        Log("Failed: GetPotentialCapabilities error=%d", static_cast<int>(error));
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 7;
    }

    jvmtiCapabilities requested = {};
    requested.can_retransform_classes = potential.can_retransform_classes;
    requested.can_retransform_any_class = potential.can_retransform_any_class;
    requested.can_generate_all_class_hook_events = potential.can_generate_all_class_hook_events;
    error = g_jvmti->AddCapabilities(&requested);
    if (error != JVMTI_ERROR_NONE) {
        Log("Failed: AddCapabilities error=%d", static_cast<int>(error));
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 8;
    }

    jvmtiCapabilities acquired = {};
    g_jvmti->GetCapabilities(&acquired);
    Log(
        "JVMTI capabilities retransform=%d any=%d all_hooks=%d",
        acquired.can_retransform_classes ? 1 : 0,
        acquired.can_retransform_any_class ? 1 : 0,
        acquired.can_generate_all_class_hook_events ? 1 : 0);
    if (!acquired.can_retransform_classes) {
        Log("Failed: VM did not grant class retransformation capability");
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 9;
    }

    jvmtiEventCallbacks callbacks = {};
    callbacks.ClassFileLoadHook = &ClassFileLoadHook;
    error = g_jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));
    if (error != JVMTI_ERROR_NONE) {
        Log("Failed: SetEventCallbacks error=%d", static_cast<int>(error));
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 10;
    }

    InterlockedExchange(&g_captureEnabled, 1);
    error = g_jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    if (error != JVMTI_ERROR_NONE) {
        Log("Failed: enabling ClassFileLoadHook error=%d", static_cast<int>(error));
        InterlockedExchange(&g_captureEnabled, 0);
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 11;
    }

    jint loadedCount = 0;
    jclass* loadedClasses = nullptr;
    error = g_jvmti->GetLoadedClasses(&loadedCount, &loadedClasses);
    if (error != JVMTI_ERROR_NONE || !loadedClasses) {
        Log("Failed: GetLoadedClasses error=%d count=%d", static_cast<int>(error), static_cast<int>(loadedCount));
        g_jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
        InterlockedExchange(&g_captureEnabled, 0);
        if (attachedHere) vm->DetachCurrentThread();
        CloseOutput();
        return 12;
    }

    std::vector<jclass> batch;
    batch.reserve(64);
    LONG selectedCount = 0;
    LONG unmodifiableCount = 0;
    LONG batchFailures = 0;

    auto flushBatch = [&]() {
        if (batch.empty()) return;
        const jvmtiError batchError = g_jvmti->RetransformClasses(
            static_cast<jint>(batch.size()),
            batch.data());
        if (batchError != JVMTI_ERROR_NONE) {
            ++batchFailures;
            Log("Batch retransform failed error=%d count=%u; retrying individually",
                static_cast<int>(batchError),
                static_cast<unsigned int>(batch.size()));
            for (jclass klass : batch) {
                const jvmtiError singleError = g_jvmti->RetransformClasses(1, &klass);
                if (singleError != JVMTI_ERROR_NONE) {
                    Log("Individual retransform failed error=%d", static_cast<int>(singleError));
                }
            }
        }
        batch.clear();
    };

    for (jint index = 0; index < loadedCount; ++index) {
        char* signature = nullptr;
        const jvmtiError signatureError = g_jvmti->GetClassSignature(loadedClasses[index], &signature, nullptr);
        if (signatureError != JVMTI_ERROR_NONE || !signature) continue;

        const std::string name = SignatureToName(signature);
        g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(signature));
        if (name.empty() || !IsApplicationClass(name.c_str())) continue;

        jboolean modifiable = JNI_FALSE;
        const jvmtiError modifiableError = g_jvmti->IsModifiableClass(loadedClasses[index], &modifiable);
        if (modifiableError != JVMTI_ERROR_NONE || modifiable != JNI_TRUE) {
            ++unmodifiableCount;
            continue;
        }

        batch.push_back(loadedClasses[index]);
        ++selectedCount;
        if (batch.size() >= 64) flushBatch();
    }
    flushBatch();

    g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(loadedClasses));
    Sleep(500);
    g_jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_CLASS_FILE_LOAD_HOOK, nullptr);
    InterlockedExchange(&g_captureEnabled, 0);
    jvmtiEventCallbacks emptyCallbacks = {};
    g_jvmti->SetEventCallbacks(&emptyCallbacks, sizeof(emptyCallbacks));

    Log(
        "Snapshot complete loaded=%d selected=%ld dumped=%ld unmodifiable=%ld failed_writes=%ld batch_failures=%ld",
        static_cast<int>(loadedCount),
        selectedCount,
        g_dumpedClasses.load(),
        unmodifiableCount,
        g_failedWrites.load(),
        batchFailures);

    if (attachedHere) vm->DetachCurrentThread();
    CloseOutput();
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);
        HANDLE thread = CreateThread(nullptr, 0, &DumpThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
