// Simulacrum - photos to a textured 3D model, on your own PC. Part of Atelier.
// AliceVision's photogrammetry (as Meshroom runs it) in three plain steps: add photos, pick a quality, build; then
// look at the model and save it as GLB, OBJ or STL. Every setting is a slider.
//
//   Simulacrum [project folder or photos...]
//   Simulacrum --capture out.bmp [project]      a screenshot from a hidden window, then quit
//   Simulacrum --make [--quality 0..3] photos   build straight away (with --capture: the picture once it's made)
//   Simulacrum --resume project                 carry a stopped build on
#include "model.h"
#include "pipeline.h"
#include "proc.h"
#include "splash.h"
#include "viewer.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#endif

namespace fs = std::filesystem;

#ifndef SIMULACRUM_VERSION
#define SIMULACRUM_VERSION "1.1.0"
#endif

// ---------- the Atelier look ----------
static const ImU32 C_BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), C_PANEL = IM_COL32(0x24, 0x24, 0x24, 255),
                   C_LINE = IM_COL32(0x3a, 0x3a, 0x3a, 255), C_TEXT = IM_COL32(0xe6, 0xe6, 0xe6, 255),
                   C_DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255), C_PINK = IM_COL32(0x00, 0x7b, 0xb7, 255),
                   C_DEEP = IM_COL32(0x00, 0x51, 0x84, 255), C_WELL = IM_COL32(0x14, 0x14, 0x14, 255);
static ImVec4 col4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

static void applyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 0; s.FrameRounding = 2; s.GrabRounding = 2; s.PopupRounding = 2; s.ScrollbarRounding = 2; s.TabRounding = 2;
    s.WindowBorderSize = 0; s.FrameBorderSize = 0; s.PopupBorderSize = 1;
    s.WindowPadding = ImVec2(14, 12); s.FramePadding = ImVec2(8, 5); s.ItemSpacing = ImVec2(7, 8);
    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = col4(C_TEXT); c[ImGuiCol_TextDisabled] = col4(C_DIM);
    c[ImGuiCol_WindowBg] = col4(C_PANEL); c[ImGuiCol_ChildBg] = col4(C_PANEL); c[ImGuiCol_PopupBg] = col4(IM_COL32(0x21, 0x21, 0x21, 255));
    c[ImGuiCol_Border] = col4(C_LINE); c[ImGuiCol_Separator] = col4(C_LINE);
    c[ImGuiCol_FrameBg] = col4(C_WELL); c[ImGuiCol_FrameBgHovered] = col4(IM_COL32(0x2e, 0x2e, 0x2e, 255)); c[ImGuiCol_FrameBgActive] = col4(IM_COL32(0x33, 0x33, 0x33, 255));
    c[ImGuiCol_MenuBarBg] = col4(C_BG);
    c[ImGuiCol_CheckMark] = col4(C_PINK); c[ImGuiCol_SliderGrab] = col4(C_PINK); c[ImGuiCol_SliderGrabActive] = col4(C_DEEP);
    c[ImGuiCol_Button] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_ButtonHovered] = col4(C_DEEP); c[ImGuiCol_ButtonActive] = col4(C_PINK);
    c[ImGuiCol_Header] = col4(IM_COL32(0x33, 0x33, 0x33, 255)); c[ImGuiCol_HeaderHovered] = col4(C_DEEP); c[ImGuiCol_HeaderActive] = col4(C_PINK);
    c[ImGuiCol_PlotHistogram] = col4(C_PINK);
    c[ImGuiCol_Tab] = col4(C_PANEL); c[ImGuiCol_TabActive] = col4(C_DEEP); c[ImGuiCol_TabHovered] = col4(C_PINK);
    c[ImGuiCol_TabUnfocusedActive] = col4(C_DEEP); c[ImGuiCol_TabUnfocused] = col4(C_PANEL);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.5f);
}

static bool fileExists(const char* f) { if (FILE* t = fopen(f, "rb")) { fclose(t); return true; } return false; }
static ImFont *bigFont = nullptr;

static void loadFont() {
    ImGuiIO& io = ImGui::GetIO();
    const char* f = "C:/Windows/Fonts/segoeui.ttf";
    if (!fileExists(f)) { io.Fonts->AddFontDefault(); return; }
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2013, 0x2026, 0x2190, 0x21FF, 0x2600, 0x27BF, 0};
    auto add = [&](const char* file, float size) {
        ImFont* font = io.Fonts->AddFontFromFileTTF(file, size, nullptr, ranges);
        if (fileExists("C:/Windows/Fonts/seguisym.ttf")) {
            ImFontConfig merge; merge.MergeMode = true;
            static const ImWchar sym[] = {0x2190, 0x21FF, 0x2600, 0x27BF, 0};
            io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/seguisym.ttf", size, &merge, sym);
        }
        return font;
    };
    add(f, 17.0f);
    bigFont = add(fileExists("C:/Windows/Fonts/seguisb.ttf") ? "C:/Windows/Fonts/seguisb.ttf" : f, 21.0f);
    const char* light = fileExists("C:/Windows/Fonts/segoeuil.ttf") ? "C:/Windows/Fonts/segoeuil.ttf" : f;
    Splash::setFonts(add(light, 50.0f), add(f, 13.0f));
}

// ---------- files ----------
static FILE* openU8(const std::string& p, const char* mode) {
#ifdef _WIN32
    return _wfopen(fs::u8path(p).wstring().c_str(), std::wstring(mode, mode + strlen(mode)).c_str());
#else
    return fopen(p.c_str(), mode);
#endif
}

static bool isPhoto(const fs::path& p) {
    std::string e = p.extension().u8string();
    for (auto& c : e) c = (char)tolower((unsigned char)c);
    return e == ".jpg" || e == ".jpeg" || e == ".png" || e == ".tif" || e == ".tiff" || e == ".exr" || e == ".heic" || e == ".dng" || e == ".cr2" || e == ".nef" || e == ".arw";
}

