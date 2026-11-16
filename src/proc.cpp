// Simulacrum - running AliceVision's programs (see proc.h)
#include "proc.h"

#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace Proc {

#ifdef _WIN32
static std::wstring wide(const std::string& s) { return fs::u8path(s).wstring(); }

static std::string exeDir() {
    wchar_t b[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, b, (DWORD)(sizeof b / sizeof b[0]));
    return fs::path(std::wstring(b, n)).parent_path().u8string();
}

std::string runtimeDir() {
    if (const wchar_t* e = _wgetenv(L"SIMULACRUM_RUNTIME")) if (*e) return fs::path(e).u8string();   // another AliceVision folder
    // installed: <Simulacrum>\alicevision; while developing, the unpacked runtime next to the build
    for (const fs::path& p : {fs::u8path(exeDir()) / "alicevision", fs::u8path(exeDir()) / ".." / ".." / "runtime" / "alicevision"}) {
        std::error_code ec;
        if (fs::exists(p / "bin" / "aliceVision_cameraInit.exe", ec)) return fs::weakly_canonical(p, ec).u8string();
    }
    return {};
}

void setupEnvironment() {
    const std::string rt = runtimeDir();
    if (rt.empty()) return;
    const fs::path r = fs::u8path(rt);
    auto set = [](const wchar_t* k, const fs::path& v) { SetEnvironmentVariableW(k, v.wstring().c_str()); };
    set(L"ALICEVISION_ROOT", r);
    set(L"ALICEVISION_SENSOR_DB", r / "share" / "aliceVision" / "cameraSensors.db");
    set(L"ALICEVISION_VOCTREE", r / "share" / "aliceVision" / "vlfeat_K80L3.SIFT.tree");
    set(L"ALICEVISION_OCIO", r / "share" / "aliceVision" / "config.ocio");
    wchar_t old[32768];
    const DWORD n = GetEnvironmentVariableW(L"PATH", old, 32768);
    std::wstring path = (r / "bin").wstring() + L";" + (r / "lib").wstring() + L";" + std::wstring(old, n);
    SetEnvironmentVariableW(L"PATH", path.c_str());
}

static std::wstring quote(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
    std::wstring q = L"\"";
    int bs = 0;
    for (wchar_t c : a) {
        if (c == L'\\') { ++bs; continue; }
        if (c == L'"') { q.append(bs * 2 + 1, L'\\'); q += c; bs = 0; continue; }
        q.append(bs, L'\\'); bs = 0; q += c;
    }
    q.append(bs * 2, L'\\');
    return q + L"\"";
}

int run(const std::string& exe, const std::vector<std::string>& args, const std::string& workDir,
        const std::function<void(const std::string&)>& onLine, std::atomic<bool>& stop) {
    const std::string rt = runtimeDir();
    if (rt.empty()) { onLine("The AliceVision runtime is missing: please reinstall Simulacrum."); return -1; }
    const std::wstring path = (fs::u8path(rt) / "bin" / (exe + ".exe")).wstring();
    std::wstring cmd = quote(path);
    for (const auto& a : args) cmd += L" " + quote(wide(a));

    SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{}; si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cl(cmd.begin(), cmd.end()); cl.push_back(0);
    const std::wstring wd = wide(workDir);
    // its own job object: if Simulacrum goes, the program goes with it
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
    li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (job) SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
    const BOOL ok = CreateProcessW(path.c_str(), cl.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED | BELOW_NORMAL_PRIORITY_CLASS,
                                   nullptr, wd.empty() ? nullptr : wd.c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); if (job) CloseHandle(job); onLine("Couldn't start " + exe); return -1; }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    std::string buf;
    char chunk[4096];
    bool stopped = false;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr)) break;   // the pipe closed: it finished
        if (avail == 0) {
            if (stop) { TerminateJobObject(job, 1); stopped = true; break; }
            if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) {
                if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) break;
            }
            continue;
        }
        DWORD got = 0;
        if (!ReadFile(rd, chunk, std::min<DWORD>(avail, sizeof chunk), &got, nullptr) || got == 0) break;
        buf.append(chunk, got);
        size_t nl;
        while ((nl = buf.find_first_of("\r\n")) != std::string::npos) {
            std::string line = buf.substr(0, nl);
            buf.erase(0, nl + 1);
            if (!line.empty()) onLine(line);
        }
    }
    if (!buf.empty()) onLine(buf);
    WaitForSingleObject(pi.hProcess, stopped ? 5000 : INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread); CloseHandle(rd);
    if (job) CloseHandle(job);
    return stopped ? -2 : (int)code;
}

