#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <algorithm>
#include <cwctype>
#include <vector>
#include <compressapi.h>
#include "resource.h"

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "cabinet.lib")

class TagEssentialsLauncher {
public:
    static bool Inject() {
        std::wstring stagedDllPath;
        if (!CreateStagedDllFromResource(stagedDllPath)) {
            return false;
        }

        DWORD pid = FindMinecraftProcess();
        if (!pid) {
            ShowError(L"Supported Badlion or Lunar Client 1.8.9 javaw.exe not running!");
            DeleteFileW(stagedDllPath.c_str());
            return false;
        }

        HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
        if (!hProcess) {
            ShowError(L"Failed to open process. Run as Administrator!");
            DeleteFileW(stagedDllPath.c_str());
            return false;
        }

        InjectionResult result = PerformInjection(hProcess, stagedDllPath);
        CloseHandle(hProcess);
        // A pending loader still owns the path buffer and may open this file later.
        if (result == InjectionResult::Failed) DeleteFileW(stagedDllPath.c_str());
        return result == InjectionResult::Succeeded;
    }

private:
    enum class InjectionResult { Failed, Succeeded, Pending };

    static std::wstring GetStageDirectory() {
        wchar_t tempPath[MAX_PATH] = {};
        DWORD length = GetTempPathW(MAX_PATH, tempPath);
        if (length == 0 || length >= MAX_PATH) return L"";

        std::wstring result(tempPath);
        if (!result.empty() && result.back() == L'\\') result.pop_back();
        result += L"\\TagEssentials";
        return result;
    }

    static std::wstring GetExecutableDirectory() {
        wchar_t exePath[MAX_PATH] = {};
        DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return L"";

        std::wstring dir(exePath, length);
        size_t slash = dir.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return L"";
        dir.resize(slash);
        return dir;
    }

    static std::string WideToUtf8(const std::wstring& value) {
        if (value.empty()) return "";

        int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 1) return "";