#ifdef _WIN32
static std::vector<std::string> pickPhotos() {
    std::vector<wchar_t> buf(1 << 16, 0);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o;
    o.lpstrFilter = L"Photos\0*.jpg;*.jpeg;*.png;*.tif;*.tiff;*.heic;*.dng;*.cr2;*.nef;*.arw\0All files\0*.*\0\0";
    o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size();
    o.lpstrTitle = L"Add photos (select as many as you like: Ctrl+A picks them all)";
    o.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    std::vector<std::string> out;
    if (!GetOpenFileNameW(&o)) return out;
    const std::wstring first(buf.data());
    const wchar_t* p = buf.data() + first.size() + 1;
    if (!*p) { out.push_back(fs::path(first).u8string()); return out; }
    for (; *p; p += wcslen(p) + 1) out.push_back((fs::path(first) / p).u8string());
    return out;
}

static std::string pickFolder(const wchar_t* title) {
    std::string r;
    IFileOpenDialog* d = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return r;
    DWORD opt = 0; d->GetOptions(&opt); d->SetOptions(opt | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    d->SetTitle(title);
    if (SUCCEEDED(d->Show(nullptr))) {
        IShellItem* it = nullptr;
        if (SUCCEEDED(d->GetResult(&it))) { PWSTR s = nullptr; if (SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &s))) { r = fs::path(s).u8string(); CoTaskMemFree(s); } it->Release(); }
    }
    d->Release();
    return r;
}

static std::string saveDialog(const wchar_t* filter, const wchar_t* ext, const std::string& start) {
    std::vector<wchar_t> buf(4096, 0);
    const std::wstring s = fs::u8path(start).wstring();
    wcsncpy(buf.data(), s.c_str(), buf.size() - 1);
    OPENFILENAMEW o{};
    o.lStructSize = sizeof o; o.lpstrFilter = filter; o.lpstrFile = buf.data(); o.nMaxFile = (DWORD)buf.size(); o.lpstrDefExt = ext;
    o.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&o)) return "";
    return fs::path(buf.data()).u8string();
}

static void openInExplorer(const std::string& p) { ShellExecuteW(nullptr, L"open", fs::u8path(p).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL); }
#else
static std::vector<std::string> pickPhotos() { return {}; }
static std::string pickFolder(const wchar_t*) { return ""; }
static std::string saveDialog(const wchar_t*, const wchar_t*, const std::string&) { return ""; }
static void openInExplorer(const std::string&) {}
#endif

// ---------- state ----------
static SDL_Window* window = nullptr;
static bool quit = false;
static std::string project;                      // the project folder ("" until the first photo comes)
static std::vector<std::string> photos;
static Pipeline::Settings settings;
static bool cuda = false; static std::string gpuName;
static std::string runtimeMissing;

static std::mutex mtx;
static std::deque<std::string> logLines;
static std::vector<std::function<void()>> onMain;
static void post(std::function<void()> f) { std::lock_guard<std::mutex> g(mtx); onMain.push_back(std::move(f)); }
static void logLine(const std::string& s) { std::lock_guard<std::mutex> g(mtx); logLines.push_back(s); if (logLines.size() > 4000) logLines.pop_front(); }

static Pipeline::Progress progress;
static std::thread buildThread;
static std::string buildError, toast; static double toastUntil = 0;
static std::chrono::steady_clock::time_point buildStart;
static double buildSeconds = 0;
static bool done = false;                        // the textured model is there

static Mesh mesh; static Points points; static std::vector<Camera> cameras;
static float exportSize = 1.0f;                  // multiply every position on export
static int forcedView = -1;                      // 0 photos, 1 3D

static double now() { return SDL_GetTicks() / 1000.0; }
static void say(const std::string& s) { toast = s; toastUntil = now() + 6; }

// ---- photo thumbnails, made in the background
struct Thumb { int w = 0, h = 0; std::vector<uint8_t> px; GLuint tex = 0; bool asked = false; };
static std::vector<Thumb> thumbs;
static std::thread thumbThread; static std::atomic<bool> thumbStop{false};

static void makeThumbs() {
    if (thumbThread.joinable()) { thumbStop = true; thumbThread.join(); thumbStop = false; }
    thumbs.assign(photos.size(), Thumb());
    std::vector<std::string> list = photos;
    thumbThread = std::thread([list] {
        for (size_t i = 0; i < list.size() && !thumbStop; ++i) {
            FILE* f = openU8(list[i], "rb");
            if (!f) continue;
            int w, h, n;
            stbi_set_flip_vertically_on_load(0);
            uint8_t* px = stbi_load_from_file(f, &w, &h, &n, 4);
            fclose(f);
            if (!px) continue;
            const int S = 180, tw = w >= h ? S : std::max(1, S * w / h), th = w >= h ? std::max(1, S * h / w) : S;
            std::vector<uint8_t> t((size_t)tw * th * 4);
            for (int y = 0; y < th; ++y)
                for (int x = 0; x < tw; ++x) {
                    // a box average over the source pixels this one covers
                    const int x0 = x * w / tw, x1 = std::max(x0 + 1, (x + 1) * w / tw), y0 = y * h / th, y1 = std::max(y0 + 1, (y + 1) * h / th);
                    unsigned s[4] = {0, 0, 0, 0}, c = 0;
                    for (int yy = y0; yy < y1; yy += 2) for (int xx = x0; xx < x1; xx += 2) { const uint8_t* p = &px[((size_t)yy * w + xx) * 4]; for (int k = 0; k < 4; ++k) s[k] += p[k]; ++c; }
                    for (int k = 0; k < 4; ++k) t[((size_t)y * tw + x) * 4 + k] = (uint8_t)(s[k] / std::max(1u, c));
                }
            stbi_image_free(px);
            post([i, tw, th, t = std::move(t)]() mutable { if (i < thumbs.size()) { thumbs[i].w = tw; thumbs[i].h = th; thumbs[i].px = std::move(t); } });
        }
    });
}

