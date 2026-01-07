#include "WindowsAppManager.hpp"
#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace platform {
namespace windows_os {

    WindowsAppManager::WindowsAppManager() {}

    std::vector<interfaces::AppEntry> WindowsAppManager::list_applications(bool only_running) {
        if (!only_running) {
            if (installed_apps_.empty()) refresh_installed_apps();
            return installed_apps_;
        }

        std::vector<interfaces::AppEntry> procs;
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE) return procs;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(hSnap, &pe32)) {
            do {
                interfaces::AppEntry app;
                #ifdef UNICODE
                char buff[MAX_PATH];
                WideCharToMultiByte(CP_UTF8, 0, pe32.szExeFile, -1, buff, MAX_PATH, NULL, NULL);
                app.name = std::string(buff);
                #else
                app.name = std::string(pe32.szExeFile);
                #endif

                app.pid = pe32.th32ProcessID;
                app.id = std::to_string(app.pid);
                app.exec = app.name;
                app.cpu = 0.0; // CPU requires time-based sampling, set to 0 for now
                app.memory_kb = 0;

                // Get memory info
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, app.pid);
                if (hProc) {
                    PROCESS_MEMORY_COUNTERS pmc;
                    if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) {
                        app.memory_kb = pmc.WorkingSetSize / 1024;
                    }
                    CloseHandle(hProc);
                }

                procs.push_back(app);

            } while (Process32Next(hSnap, &pe32));
        }
        CloseHandle(hSnap);
        return procs;
    }

    common::Result<uint32_t> WindowsAppManager::launch_app(const std::string& command) {
        SHELLEXECUTEINFOA sei = {0};
        sei.cbSize = sizeof(SHELLEXECUTEINFOA);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = "open";
        sei.nShow = SW_SHOW;

        std::string prog = command;
        std::string params = "";

        // Simple splitting logic if space exists (naive)
        // If start menu entry, 'command' is full path, no params usually.
        size_t firstSpace = command.find(' ');
        // Logic: Check if 'command' is an existing file. If so, treat whole as program.
        // Otherwise split.
        bool isFile = false;
        try { isFile = fs::exists(command); } catch(...) {}

        if (!isFile && firstSpace != std::string::npos) {
            prog = command.substr(0, firstSpace);
            params = command.substr(firstSpace + 1);
        }

        sei.lpFile = prog.c_str();
        if(!params.empty()) sei.lpParameters = params.c_str();

        if (ShellExecuteExA(&sei)) {
            uint32_t pid = 0;
            if(sei.hProcess) {
                pid = GetProcessId(sei.hProcess);
                CloseHandle(sei.hProcess);
            }
            return common::Result<uint32_t>::ok(pid);
        } else {
            return common::Result<uint32_t>::err(common::ErrorCode::Unknown, "ShellExecuteEx failed");
        }
    }

    common::EmptyResult WindowsAppManager::kill_process(uint32_t pid) {
        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProc) {
            TerminateProcess(hProc, 9);
            CloseHandle(hProc);
            return common::Result<common::Ok>::success();
        }
        return common::Result<common::Ok>::success(); // Ignore failure
    }

    common::EmptyResult WindowsAppManager::shutdown_system() {
        system("shutdown /s /t 0");
        return common::Result<common::Ok>::success();
    }

    common::EmptyResult WindowsAppManager::restart_system() {
        system("shutdown /r /t 0");
        return common::Result<common::Ok>::success();
    }

    std::string to_lower(const std::string& s) {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(), ::tolower);
        return out;
    }

    std::vector<interfaces::AppEntry> WindowsAppManager::search_apps(const std::string& query) {
        if (installed_apps_.empty()) {
            refresh_installed_apps();
        }

        std::vector<interfaces::AppEntry> results;
        std::string q = to_lower(query);

        for (const auto& app : installed_apps_) {
            if (to_lower(app.name).find(q) != std::string::npos) {
                results.push_back(app);
            }
        }
        return results;
    }

    // COM Helper
    struct ComInit {
        ComInit() { CoInitialize(NULL); }
        ~ComInit() { CoUninitialize(); }
    };

    void WindowsAppManager::refresh_installed_apps() {
        installed_apps_.clear();
        ComInit comContext;

        IShellLinkW* psl;
        HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
        if (FAILED(hres)) return;

        IPersistFile* ppf;
        hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf);
        if (FAILED(hres)) { psl->Release(); return; }

        std::vector<std::string> roots;
        char buf[MAX_PATH];
        if (GetEnvironmentVariableA("ProgramData", buf, MAX_PATH))
            roots.push_back(std::string(buf) + "\\Microsoft\\Windows\\Start Menu\\Programs");
        if (GetEnvironmentVariableA("APPDATA", buf, MAX_PATH))
            roots.push_back(std::string(buf) + "\\Microsoft\\Windows\\Start Menu\\Programs");

        for (const auto& rootStr : roots) {
            if (!fs::exists(rootStr)) continue;

            try {
                for (const auto& entry : fs::recursive_directory_iterator(rootStr)) {
                    if (entry.path().extension() == ".lnk") {
                        WCHAR wPath[MAX_PATH];
                        MultiByteToWideChar(CP_UTF8, 0, entry.path().string().c_str(), -1, wPath, MAX_PATH);

                        if (SUCCEEDED(ppf->Load(wPath, STGM_READ))) {
                            if (SUCCEEDED(psl->Resolve(NULL, SLR_NO_UI | SLR_NOUPDATE))) {
                                WCHAR szGotPath[MAX_PATH];
                                WIN32_FIND_DATAW wfd;
                                psl->GetPath(szGotPath, MAX_PATH, &wfd, SLGP_SHORTPATH);

                                interfaces::AppEntry app;
                                app.name = entry.path().stem().string();

                                char execBuf[MAX_PATH];
                                WideCharToMultiByte(CP_UTF8, 0, szGotPath, -1, execBuf, MAX_PATH, NULL, NULL);
                                app.exec = std::string(execBuf);

                                app.id = app.name;
                                if (!app.exec.empty()) {
                                    installed_apps_.push_back(app);
                                }
                            }
                        }
                    }
                }
            } catch (...) {}
        }

        ppf->Release();
        psl->Release();
    }

}
}
