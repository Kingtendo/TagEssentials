#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <string>

namespace {

std::wstring ToLower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(towlower(ch));
    });
    return value;
}

std::wstring ProcessPath(DWORD processId) {
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return L"";
    std::wstring path(32768, L'\0');
    DWORD length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process, 0, path.data(), &length)) path.clear();
    else path.resize(length);
    CloseHandle(process);
    return path;
}

DWORD FindLunarJavaProcess() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD newestProcessId = 0;
    ULONGLONG newestStart = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"javaw.exe") != 0) continue;
            const std::wstring path = ToLower(ProcessPath(entry.th32ProcessID));
            if (path.find(L"\\.lunarclient\\jre\\") == std::wstring::npos) continue;

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            FILETIME created = {}, exited = {}, kernel = {}, user = {};
            ULONGLONG createdValue = 0;
            if (process && GetProcessTimes(process, &created, &exited, &kernel, &user)) {
                createdValue = (static_cast<ULONGLONG>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
            }
            if (process) CloseHandle(process);
            if (!newestProcessId || createdValue >= newestStart) {
                newestStart = createdValue;
                newestProcessId = entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return newestProcessId;
}

std::wstring ExecutableDirectory() {
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) return L"";
    path.resize(length);
    const size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return L"";
    path.resize(slash);
    return path;
}

bool Inject(DWORD processId, const std::wstring& dllPath) {
    const std::wstring imagePath = ProcessPath(processId);
    const std::wstring lowerPath = ToLower(imagePath);
    if (lowerPath.find(L"\\.lunarclient\\jre\\") == std::wstring::npos ||
        lowerPath.rfind(L"\\javaw.exe") != lowerPath.size() - 10) {
        std::wcerr << L"Refusing target: PID " << processId << L" is not Lunar's javaw.exe.\n";
        return false;
    }

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
            PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        processId);
    if (!process) {
        std::wcerr << L"OpenProcess failed with Windows error " << GetLastError()
                   << L". Run this utility as administrator.\n";
        return false;
    }

    const SIZE_T pathBytes = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remotePath = VirtualAllocEx(process, nullptr, pathBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remotePath) {
        std::wcerr << L"VirtualAllocEx failed with Windows error " << GetLastError() << L".\n";
        CloseHandle(process);
        return false;
    }

    bool success = false;
    bool loaderFinished = true;
    if (WriteProcessMemory(process, remotePath, dllPath.c_str(), pathBytes, nullptr)) {
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW"));
        HANDLE thread = loadLibrary
            ? CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr)
            : nullptr;
        if (thread) {
            const DWORD waitResult = WaitForSingleObject(thread, 15000);
            loaderFinished = waitResult == WAIT_OBJECT_0;
            DWORD exitCode = 0;
            if (waitResult == WAIT_OBJECT_0 && GetExitCodeThread(thread, &exitCode) && exitCode != 0) {
                success = true;
            }
            else {
                std::wcerr << L"Remote LoadLibraryW failed or timed out.\n";
            }
            CloseHandle(thread);
        }
        else {
            std::wcerr << L"CreateRemoteThread failed with Windows error " << GetLastError() << L".\n";
        }
    }
    else {
        std::wcerr << L"WriteProcessMemory failed with Windows error " << GetLastError() << L".\n";
    }

    // On timeout or wait failure the loader may still read this path.
    // The target process reclaims the allocation when it exits.
    if (loaderFinished) VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);
    if (success) {
        std::wcout << L"DLL loaded into Lunar PID " << processId << L":\n  " << dllPath << L"\n";
    }
    return success;
}

struct CloseGuiContext {
    DWORD processId = 0;
    unsigned int closed = 0;
};

BOOL CALLBACK CloseModGuiWindow(HWND window, LPARAM parameter) {
    CloseGuiContext* context = reinterpret_cast<CloseGuiContext*>(parameter);
    if (!context) return TRUE;

    DWORD windowProcessId = 0;
    GetWindowThreadProcessId(window, &windowProcessId);
    if (windowProcessId != context->processId) return TRUE;

    wchar_t className[128] = {};
    wchar_t title[256] = {};
    GetClassNameW(window, className, static_cast<int>(std::size(className)));
    GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    if (wcscmp(className, L"TagEssentialsGuiWnd") != 0 && wcscmp(title, L"TagEssentials") != 0) return TRUE;

    if (PostMessageW(window, WM_CLOSE, 0, 0)) ++context->closed;
    return TRUE;
}

bool RequestModUnload(DWORD processId) {
    CloseGuiContext context = {};
    context.processId = processId;
    EnumWindows(CloseModGuiWindow, reinterpret_cast<LPARAM>(&context));
    if (!context.closed) {
        std::wcerr << L"No TagEssentials window was found for Lunar PID " << processId << L".\n";
        return false;
    }
    std::wcout << L"Requested a clean mod unload from Lunar PID " << processId << L".\n";
    return true;
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    DWORD processId = 0;
    const bool closeGui = argc >= 2 && _wcsicmp(argv[1], L"--close-gui") == 0;
    if (closeGui) {
        if (argc >= 3) processId = wcstoul(argv[2], nullptr, 10);
    }
    else if (argc >= 2) {
        processId = wcstoul(argv[1], nullptr, 10);
    }
    if (!processId) processId = FindLunarJavaProcess();
    if (!processId) {
        std::wcerr << L"No running Lunar javaw.exe was found.\n";
        return 1;
    }

    if (closeGui) return RequestModUnload(processId) ? 0 : 4;

    std::wstring dllPath = ExecutableDirectory() + L"\\LunarClassDumper.dll";
    if (argc >= 3 && argv[2] && *argv[2]) {
        std::wstring requestedPath(32768, L'\0');
        const DWORD length = GetFullPathNameW(argv[2], static_cast<DWORD>(requestedPath.size()), requestedPath.data(), nullptr);
        if (length == 0 || length >= requestedPath.size()) {
            std::wcerr << L"Invalid DLL path: " << argv[2] << L"\n";
            return 2;
        }
        requestedPath.resize(length);
        dllPath = requestedPath;
    }
    if (GetFileAttributesW(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::wcerr << L"DLL not found: " << dllPath << L"\n";
        return 2;
    }

    return Inject(processId, dllPath) ? 0 : 3;
}