// ---- projects: a folder in Documents\Simulacrum with project.txt (the photos and the sliders)
static void saveProject() {
    if (project.empty()) return;
    std::error_code ec; fs::create_directories(fs::u8path(project), ec);
    std::ofstream o(fs::u8path(project) / "project.txt");
    o << "simulacrum 1\n";
    const Pipeline::Settings& s = settings;
    o << "quality " << s.quality << "\nfeatureDetail " << s.featureDetail << "\nakaze " << s.akaze << "\nmatchNeighbours " << s.matchNeighbours
      << "\ndepthScale " << s.depthScale << "\ndepthMaps " << s.depthMaps << "\nmeshDetail " << s.meshDetail << "\nsmoothing " << s.smoothing
      << "\nlargestOnly " << s.largestOnly << "\nsimplify " << s.simplify << "\ntextureSize " << s.textureSize << "\ntextureScale " << s.textureScale
      << "\nfillHoles " << s.fillHoles << "\nexportSize " << exportSize << "\n";
    for (const auto& p : photos) o << "photo " << p << "\n";
}

static void loadResults();

static bool openProject(const std::string& dir) {
    std::ifstream in(fs::u8path(dir) / "project.txt");
    if (!in) return false;
    project = dir; photos.clear();
    settings = Pipeline::Settings::preset(1, cuda);
    std::string line;
    while (std::getline(in, line)) {
        const size_t sp = line.find(' ');
        if (sp == std::string::npos) continue;
        const std::string k = line.substr(0, sp), v = line.substr(sp + 1);
        Pipeline::Settings& s = settings;
        if (k == "photo") photos.push_back(v);
        else if (k == "quality") s.quality = atoi(v.c_str()); else if (k == "featureDetail") s.featureDetail = atoi(v.c_str());
        else if (k == "akaze") s.akaze = v == "1"; else if (k == "matchNeighbours") s.matchNeighbours = atoi(v.c_str());
        else if (k == "depthScale") s.depthScale = atoi(v.c_str()); else if (k == "depthMaps") s.depthMaps = v == "1" && cuda;
        else if (k == "meshDetail") s.meshDetail = (float)atof(v.c_str()); else if (k == "smoothing") s.smoothing = atoi(v.c_str());
        else if (k == "largestOnly") s.largestOnly = v == "1"; else if (k == "simplify") s.simplify = (float)atof(v.c_str());
        else if (k == "textureSize") s.textureSize = atoi(v.c_str()); else if (k == "textureScale") s.textureScale = atoi(v.c_str());
        else if (k == "fillHoles") s.fillHoles = v == "1"; else if (k == "exportSize") exportSize = (float)atof(v.c_str());
    }
    makeThumbs();
    loadResults();
    SDL_SetWindowTitle(window, ("Simulacrum - " + fs::u8path(dir).filename().u8string()).c_str());
    return true;
}

static void newProject() {
    char name[64];
    const time_t t = time(nullptr); tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    strftime(name, sizeof name, "Model %Y-%m-%d %H-%M-%S", &lt);
    project = (fs::u8path(Proc::projectsDir()) / name).u8string();
    photos.clear(); thumbs.clear(); mesh = Mesh(); points = Points(); cameras.clear(); done = false;
    Viewer::setMesh(nullptr); Viewer::setPoints(nullptr); Viewer::setCameras(nullptr);
    { std::lock_guard<std::mutex> g(mtx); logLines.clear(); }
    buildError.clear();
    SDL_SetWindowTitle(window, "Simulacrum");
}

static void addPhotos(const std::vector<std::string>& list) {
    if (progress.running) { say("Wait for the build to finish (or stop it) before adding photos"); return; }
    if (project.empty()) newProject();
    int added = 0;
    for (const auto& p : list) {
        std::error_code ec;
        const fs::path fp = fs::u8path(p);
        if (fs::exists(fp / "project.txt", ec)) {   // a Simulacrum model: open it
            openProject(p); return;
        } else if (fs::is_directory(fp, ec)) {
            for (const auto& e : fs::directory_iterator(fp, ec))
                if (e.is_regular_file() && isPhoto(e.path()) && std::find(photos.begin(), photos.end(), e.path().u8string()) == photos.end()) { photos.push_back(e.path().u8string()); ++added; }
        } else if (isPhoto(fp) && std::find(photos.begin(), photos.end(), p) == photos.end()) { photos.push_back(p); ++added; }
    }
    if (added) { say(std::to_string(added) + " photo" + (added == 1 ? "" : "s") + " added"); makeThumbs(); saveProject(); forcedView = 0; }
    else if (!list.empty()) say("No photos there (JPG, PNG, TIFF, HEIC or camera RAW)");
}

static void loadResults() {
    std::string err;
    mesh = Mesh(); points = Points(); cameras.clear(); done = false;
    if (project.empty()) return;
    if (fs::exists(fs::u8path(Pipeline::texturedMesh(project)))) done = loadObj(Pipeline::texturedMesh(project), mesh, err);
    if (fs::exists(fs::u8path(Pipeline::pointCloud(project)))) loadPly(Pipeline::pointCloud(project), points, err);
    if (fs::exists(fs::u8path(Pipeline::camerasFile(project)))) loadCameras(Pipeline::camerasFile(project), cameras, err);
    Viewer::setMesh(done ? &mesh : nullptr); Viewer::setPoints(&points); Viewer::setCameras(&cameras);
    Viewer::frameAll();
    if (done || !points.empty()) forcedView = 1;
}