        std::string result;
        result.resize((size_t)required - 1);
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], (int)result.size(), nullptr, nullptr);
        return result;
    }

    static std::wstring Utf8ToWide(const std::string& value) {
        if (value.empty()) return L"";

        int required = MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), nullptr, 0);
        if (required <= 0) return L"";

        std::wstring result;
        result.resize((size_t)required);
        if (MultiByteToWideChar(CP_UTF8, 0, value.data(), (int)value.size(), &result[0], required) <= 0) {
            return L"";
        }
        return result;
    }

    static bool FileExists(const std::wstring& path) {
        DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    static bool EnsureDirectoryTree(const std::wstring& path) {
        if (path.empty()) return false;

        std::wstring normalizedPath = path;
        while (normalizedPath.size() > 3 &&
            (normalizedPath.back() == L'\\' || normalizedPath.back() == L'/')) {
            normalizedPath.pop_back();
        }

        DWORD attributes = GetFileAttributesW(normalizedPath.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES) {
            return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        }

        size_t slash = normalizedPath.find_last_of(L"\\/");
        if (slash != std::wstring::npos && slash > 0) {
            std::wstring parent = normalizedPath.substr(0, slash);
            if (parent.size() > 2 && !EnsureDirectoryTree(parent)) return false;
        }

        if (CreateDirectoryW(normalizedPath.c_str(), nullptr)) return true;
        if (GetLastError() != ERROR_ALREADY_EXISTS) return false;

        attributes = GetFileAttributesW(normalizedPath.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    static std::wstring GetEnvironmentValue(const wchar_t* name) {
        std::vector<wchar_t> buffer(32768, L'\0');
        DWORD length = GetEnvironmentVariableW(name, buffer.data(), (DWORD)buffer.size());
        if (length == 0 || length >= buffer.size()) return L"";
        return std::wstring(buffer.data(), length);
    }

    static void WriteMutedVoiceRuntimeDirectory(
        const std::wstring& stageDir,
        const std::wstring& runtimeDir) {
        if (runtimeDir.empty()) return;

        std::string payload = WideToUtf8(runtimeDir);
        if (payload.empty()) return;
        payload += "\n";

        std::wstring path = stageDir + L"\\muted_voice_runtime_dir.txt";
        HANDLE file = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE) return;

        DWORD written = 0;
        WriteFile(file, payload.data(), (DWORD)payload.size(), &written, nullptr);
        CloseHandle(file);
    }

    static bool LoadResourceBytes(
        int resourceId,
        const void*& resourceBytes,
        DWORD& resourceSize,
        const wchar_t* description,
        bool required) {
        resourceBytes = nullptr;
        resourceSize = 0;

        HMODULE moduleHandle = GetModuleHandleW(nullptr);
        if (!moduleHandle) {
            if (required) ShowError(L"Failed to resolve the TagEssentials launcher module handle.");
            return false;
        }

        HRSRC resourceHandle = FindResourceW(
            moduleHandle,
            MAKEINTRESOURCEW(resourceId),
            RT_RCDATA);
        if (!resourceHandle) {
            if (required) {
                ShowError(std::wstring(L"Embedded ") + description + L" resource not found.");
            }
            return false;
        }

        resourceSize = SizeofResource(moduleHandle, resourceHandle);
        if (resourceSize == 0) {
            if (required) {
                ShowError(std::wstring(L"Embedded ") + description + L" resource is empty.");
            }
            return false;
        }

        HGLOBAL loadedResource = LoadResource(moduleHandle, resourceHandle);
        if (!loadedResource) {
            if (required) {
                ShowError(std::wstring(L"Failed to load the embedded ") + description + L" resource.");
            }
            return false;
        }

        resourceBytes = LockResource(loadedResource);
        if (!resourceBytes) {
            if (required) {
                ShowError(std::wstring(L"Failed to lock the embedded ") + description + L" resource.");
            }
            return false;
        }

        return true;
    }

    static bool HasResource(int resourceId) {
        HMODULE moduleHandle = GetModuleHandleW(nullptr);
        return moduleHandle && FindResourceW(
            moduleHandle,
            MAKEINTRESOURCEW(resourceId),
            RT_RCDATA) != nullptr;
    }

    static void StageMutedVoiceScriptFallback(const std::wstring& stageDir) {
        std::wstring exeDir = GetExecutableDirectory();
        if (exeDir.empty()) return;

        std::wstring source = exeDir + L"\\mutedVoiceBot.js";
        std::wstring target = stageDir + L"\\mutedVoiceBot.js";
        if (GetFileAttributesW(source.c_str()) == INVALID_FILE_ATTRIBUTES) return;

        CopyFileW(source.c_str(), target.c_str(), FALSE);
    }

    static void CleanupOldStagedCopies(const std::wstring& stageDir) {
        std::wstring pattern = stageDir + L"\\TagEssentialsMod_*.dll";
        WIN32_FIND_DATAW findData = {};
        HANDLE findHandle = FindFirstFileW(pattern.c_str(), &findData);
        if (findHandle == INVALID_HANDLE_VALUE) return;

        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
            std::wstring candidate = stageDir + L"\\" + findData.cFileName;
            DeleteFileW(candidate.c_str());
        } while (FindNextFileW(findHandle, &findData));

        FindClose(findHandle);
    }

    static bool LoadEmbeddedDllResource(const void*& resourceBytes, DWORD& resourceSize) {
        return LoadResourceBytes(
            IDR_TAGESSENTIALS_MOD,
            resourceBytes,
            resourceSize,
            L"DLL",
            true);
    }

    static bool WriteBytesToFile(
        const std::wstring& path,
        const void* bytes,
        size_t byteCount,
        const wchar_t* description) {
        if (byteCount > std::numeric_limits<DWORD>::max()) {
            ShowError(std::wstring(L"Embedded ") + description + L" is too large.");
            return false;
        }

        HANDLE fileHandle = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            ShowError(std::wstring(L"Failed to create the ") + description + L" file.");
            return false;
        }

        const BYTE* source = static_cast<const BYTE*>(bytes);
        size_t remaining = byteCount;
        BOOL writeOk = TRUE;
        while (remaining > 0) {
            DWORD chunk = (DWORD)std::min<size_t>(remaining, std::numeric_limits<DWORD>::max());
            DWORD bytesWritten = 0;
            if (!WriteFile(fileHandle, source, chunk, &bytesWritten, nullptr) || bytesWritten != chunk) {
                writeOk = FALSE;
                break;
            }
            source += bytesWritten;
            remaining -= bytesWritten;
        }
        CloseHandle(fileHandle);

        if (!writeOk) {
            DeleteFileW(path.c_str());
            ShowError(std::wstring(L"Failed to write the embedded ") + description + L" file.");
            return false;
        }

        return true;
    }

    static bool ReadBundleU32(const BYTE*& cursor, const BYTE* end, std::uint32_t& value) {
        if ((size_t)(end - cursor) < sizeof(value)) return false;
        std::memcpy(&value, cursor, sizeof(value));
        cursor += sizeof(value);
        return true;
    }

    static bool ReadBundleU64(const BYTE*& cursor, const BYTE* end, std::uint64_t& value) {
        if ((size_t)(end - cursor) < sizeof(value)) return false;
        std::memcpy(&value, cursor, sizeof(value));
        cursor += sizeof(value);
        return true;
    }

    static bool BundlePathToWide(const BYTE* pathBytes, size_t pathLength, std::wstring& path) {
        if (!pathBytes || pathLength == 0 || pathLength > 32768) return false;

        std::string value(reinterpret_cast<const char*>(pathBytes), pathLength);
        if (value.empty() || value.front() == '/' || value.front() == '\\' || value.find(':') != std::string::npos) {
            return false;
        }

        std::string normalized;
        size_t componentStart = 0;
        while (componentStart <= value.size()) {
            size_t componentEnd = componentStart;
            while (componentEnd < value.size() && value[componentEnd] != '/' && value[componentEnd] != '\\') {
                ++componentEnd;
            }

            if (componentEnd == componentStart) return false;
            std::string component = value.substr(componentStart, componentEnd - componentStart);
            if (component == "." || component == "..") return false;
            for (unsigned char ch : component) {
                if (ch < 32 || ch == '<' || ch == '>' || ch == '"' || ch == '|' ||
                    ch == '?' || ch == '*') {
                    return false;
                }
            }

            if (!normalized.empty()) normalized.push_back('/');
            normalized += component;
            if (componentEnd == value.size()) break;
            componentStart = componentEnd + 1;
        }

        path = Utf8ToWide(normalized);
        return !path.empty();
    }

    static bool ExtractRuntimeBundle(
        const void* bundleBytes,
        DWORD bundleSize,
        const std::wstring& runtimeDir) {
        if (!bundleBytes || bundleSize < 24 || runtimeDir.empty()) {
            ShowError(L"Embedded muted utilities bundle is invalid.");
            return false;
        }

        const BYTE* cursor = static_cast<const BYTE*>(bundleBytes);
        const BYTE* end = cursor + bundleSize;
        const char expectedMagic[] = "TEBNDL01";
        if (std::memcmp(cursor, expectedMagic, sizeof(expectedMagic) - 1) != 0) {
            ShowError(L"Embedded muted utilities bundle has an invalid format.");
            return false;
        }
        cursor += sizeof(expectedMagic) - 1;

        std::uint32_t formatVersion = 0;
        std::uint32_t compressionAlgorithm = 0;
        std::uint32_t fileCount = 0;
        std::uint32_t reserved = 0;
        if (!ReadBundleU32(cursor, end, formatVersion) ||
            !ReadBundleU32(cursor, end, compressionAlgorithm) ||
            !ReadBundleU32(cursor, end, fileCount) ||
            !ReadBundleU32(cursor, end, reserved) ||
            formatVersion != 1 ||
            compressionAlgorithm != 1 ||
            fileCount > 100000) {
            ShowError(L"Embedded muted utilities bundle has an unsupported format.");
            return false;
        }

        DECOMPRESSOR_HANDLE decompressor = nullptr;
        bool decompressorCreated = false;
        for (std::uint32_t index = 0; index < fileCount; ++index) {
            std::uint32_t pathLength = 0;
            std::uint32_t compressionType = 0;
            std::uint64_t uncompressedSize = 0;
            std::uint64_t payloadSize = 0;
            if (!ReadBundleU32(cursor, end, pathLength) ||
                !ReadBundleU32(cursor, end, compressionType) ||
                !ReadBundleU64(cursor, end, uncompressedSize) ||
                !ReadBundleU64(cursor, end, payloadSize) ||
                pathLength > 32768 ||
                payloadSize > static_cast<std::uint64_t>(end - cursor) ||
                uncompressedSize > static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())) {
                if (decompressorCreated) CloseDecompressor(decompressor);
                ShowError(L"Embedded muted utilities bundle contains an invalid file entry.");
                return false;
            }

            const BYTE* pathBytes = cursor;
            cursor += pathLength;
            if (payloadSize > static_cast<std::uint64_t>(end - cursor)) {
                if (decompressorCreated) CloseDecompressor(decompressor);
                ShowError(L"Embedded muted utilities bundle contains truncated data.");
                return false;
            }

            std::wstring relativePath;
            if (!BundlePathToWide(pathBytes, pathLength, relativePath)) {
                if (decompressorCreated) CloseDecompressor(decompressor);
                ShowError(L"Embedded muted utilities bundle contains an unsafe path.");
                return false;
            }

            const BYTE* payload = cursor;
            cursor += static_cast<size_t>(payloadSize);
            std::vector<BYTE> decompressed;
            const void* outputBytes = payload;
            size_t outputSize = static_cast<size_t>(uncompressedSize);

            if (compressionType == 1) {
                if (!decompressorCreated) {
                    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, nullptr, &decompressor)) {
                        ShowError(L"Windows could not create the muted utilities decompressor.");
                        return false;
                    }
                    decompressorCreated = true;
                }

                decompressed.resize(outputSize);
                SIZE_T actualSize = 0;
                if (!Decompress(
                    decompressor,
                    payload,
                    static_cast<SIZE_T>(payloadSize),
                    decompressed.empty() ? nullptr : decompressed.data(),
                    decompressed.size(),
                    &actualSize) || actualSize != decompressed.size()) {
                    CloseDecompressor(decompressor);
                    ShowError(L"Windows could not unpack the muted utilities bundle.");
                    return false;
                }
                outputBytes = decompressed.empty() ? nullptr : decompressed.data();
            } else if (compressionType != 0 || payloadSize != uncompressedSize) {
                if (decompressorCreated) CloseDecompressor(decompressor);
                ShowError(L"Embedded muted utilities bundle contains an invalid compression entry.");
                return false;
            }

            std::wstring targetPath = runtimeDir + L"\\" + relativePath;
            size_t slash = targetPath.find_last_of(L"\\/");
            if (slash == std::wstring::npos || !EnsureDirectoryTree(targetPath.substr(0, slash)) ||
                !WriteBytesToFile(targetPath, outputBytes, outputSize, L"muted utilities runtime")) {
                if (decompressorCreated) CloseDecompressor(decompressor);
                return false;
            }
        }

        if (decompressorCreated) CloseDecompressor(decompressor);
        return cursor == end;
    }

    static bool GetEmbeddedRuntimeVersion(std::string& version) {
        version.clear();
        const void* bytes = nullptr;
        DWORD size = 0;
        if (!LoadResourceBytes(
            IDR_TAGESSENTIALS_RUNTIME_VERSION,
            bytes,
            size,
            L"muted utilities runtime version",
            true)) {
            return false;
        }

        const char* begin = static_cast<const char*>(bytes);
        const char* end = begin + size;
        while (begin < end && (*begin == ' ' || *begin == '\t' || *begin == '\r' || *begin == '\n')) ++begin;
        while (end > begin && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) --end;
        if (begin == end || end - begin > 80) {
            ShowError(L"Embedded muted utilities runtime version is invalid.");
            return false;
        }

        for (const char* cursor = begin; cursor < end; ++cursor) {
            if (!((*cursor >= '0' && *cursor <= '9') ||
                (*cursor >= 'a' && *cursor <= 'f') ||
                (*cursor >= 'A' && *cursor <= 'F'))) {
                ShowError(L"Embedded muted utilities runtime version is invalid.");
                return false;
            }
        }

        version.assign(begin, end);
        return true;
    }

    static bool StageEmbeddedPublicHelpers(const std::wstring& runtimeDir) {
        if (!HasResource(IDR_TAGESSENTIALS_PUBLIC_HELPERS)) return true;

        std::wstring configPath = runtimeDir + L"\\public_helpers_server.txt";
        if (FileExists(configPath)) return true;

        const void* bytes = nullptr;
        DWORD size = 0;
        if (!LoadResourceBytes(
            IDR_TAGESSENTIALS_PUBLIC_HELPERS,
            bytes,
            size,
            L"Public Helpers configuration",
            true)) {
            return false;
        }
        return WriteBytesToFile(configPath, bytes, size, L"Public Helpers configuration");
    }

    static bool StageEmbeddedRuntime(
        const std::wstring& stageDir,
        std::wstring& runtimeDir) {
        runtimeDir = GetExecutableDirectory();
        if (!HasResource(IDR_TAGESSENTIALS_RUNTIME)) return true;

        std::string version;
        if (!GetEmbeddedRuntimeVersion(version)) return false;
        std::wstring versionDirectory = Utf8ToWide(version);
        if (versionDirectory.empty()) {
            ShowError(L"Embedded muted utilities runtime version could not be decoded.");
            return false;
        }

        std::wstring root = GetEnvironmentValue(L"LOCALAPPDATA");
        if (root.empty()) root = stageDir;
        runtimeDir = root + L"\\TagEssentials\\MutedVoiceRuntime\\" + versionDirectory;
        if (!EnsureDirectoryTree(runtimeDir)) {
            ShowError(L"Failed to create the muted utilities runtime directory.");
            return false;
        }

        std::wstring markerPath = runtimeDir + L"\\.tagessentials-runtime-complete";
        bool ready = FileExists(markerPath) &&
            FileExists(runtimeDir + L"\\node.exe") &&
            FileExists(runtimeDir + L"\\mutedVoiceBot.js") &&
            FileExists(runtimeDir + L"\\node_modules\\mineflayer\\package.json");
        if (!ready) {
            const void* bundleBytes = nullptr;
            DWORD bundleSize = 0;
            if (!LoadResourceBytes(
                IDR_TAGESSENTIALS_RUNTIME,
                bundleBytes,
                bundleSize,
                L"muted utilities bundle",
                true) ||
                !ExtractRuntimeBundle(bundleBytes, bundleSize, runtimeDir)) {
                return false;
            }

            const char marker[] = "TagEssentials muted utilities runtime\n";
            if (!WriteBytesToFile(markerPath, marker, sizeof(marker) - 1, L"muted utilities runtime marker")) {
                return false;
            }
        }

        return StageEmbeddedPublicHelpers(runtimeDir);
    }

    static bool CreateStagedDllFromResource(std::wstring& stagedPath) {
        std::wstring stageDir = GetStageDirectory();
        if (stageDir.empty()) {
            ShowError(L"Failed to resolve a temp directory for staged injection.");
            return false;
        }

        if (!EnsureDirectoryTree(stageDir)) {
            ShowError(L"Failed to create the staged DLL directory.");
            return false;
        }

        CleanupOldStagedCopies(stageDir);
        std::wstring runtimeDir;
        if (!StageEmbeddedRuntime(stageDir, runtimeDir)) return false;
        WriteMutedVoiceRuntimeDirectory(stageDir, runtimeDir);
        StageMutedVoiceScriptFallback(stageDir);

        SYSTEMTIME st = {};
        GetLocalTime(&st);
        DWORD tick = static_cast<DWORD>(GetTickCount64() % 100000);

        wchar_t fileName[128] = {};
        swprintf_s(fileName, L"TagEssentialsMod_%04u%02u%02u_%02u%02u%02u_%05u.dll",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            tick % 100000);

        stagedPath = stageDir + L"\\" + fileName;

        const void* resourceBytes = nullptr;
        DWORD resourceSize = 0;
        if (!LoadEmbeddedDllResource(resourceBytes, resourceSize) ||
            !WriteBytesToFile(stagedPath, resourceBytes, resourceSize, L"DLL")) {
            stagedPath.clear();
            return false;
        }

        return true;
    }

    static std::wstring ToLower(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(),
            [](wchar_t ch) { return (wchar_t)towlower(ch); });
        return value;
    }

    static std::wstring GetProcessImagePath(DWORD pid) {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) return L"";

        std::wstring path;
        DWORD size = 32768;
        path.resize(size);
        LPWSTR buffer = path.empty() ? nullptr : &path[0];
        if (buffer && QueryFullProcessImageNameW(process, 0, buffer, &size)) {
            path.resize(size);
        }
        else {
            path.clear();
        }

        CloseHandle(process);
        return path;
    }

    static int ScoreMinecraftProcess(const PROCESSENTRY32& pe, const std::wstring& imagePath) {
        if (_wcsicmp(pe.szExeFile, L"javaw.exe") != 0) return -100000;

        std::wstring lowerPath = ToLower(imagePath);
        if (lowerPath.find(L"\\.lunarclient\\jre\\") != std::wstring::npos) return 1100;
        if (lowerPath.find(L"\\.lunarclient\\") != std::wstring::npos) return 1050;
        if (lowerPath.find(L"badlion client\\data\\jdk") != std::wstring::npos) return 1000;
        if (lowerPath.find(L"badlion client") != std::wstring::npos) return 800;
        if (lowerPath.find(L"\\program files\\java\\") != std::wstring::npos) return -100;
        if (lowerPath.find(L"\\javaw.exe") != std::wstring::npos) return 10;
        return 0;
    }

    static DWORD FindMinecraftProcess() {
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32 pe = { sizeof(PROCESSENTRY32) };
        DWORD bestPid = 0;
        int bestScore = -100000;

        if (Process32First(hSnap, &pe)) {
            do {
                std::wstring imagePath = GetProcessImagePath(pe.th32ProcessID);
                int score = ScoreMinecraftProcess(pe, imagePath);
                if (score > bestScore) {
                    bestScore = score;
                    bestPid = pe.th32ProcessID;
                }
            } while (Process32Next(hSnap, &pe));
        }

        CloseHandle(hSnap);
        return bestScore >= 800 ? bestPid : 0;
    }

    static InjectionResult PerformInjection(HANDLE hProcess, const std::wstring& dllPath) {
        size_t pathSize = (dllPath.length() + 1) * sizeof(wchar_t);
        LPVOID remoteMem = VirtualAllocEx(hProcess, NULL, pathSize,
            MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        if (!remoteMem) {
            ShowError(L"Failed to allocate memory in target process");
            return InjectionResult::Failed;
        }

        if (!WriteProcessMemory(hProcess, remoteMem, dllPath.c_str(), pathSize, NULL)) {
            ShowError(L"Failed to write DLL path to target process");
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            return InjectionResult::Failed;
        }

        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        LPVOID loadLibAddr = kernel32 ? GetProcAddress(kernel32, "LoadLibraryW") : nullptr;
        if (!loadLibAddr) {
            VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
            ShowError(L"Failed to resolve LoadLibraryW.");
            return InjectionResult::Failed;
        }

        HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
            (LPTHREAD_START_ROUTINE)loadLibAddr,
            remoteMem, 0, NULL);

        bool success = false;
        if (hThread) {
            DWORD waitResult = WaitForSingleObject(hThread, 30000);
            if (waitResult != WAIT_OBJECT_0) {
                CloseHandle(hThread);
                // Never free memory while the remote loader may still read it.
                // The target process will reclaim it on exit.
                ShowError(L"DLL loading has not been confirmed complete. It may still finish.\n"
                    L"Do not launch TagEssentials again until Minecraft has been restarted.");
                return InjectionResult::Pending;
            }

            DWORD exitCode = 0;
            success = GetExitCodeThread(hThread, &exitCode) && exitCode != 0;

            CloseHandle(hThread);
        }

        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);

        if (!success) {
            ShowError(L"Injection failed. TagEssentials may already be loaded.\n"
                L"If its window is not open, restart Minecraft and try again.");
        }

        return success ? InjectionResult::Succeeded : InjectionResult::Failed;
    }

    static void ShowError(const std::wstring& message) {
        MessageBoxW(nullptr, message.c_str(), L"TagEssentials",
            MB_OK | MB_ICONERROR);
    }
};

int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int) {
    TagEssentialsLauncher::Inject();
    return 0;
}