bool hasCuda(std::string& name) {
    // the NVIDIA driver's CUDA library, asked directly: no CUDA toolkit needed
    HMODULE cu = LoadLibraryW(L"nvcuda.dll");
    if (!cu) return false;
    using InitFn = int(__stdcall*)(unsigned);
    using CountFn = int(__stdcall*)(int*);
    using GetFn = int(__stdcall*)(int*, int);
    using NameFn = int(__stdcall*)(char*, int, int);
    using MemFn = int(__stdcall*)(size_t*, int);
    using AttrFn = int(__stdcall*)(int*, int, int);
    auto init = (InitFn)GetProcAddress(cu, "cuInit");
    auto count = (CountFn)GetProcAddress(cu, "cuDeviceGetCount");
    auto get = (GetFn)GetProcAddress(cu, "cuDeviceGet");
    auto nm = (NameFn)GetProcAddress(cu, "cuDeviceGetName");
    auto mem = (MemFn)GetProcAddress(cu, "cuDeviceTotalMem_v2");
    auto attr = (AttrFn)GetProcAddress(cu, "cuDeviceGetAttribute");
    int n = 0, dev = 0;
    if (!init || !count || !get || init(0) != 0 || count(&n) != 0 || n < 1 || get(&dev, 0) != 0) return false;
    char b[256] = {};
    if (nm) nm(b, sizeof b, dev);
    size_t bytes = 0;
    if (mem) mem(&bytes, dev);
    int major = 0;
    if (attr) attr(&major, 75, dev);   // CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR
    name = std::string(b) + " (" + std::to_string((int)(bytes / (1024.0 * 1024 * 1024) + 0.5)) + " GB)";
    return major >= 5;                  // AliceVision's CUDA build wants Maxwell or newer
}

std::string projectsDir() {
    if (const wchar_t* e = _wgetenv(L"SIMULACRUM_PROJECTS")) if (*e) return fs::path(e).u8string();
    // Documents\Simulacrum, unless Windows guards Documents (Controlled folder access): then Simulacrum in the user's own
    // folder. Tried once, by really writing a file.
    static std::string dir;
    if (!dir.empty()) return dir;
    auto canWrite = [](const fs::path& d) {
        std::error_code ec;
        fs::create_directories(d, ec);
        const fs::path t = d / ".simulacrum-write-test";
        FILE* f = _wfopen(t.wstring().c_str(), L"wb");
        if (!f) return false;
        fclose(f); fs::remove(t, ec);
        return true;
    };
    for (const KNOWNFOLDERID& id : {FOLDERID_Documents, FOLDERID_Profile}) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p))) {
            const fs::path d = fs::path(p) / "Simulacrum"; CoTaskMemFree(p);
            if (canWrite(d)) return dir = d.u8string();
        }
    }
    return dir;
}
#else
std::string runtimeDir() { return {}; }
void setupEnvironment() {}
int run(const std::string&, const std::vector<std::string>&, const std::string&, const std::function<void(const std::string&)>& onLine, std::atomic<bool>&) {
    onLine("Simulacrum runs on Windows for now"); return -1;
}
bool hasCuda(std::string&) { return false; }
std::string projectsDir() { return {}; }
#endif

}