static void startBuild(int from) {
    if (progress.running) return;
    if (photos.size() < 3) { say("Add at least 3 photos (30 or more give a good model)"); return; }
    if (buildThread.joinable()) buildThread.join();
    saveProject();
    buildError.clear();
    { std::lock_guard<std::mutex> g(mtx); logLines.clear(); }
    if (from == 0) { mesh = Mesh(); points = Points(); cameras.clear(); done = false; Viewer::setMesh(nullptr); Viewer::setPoints(nullptr); Viewer::setCameras(nullptr); }
    buildStart = std::chrono::steady_clock::now();
    progress.running = true; progress.stop = false;
    const std::string proj = project; const auto ph = photos; const auto s = settings;
    buildThread = std::thread([proj, ph, s, from] {
        int lastStep = -1;
        const std::string err = Pipeline::build(proj, ph, s, from, progress, [&](const std::string& l) {
            logLine(l);
            const int st = progress.step;
            if (st != lastStep) {
                // after Structure from Motion the points and cameras can be shown while the rest carries on
                const auto all = Pipeline::steps(s);
                if (lastStep >= 0 && lastStep < (int)all.size() && all[lastStep].id == "structureFromMotion")
                    post([] { std::string e; loadPly(Pipeline::pointCloud(project), points, e); loadCameras(Pipeline::camerasFile(project), cameras, e);
                              Viewer::setPoints(&points); Viewer::setCameras(&cameras); Viewer::frameAll(); forcedView = 1; });
                lastStep = st;
            }
        });
        post([err] {
            buildError = err;
            buildSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - buildStart).count();
            loadResults();
            if (err.empty()) say("Your 3D model is ready");
        });
    });
}

// ---------- the window ----------
static void sectionTitle(const char* n, const char* s) {
    ImGui::Spacing();
    ImGui::PushFont(bigFont);
    ImGui::PushStyleColor(ImGuiCol_Text, col4(C_PINK)); ImGui::TextUnformatted(n); ImGui::PopStyleColor();
    ImGui::SameLine(); ImGui::TextUnformatted(s);
    ImGui::PopFont();
}

static bool bigButton(const char* label, float w, bool pink = true) {
    if (pink) { ImGui::PushStyleColor(ImGuiCol_Button, col4(C_PINK)); ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col4(C_DEEP)); ImGui::PushStyleColor(ImGuiCol_ButtonActive, col4(C_DEEP)); }
    ImGui::PushFont(bigFont);
    const bool r = ImGui::Button(label, ImVec2(w, 46));
    ImGui::PopFont();
    if (pink) ImGui::PopStyleColor(3);
    return r;
}

static void tip(const char* s) { if (ImGui::IsItemHovered()) { ImGui::BeginTooltip(); ImGui::PushTextWrapPos(380); ImGui::TextUnformatted(s); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); } }

static bool showHelp = false, showAbout = false;

static void photoTips() {
    ImGui::TextWrapped("For a good model:");
    ImGui::BulletText("Walk all the way round, taking a photo every few steps.");
    ImGui::BulletText("Each photo should share most of what's in the last one.");
    ImGui::BulletText("Go round again higher up and lower down.");
    ImGui::BulletText("Keep it sharp: good light, no zooming between photos.");
    ImGui::BulletText("30 to 200 photos. More is better, but slower.");
    ImGui::BulletText("Shiny, see-through and plain white things are hard.");
}

