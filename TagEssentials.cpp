#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <iostream>
#include <string>
#include <algorithm>
#include <cwctype>
#include "resource.h"

#pragma comment(lib, "kernel32.lib")

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
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], required, nullptr, nullptr);
        return result;
    }

    static void WriteMutedVoiceRuntimeDirectory(const std::wstring& stageDir) {
        std::wstring exeDir = GetExecutableDirectory();
        if (exeDir.empty()) return;

        std::string payload = WideToUtf8(exeDir);
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
        HMODULE moduleHandle = GetModuleHandleW(nullptr);
        if (!moduleHandle) {
            ShowError(L"Failed to resolve the TagEssentials launcher module handle.");
            return false;
        }

        HRSRC resourceHandle = FindResourceW(
            moduleHandle,
            MAKEINTRESOURCEW(IDR_TAGESSENTIALS_MOD),
            RT_RCDATA);
        if (!resourceHandle) {
            ShowError(L"Embedded DLL resource not found.");
            return false;
        }

        resourceSize = SizeofResource(moduleHandle, resourceHandle);
        if (resourceSize == 0) {
            ShowError(L"Embedded DLL resource is empty.");
            return false;
        }

        HGLOBAL loadedResource = LoadResource(moduleHandle, resourceHandle);
        if (!loadedResource) {
            ShowError(L"Failed to load the embedded DLL resource.");
            return false;
        }

        resourceBytes = LockResource(loadedResource);
        if (!resourceBytes) {
            ShowError(L"Failed to lock the embedded DLL resource.");
            return false;
        }

        return true;
    }

    static bool WriteBytesToFile(const std::wstring& path, const void* bytes, DWORD byteCount) {
        HANDLE fileHandle = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (fileHandle == INVALID_HANDLE_VALUE) {
            ShowError(L"Failed to create the staged DLL file.");
            return false;
        }

        DWORD bytesWritten = 0;
        BOOL writeOk = WriteFile(fileHandle, bytes, byteCount, &bytesWritten, nullptr);
        CloseHandle(fileHandle);

        if (!writeOk || bytesWritten != byteCount) {
            DeleteFileW(path.c_str());
            ShowError(L"Failed to write the embedded DLL to the staging directory.");
            return false;
        }

        return true;
    }

    static bool CreateStagedDllFromResource(std::wstring& stagedPath) {
        std::wstring stageDir = GetStageDirectory();
        if (stageDir.empty()) {
            ShowError(L"Failed to resolve a temp directory for staged injection.");
            return false;
        }

        if (!CreateDirectoryW(stageDir.c_str(), nullptr)) {
            DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                ShowError(L"Failed to create the staged DLL directory.");
                return false;
            }
        }

        CleanupOldStagedCopies(stageDir);
        WriteMutedVoiceRuntimeDirectory(stageDir);
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
            !WriteBytesToFile(stagedPath, resourceBytes, resourceSize)) {
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