static void leftPanel(float w) {
    const bool running = progress.running;
    // 1. photos
    sectionTitle("1", "Add your photos");
    if (ImGui::Button("Add photos...", ImVec2((w - 7) / 2, 34)) && !running) addPhotos(pickPhotos());
    ImGui::SameLine();
    if (ImGui::Button("Add a folder...", ImVec2((w - 7) / 2, 34)) && !running) { const std::string f = pickFolder(L"Add every photo in a folder"); if (!f.empty()) addPhotos({f}); }
    if (photos.empty()) ImGui::TextDisabled("Or drop photos or a folder on the window.");
    else {
        ImGui::Text("%zu photos", photos.size());
        ImGui::SameLine(); ImGui::TextDisabled(photos.size() < 20 ? " (more would help)" : "");
        ImGui::SameLine(w - 74);
        if (ImGui::SmallButton("Clear") && !running) { photos.clear(); thumbs.clear(); saveProject(); }
    }
    if (ImGui::SmallButton("How to take good photos")) showHelp = true;

    // 2. quality
    sectionTitle("2", "Choose the quality");
    const char* names[] = {"Draft", "Normal", "High", "Ultra"};
    const char* tips[] = {"Quickest: a rough shape, fine for a first look", "Good detail in a reasonable time (a few minutes to an hour)",
                          "More detail and a sharper texture; takes longer", "The most detail it can find; can take hours and lots of memory"};
    const float bw = (w - 21) / 4;
    for (int q = 0; q < 4; ++q) {
        if (q) ImGui::SameLine();
        const bool on = settings.quality == q;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
        if (ImGui::Button(names[q], ImVec2(bw, 32)) && !running) { const float keep = settings.simplify; settings = Pipeline::Settings::preset(q, cuda); settings.simplify = keep; saveProject(); }
        if (on) ImGui::PopStyleColor();
        tip(tips[q]);
    }
    if (!cuda) ImGui::TextColored(col4(C_DIM), "No NVIDIA graphics card found: the model is\nbuilt from the matched points (less detail).");
    else ImGui::TextDisabled("Graphics card: %s", gpuName.c_str());
    if (ImGui::CollapsingHeader("More settings")) {
        Pipeline::Settings& s = settings;
        bool ch = false;
        ImGui::PushItemWidth(w - 150);
        const char* lvl[] = {"Low", "Normal", "High", "Ultra"};
        ch |= ImGui::SliderInt("Feature detail", &s.featureDetail, 0, 3, lvl[std::clamp(s.featureDetail, 0, 3)]); tip("How many points it looks for in each photo");
        ch |= ImGui::SliderInt("Photos compared", &s.matchNeighbours, 10, 200); tip("How many other photos each one is compared with: more finds more, slower");
        ch |= ImGui::Checkbox("Plain and shiny surfaces too", &s.akaze); tip("A second kind of feature (AKAZE): helps with walls, cars and skin");
        ImGui::BeginDisabled(!cuda);
        ch |= ImGui::Checkbox("Measure depth (detail)", &s.depthMaps); tip("Depth maps on the graphics card: far more detail. Needs an NVIDIA card.");
        ImGui::BeginDisabled(!s.depthMaps);
        int ds = s.depthScale == 1 ? 0 : s.depthScale == 2 ? 1 : s.depthScale == 4 ? 2 : 3;
        const char* dsn[] = {"Full size", "Half", "Quarter", "Eighth"};
        if (ImGui::SliderInt("Depth photo size", &ds, 0, 3, dsn[ds])) { s.depthScale = 1 << ds; ch = true; } tip("Smaller is quicker and uses less graphics memory");
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        ch |= ImGui::SliderFloat("Surface detail", &s.meshDetail, 0.5f, 30, "%.1f million points", ImGuiSliderFlags_Logarithmic);
        ch |= ImGui::SliderInt("Smoothing", &s.smoothing, 0, 30); tip("Passes that smooth the surface: more is softer");
        ch |= ImGui::Checkbox("Only the biggest piece", &s.largestOnly); tip("Drops loose bits floating round the model");
        int simp = (int)std::lround(s.simplify * 100);
        if (ImGui::SliderInt("Keep triangles", &simp, 5, 100, "%d%%")) { s.simplify = simp / 100.0f; ch = true; } tip("Fewer triangles make a lighter file with the same shape");
        int ts = s.textureSize <= 1024 ? 0 : s.textureSize <= 2048 ? 1 : s.textureSize <= 4096 ? 2 : s.textureSize <= 8192 ? 3 : 4;
        const char* tsn[] = {"1024", "2048", "4096", "8192", "16384"};
        if (ImGui::SliderInt("Texture size", &ts, 0, 4, tsn[ts])) { s.textureSize = 1024 << ts; ch = true; }
        int tsc = s.textureScale == 1 ? 0 : s.textureScale == 2 ? 1 : 2;
        const char* tscn[] = {"Full size", "Half", "Quarter"};
        if (ImGui::SliderInt("Texture photos", &tsc, 0, 2, tscn[tsc])) { s.textureScale = 1 << tsc; ch = true; }
        ch |= ImGui::Checkbox("Fill gaps in the texture", &s.fillHoles);
        ImGui::PopItemWidth();
        if (ch) saveProject();
    }

    // 3. build
    sectionTitle("3", "Make the 3D model");
    if (!runtimeMissing.empty()) { ImGui::PushTextWrapPos(w); ImGui::TextColored(col4(C_PINK), "%s", runtimeMissing.c_str()); ImGui::PopTextWrapPos(); return; }
    if (!running) {
        ImGui::BeginDisabled(photos.size() < 3);
        if (bigButton(done ? "Make it again" : "Make my 3D model", w)) startBuild(0);
        ImGui::EndDisabled();
        if (!buildError.empty()) {
            ImGui::PushTextWrapPos(w); ImGui::TextColored(col4(C_PINK), "%s", buildError.c_str()); ImGui::PopTextWrapPos();
            if (ImGui::Button("Carry on from where it stopped", ImVec2(w, 30))) startBuild(-1);
        }
    }
    const auto st = Pipeline::steps(settings);
    if (running) {
        const int cur = progress.step;
        float total = 0, sofar = 0;
        for (size_t i = 0; i < st.size(); ++i) { total += st[i].weight; if ((int)i < cur) sofar += st[i].weight; }
        if (cur >= 0 && cur < (int)st.size()) sofar += st[cur].weight * progress.stepFrac;
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - buildStart).count();
        char ov[64]; snprintf(ov, sizeof ov, "%d%%   %d:%02d", (int)(sofar / total * 100), (int)secs / 60, (int)secs % 60);
        ImGui::ProgressBar(sofar / total, ImVec2(w, 26), ov);
        if (cur >= 0 && cur < (int)st.size()) { ImGui::PushFont(bigFont); ImGui::TextUnformatted(st[cur].title.c_str()); ImGui::PopFont(); ImGui::PushTextWrapPos(w); ImGui::TextDisabled("%s", st[cur].hint.c_str()); ImGui::PopTextWrapPos(); }
        if (ImGui::Button("Stop", ImVec2(w, 30))) progress.stop = true;
        tip("It can carry on from here later");
    } else if (done) {
        if (buildSeconds > 0) ImGui::TextDisabled("Made in %d:%02d. %zu triangles.", (int)buildSeconds / 60, (int)buildSeconds % 60, mesh.triangles());
        else ImGui::TextDisabled("%zu triangles.", mesh.triangles());
    }
    // the steps, ticked as they finish
    if (running || !buildError.empty()) {
        for (size_t i = 0; i < st.size(); ++i) {
            const bool cur = (int)i == progress.step, past = progress.running ? (int)i < progress.step : false;
            ImGui::TextColored(col4(cur ? C_PINK : past ? C_TEXT : C_DIM), "%s  %s", past ? "\xe2\x9c\x93" : cur ? "\xe2\x96\xb6" : "\xc2\xb7", st[i].title.c_str());
        }
    }

    // 4. save
    if (done) {
        sectionTitle("4", "Save it");
        ImGui::PushItemWidth(w - 150);
        ImGui::SliderFloat("Size", &exportSize, 0.01f, 100, "x %.2f", ImGuiSliderFlags_Logarithmic); tip("Photos alone can't tell how big it really is: scale it here before saving");
        ImGui::PopItemWidth();
        const std::string base = (fs::u8path(project) / fs::u8path(project).filename()).u8string();
        std::string err;
        if (ImGui::Button("GLB (any 3D app)", ImVec2(w, 32))) {
            const std::string p = saveDialog(L"GLB 3D model\0*.glb\0\0", L"glb", base + ".glb");
            if (!p.empty()) { if (saveGlb(mesh, p, exportSize, Viewer::look().flipUp, err)) say("Saved " + fs::u8path(p).filename().u8string()); else say(err); }
        }
        tip("One file with the texture inside: Blender, Windows' 3D Viewer, game engines, the web");
        if (ImGui::Button("OBJ (with its textures)", ImVec2((w - 7) / 2, 30))) {
            const std::string p = saveDialog(L"OBJ 3D model\0*.obj\0\0", L"obj", base + ".obj");
            if (!p.empty()) { if (saveObj(mesh, p, exportSize, Viewer::look().flipUp, err)) say("Saved " + fs::u8path(p).filename().u8string()); else say(err); }
        }
        ImGui::SameLine();
        if (ImGui::Button("STL (3D printing)", ImVec2((w - 7) / 2, 30))) {
            const std::string p = saveDialog(L"STL for 3D printing\0*.stl\0\0", L"stl", base + ".stl");
            if (!p.empty()) { if (saveStl(mesh, p, exportSize, Viewer::look().flipUp, err)) say("Saved " + fs::u8path(p).filename().u8string()); else say(err); }
        }
        if (ImGui::Button("Open the project folder", ImVec2(w, 28))) openInExplorer(project);
    }
}

static void photoGrid(ImVec2 size) {
    if (photos.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 a = ImGui::GetCursorScreenPos();
        const ImVec2 mid(a.x + size.x / 2, a.y + size.y / 2);
        const float bw = std::min(560.0f, size.x - 60), bh = std::min(300.0f, size.y - 60);
        const ImVec2 b0(mid.x - bw / 2, mid.y - bh / 2), b1(mid.x + bw / 2, mid.y + bh / 2);
        for (float x = b0.x; x < b1.x; x += 14) { dl->AddLine(ImVec2(x, b0.y), ImVec2(std::min(x + 7, b1.x), b0.y), C_LINE, 2); dl->AddLine(ImVec2(x, b1.y), ImVec2(std::min(x + 7, b1.x), b1.y), C_LINE, 2); }
        for (float y = b0.y; y < b1.y; y += 14) { dl->AddLine(ImVec2(b0.x, y), ImVec2(b0.x, std::min(y + 7, b1.y)), C_LINE, 2); dl->AddLine(ImVec2(b1.x, y), ImVec2(b1.x, std::min(y + 7, b1.y)), C_LINE, 2); }
        auto centred = [&](float y, ImU32 col, const char* s, ImFont* f = nullptr) { const float fs = f ? f->FontSize : ImGui::GetFontSize(); const ImVec2 ts = (f ? f : ImGui::GetFont())->CalcTextSizeA(fs, 1e9f, 0, s); dl->AddText(f, fs, ImVec2(mid.x - ts.x / 2, y), col, s); };
        centred(mid.y - 50, C_TEXT, "Drop your photos here", bigFont);
        centred(mid.y - 14, C_DIM, "Photos of one thing, taken while walking all the way round it.");
        centred(mid.y + 12, C_DIM, "Or use Add photos... on the left.");
        centred(mid.y + 50, C_PINK, "Simulacrum turns them into a 3D model you can turn, print or use anywhere.");
        return;
    }
    const float S = 180, pad = 8;
    const int cols = std::max(1, (int)((size.x - pad) / (S + pad)));
    ImGui::BeginChild("grid", size);
    for (size_t i = 0; i < photos.size(); ++i) {
        if (i % cols) ImGui::SameLine(0, pad);
        Thumb* t = i < thumbs.size() ? &thumbs[i] : nullptr;
        const ImVec2 a = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(S, S * 0.75f));
        if (!ImGui::IsItemVisible() || !t) continue;
        if (!t->tex && !t->px.empty()) {
            glGenTextures(1, &t->tex); glBindTexture(GL_TEXTURE_2D, t->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->px.data());
            t->px.clear(); t->px.shrink_to_fit();
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(a, ImVec2(a.x + S, a.y + S * 0.75f), C_WELL);
        if (t->tex) {
            const float k = std::min(S / t->w, S * 0.75f / t->h), dw = t->w * k, dh = t->h * k;
            const ImVec2 p0(a.x + (S - dw) / 2, a.y + (S * 0.75f - dh) / 2);
            dl->AddImage((ImTextureID)(intptr_t)t->tex, p0, ImVec2(p0.x + dw, p0.y + dh));
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", fs::u8path(photos[i]).filename().u8string().c_str());
    }
    ImGui::EndChild();
}

static void view3D(ImVec2 size) {
    Viewer::Look& lk = Viewer::look();
    // the toolbar
    const char* shades[] = {"Textured", "Clay", "Wireframe"};
    for (int k = 0; k < 3; ++k) {
        if (k) ImGui::SameLine(0, 2);
        const bool on = lk.shading == k;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, col4(C_DEEP));
        if (ImGui::Button(shades[k])) lk.shading = k;
        if (on) ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 16); ImGui::Checkbox("Cameras", &lk.cameras);
    ImGui::SameLine(); ImGui::Checkbox("Floor", &lk.grid);
    ImGui::SameLine(); if (ImGui::Checkbox("Upside down", &lk.flipUp)) Viewer::frameAll(); tip("Turn the model over, if it came out upside down");
    ImGui::SameLine(); ImGui::SetNextItemWidth(120); ImGui::SliderFloat("Light", &lk.light, 0, 1, "%.2f"); tip("How much light shapes the textured model");
    if (!done) { ImGui::SameLine(); ImGui::SetNextItemWidth(110); ImGui::SliderFloat("Points", &lk.pointSize, 1, 8, "%.0f px"); }
    ImGui::SameLine(); if (ImGui::Button("Fit")) Viewer::frameAll(); tip("Or double-click the model");
    ImGui::SameLine(); if (ImGui::Button("Picture...")) {
        std::vector<unsigned char> px; int w, h; Viewer::readPixels(px, w, h);
        const std::string p = saveDialog(L"PNG picture\0*.png\0\0", L"png", (fs::u8path(project) / "view.png").u8string());
        if (!p.empty() && !px.empty()) { stbi_write_png(fs::u8path(p).string().c_str(), w, h, 4, px.data(), w * 4); say("Saved the picture"); }
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 a = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("view", avail, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool hov = ImGui::IsItemHovered() || ImGui::IsItemActive();
    const unsigned tex = Viewer::render((int)avail.x, (int)avail.y, hov);
    ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)tex, a, ImVec2(a.x + avail.x, a.y + avail.y), ImVec2(0, 1), ImVec2(1, 0));
    if (!done && points.empty()) ImGui::GetWindowDrawList()->AddText(ImVec2(a.x + 20, a.y + 20), C_DIM, "The model shows here as it's made.");
    else ImGui::GetWindowDrawList()->AddText(ImVec2(a.x + 12, a.y + avail.y - 26), C_DIM, "Drag to turn, right-drag to move, wheel to zoom, double-click to fit");
    (void)size;
}

static int viewTab = 0;

static void frame() {
    { std::vector<std::function<void()>> todo; { std::lock_guard<std::mutex> g(mtx); todo.swap(onMain); } for (auto& f : todo) f(); }
    ImGuiIO& io = ImGui::GetIO();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    float top = 0;
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New model", nullptr, false, !progress.running)) newProject();
            if (ImGui::MenuItem("Open a model...", nullptr, false, !progress.running)) {
                const std::string f = pickFolder(L"Open a Simulacrum model (its folder in Documents\\Simulacrum)");
                if (!f.empty() && !openProject(f)) say("That folder isn't a Simulacrum model");
            }
            if (ImGui::MenuItem("Show my models")) { std::error_code ec; fs::create_directories(fs::u8path(Proc::projectsDir()), ec); openInExplorer(Proc::projectsDir()); }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) quit = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("How to take good photos")) showHelp = true;
            if (ImGui::MenuItem("About Simulacrum")) showAbout = true;
            ImGui::EndMenu();
        }
        top = ImGui::GetWindowSize().y;
        ImGui::EndMainMenuBar();
    }
    const float statusH = 26, leftW = 420;
    const ImGuiWindowFlags fixed = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::SetNextWindowPos(ImVec2(0, top)); ImGui::SetNextWindowSize(ImVec2(leftW, H - top - statusH));
    ImGui::Begin("left", nullptr, fixed);
    leftPanel(leftW - 28);
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(leftW, top)); ImGui::SetNextWindowSize(ImVec2(W - leftW, H - top - statusH));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("main", nullptr, fixed);
    if (ImGui::BeginTabBar("views")) {
        const char* names[] = {"Photos", "3D model", "Details"};
        for (int k = 0; k < 3; ++k) {
            ImGuiTabItemFlags f = forcedView == k ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(names[k], nullptr, f)) { viewTab = k; ImGui::EndTabItem(); }
        }
        forcedView = -1;
        ImGui::EndTabBar();
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (viewTab == 0) photoGrid(avail);
    else if (viewTab == 1) view3D(avail);
    else {
        ImGui::TextDisabled("What AliceVision says while it works (for when something goes wrong).");
        ImGui::BeginChild("log", ImVec2(0, 0), true);
        { std::lock_guard<std::mutex> g(mtx); for (const auto& l : logLines) ImGui::TextUnformatted(l.c_str()); }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleColor();

    ImGui::SetNextWindowPos(ImVec2(0, H - statusH)); ImGui::SetNextWindowSize(ImVec2(W, statusH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, col4(C_BG));
    ImGui::Begin("status", nullptr, fixed);
    if (now() < toastUntil) ImGui::TextColored(col4(C_PINK), "%s", toast.c_str());
    else if (!project.empty()) ImGui::TextDisabled("%s", project.c_str());
    ImGui::End();
    ImGui::PopStyleColor(); ImGui::PopStyleVar();

    if (showHelp) { ImGui::OpenPopup("help"); showHelp = false; }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("help", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::PushFont(bigFont); ImGui::TextColored(col4(C_PINK), "How to take good photos"); ImGui::PopFont();
        photoTips();
        if (ImGui::Button("Got it", ImVec2(120, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (showAbout) { ImGui::OpenPopup("about"); showAbout = false; }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::BeginPopupModal("about", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        Splash::draw(ImGui::GetWindowDrawList(), at.x, at.y, nullptr, -1);
        ImGui::InvisibleButton("splash", ImVec2(Splash::W, Splash::H));
        if (ImGui::IsItemClicked() || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
}

int main(int argc, char** argv) {
    std::string capture;
    bool make = false, makeResume = false; int makeQuality = -1;
    std::vector<std::string> args, files;
#ifdef _WIN32
    { int n; LPWSTR* wa = CommandLineToArgvW(GetCommandLineW(), &n); if (wa) { for (int i = 1; i < n; ++i) args.push_back(fs::path(wa[i]).u8string()); LocalFree(wa); } }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#else
    for (int i = 1; i < argc; ++i) args.push_back(argv[i]);
#endif
    (void)argc; (void)argv;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--capture" && i + 1 < args.size()) capture = args[++i];
        else if (args[i] == "--view" && i + 1 < args.size()) forcedView = atoi(args[++i].c_str());
        else if (args[i] == "--make") make = true;
        else if (args[i] == "--resume") { make = true; makeResume = true; }
        else if (args[i] == "--quality" && i + 1 < args.size()) makeQuality = atoi(args[++i].c_str());
        else files.push_back(args[i]);
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    window = SDL_CreateWindow("Simulacrum", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1500, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    SDL_SetWindowMinimumSize(window, 1100, 650);
    SDL_GLContext gl = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);
    char* bp = SDL_GetBasePath();
    SDL_Surface* icon = bp ? SDL_LoadBMP((std::string(bp) + "assets/icon.bmp").c_str()) : nullptr;
    if (bp) SDL_free(bp);
    if (icon) SDL_SetWindowIcon(window, icon);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    loadFont();
    applyStyle();

    SDL_Window* splashWin = nullptr;
    if (capture.empty()) {
        splashWin = SDL_CreateWindow("Simulacrum", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, (int)Splash::W, (int)Splash::H, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        if (splashWin && SDL_GL_MakeCurrent(splashWin, gl) != 0) { SDL_DestroyWindow(splashWin); splashWin = nullptr; SDL_GL_MakeCurrent(window, gl); }
        if (splashWin && icon) SDL_SetWindowIcon(splashWin, icon);
    }
    if (icon) SDL_FreeSurface(icon);
    ImGui_ImplSDL2_InitForOpenGL(splashWin ? splashWin : window, gl);
    ImGui_ImplOpenGL3_Init("#version 330");
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    // starting up: the runtime, the graphics card (asked in the background while the splash shows)
    static std::atomic<bool> ready{false};
    static std::string status = "Starting...";
    std::thread([] {
        Proc::setupEnvironment();
        if (Proc::runtimeDir().empty()) runtimeMissing = "Simulacrum's 3D engine (AliceVision) is missing. Please install Simulacrum again.";
        status = "Looking for the graphics card...";
        std::string n; const bool c = Proc::hasCuda(n);
        post([c, n] { cuda = c; gpuName = n; settings = Pipeline::Settings::preset(1, cuda); });
        status = "Ready";
        ready = true;
    }).detach();
    if (splashWin) {
        const Uint32 start = SDL_GetTicks();
        bool skip = false;
        for (;;) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                ImGui_ImplSDL2_ProcessEvent(&e);
                if (e.type == SDL_QUIT) quit = true;
                if (e.type == SDL_MOUSEBUTTONDOWN || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)) skip = true;
            }
            const Uint32 t = SDL_GetTicks() - start;
            if ((skip && ready) || quit || (ready && t >= 1500) || t >= 15000) break;
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
            Splash::draw(ImGui::GetBackgroundDrawList(), 0, 0, status.c_str(), ready ? 1.0f : std::min(0.9f, t / 1500.0f));
            ImGui::Render();
            int sw, sh; SDL_GL_GetDrawableSize(splashWin, &sw, &sh);
            glViewport(0, 0, sw, sh); glClearColor(0.1f, 0.1f, 0.1f, 1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            SDL_GL_SwapWindow(splashWin);
        }
        ImGui_ImplSDL2_Shutdown();
        SDL_DestroyWindow(splashWin);
        SDL_GL_MakeCurrent(window, gl);
        ImGui_ImplSDL2_InitForOpenGL(window, gl);
    } else while (!ready) SDL_Delay(10);
    { std::vector<std::function<void()>> todo; { std::lock_guard<std::mutex> g(mtx); todo.swap(onMain); } for (auto& f : todo) f(); }
    Viewer::init();
    if (!files.empty()) addPhotos(files);
    if (make) {
        if (makeQuality >= 0) settings = Pipeline::Settings::preset(makeQuality, cuda);
        startBuild(makeResume ? -1 : 0);
        forcedView = 1;
    }
    if (capture.empty()) { SDL_ShowWindow(window); SDL_MaximizeWindow(window); SDL_RaiseWindow(window); }

    int frames = 0;
    while (!quit) {
        SDL_Event e;
        std::vector<std::string> dropped;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_DROPFILE) { dropped.push_back(e.drop.file); SDL_free(e.drop.file); }
        }
        if (!dropped.empty()) addPhotos(dropped);
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL2_NewFrame(); ImGui::NewFrame();
        frame();
        ImGui::Render();
        int w, h; SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h); glClearColor(0.1f, 0.1f, 0.1f, 1); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frames;
        const bool waiting = make && (progress.running || frames < 30 || (buildThread.joinable() && !done && buildError.empty()));
        if (make && !capture.empty() && !buildError.empty() && !progress.running) {   // the reason, beside the picture
            if (FILE* f = fopen((capture + ".txt").c_str(), "w")) { fprintf(f, "build failed: %s\n", buildError.c_str()); fclose(f); }
        }
        if (!capture.empty() && frames >= 30 && !waiting) {
            std::vector<unsigned char> px((size_t)w * h * 4);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            std::vector<unsigned char> flip(px.size());
            for (int y = 0; y < h; ++y) memcpy(&flip[(size_t)y * w * 4], &px[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
            SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom(flip.data(), w, h, 32, w * 4, SDL_PIXELFORMAT_ABGR8888);
            SDL_SaveBMP(s, capture.c_str());
            SDL_FreeSurface(s);
            break;
        }
        SDL_GL_SwapWindow(window);
    }
    progress.stop = true;
    if (buildThread.joinable()) buildThread.join();
    thumbStop = true;
    if (thumbThread.joinable()) thumbThread.join();
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL2_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
