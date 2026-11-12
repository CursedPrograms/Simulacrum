// Terminus - the photogrammetry steps (see pipeline.h). The programs and their options are AliceVision's, as
// Meshroom 2025.1 runs them (AliceVision 3.3, MPL-2.0).
#include "pipeline.h"
#include "proc.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

namespace Pipeline {

Settings Settings::preset(int q, bool cuda) {
    Settings s;
    s.quality = q;
    s.depthMaps = cuda && q > 0;
    switch (q) {
    case 0: s.featureDetail = 1; s.meshDetail = 1; s.smoothing = 3; s.textureSize = 4096; s.textureScale = 2; s.depthScale = 4; break;
    case 1: s.featureDetail = 1; s.meshDetail = 5; s.smoothing = 5; s.textureSize = 8192; s.textureScale = 2; s.depthScale = 2; break;
    case 2: s.featureDetail = 2; s.meshDetail = 10; s.smoothing = 5; s.textureSize = 8192; s.textureScale = 1; s.depthScale = 2; s.matchNeighbours = 60; break;
    default: s.featureDetail = 3; s.meshDetail = 20; s.smoothing = 3; s.textureSize = 16384; s.textureScale = 1; s.depthScale = 1; s.matchNeighbours = 80; s.akaze = true; break;
    }
    return s;
}

std::vector<Step> steps(const Settings& s) {
    std::vector<Step> v = {
        {"cameraInit", "Reading the photos", "Each photo's camera and lens, from its EXIF", 1},
        {"featureExtraction", "Finding features", "Corners and spots it can recognise again in other photos", 8},
        {"imageMatching", "Pairing photos", "Which photos show the same part of the subject", 2},
        {"featureMatching", "Matching features", "The same points, found across the photo pairs", 8},
        {"structureFromMotion", "Placing the cameras", "Where each photo was taken from, and the first 3D points", 10},
        {"prepareDenseScene", "Preparing the photos", "Straightened photos, ready for the detail", 3},
    };
    if (s.depthMaps) {
        v.push_back({"depthMap", "Measuring depth", "How far away every pixel is, photo by photo (on the graphics card)", 30});
        v.push_back({"depthMapFilter", "Cleaning the depth", "Keeping only depth that other photos agree with", 6});
    }
    v.push_back({"meshing", "Building the surface", "A solid surface through all the measured points", 15});
    v.push_back({"meshFiltering", "Smoothing", "Smoothing the surface and dropping stray triangles", 2});
    if (s.simplify < 0.999f) v.push_back({"meshDecimate", "Simplifying", "Fewer triangles, the same shape", 2});
    v.push_back({"texturing", "Painting the texture", "The photos' colours, laid onto the surface", 12});
    return v;
}

static std::string dirOf(const std::string& project, int i, const std::string& id) {
    char b[16]; snprintf(b, sizeof b, "%02d_", i + 1);
    return (fs::u8path(project) / "cache" / (b + id)).u8string();
}

static std::string at(const std::string& dir, const char* file) { return (fs::u8path(dir) / file).u8string(); }

std::string texturedMesh(const std::string& project) { return (fs::u8path(project) / "model" / "texturedMesh.obj").u8string(); }
std::string pointCloud(const std::string& project) { return (fs::u8path(project) / "model" / "points.ply").u8string(); }
std::string camerasFile(const std::string& project) { return (fs::u8path(project) / "model" / "cameras.sfm").u8string(); }

static const char* DETAIL[] = {"low", "normal", "high", "ultra"};

std::string build(const std::string& project, const std::vector<std::string>& photos, const Settings& s, int from,
                  Progress& p, const std::function<void(const std::string&)>& logOut) {
    std::error_code ec;
    const fs::path proj = fs::u8path(project);
    fs::create_directories(proj / "cache", ec);
    fs::create_directories(proj / "model", ec);

    // the photos, copied into the project, so it doesn't matter where they came from (or if they move later)
    const fs::path pdir = proj / "photos";
    fs::create_directories(pdir, ec);
    for (const auto& ph : photos) {
        const fs::path src = fs::u8path(ph), dst = pdir / src.filename();
        if (!fs::exists(dst, ec)) fs::copy_file(src, dst, ec);
    }

    const auto st = steps(s);
    // a fresh start from `from`: that step and everything after it go
    if (from >= 0)
        for (size_t i = from; i < st.size(); ++i) fs::remove_all(fs::u8path(dirOf(project, (int)i, st[i].id)), ec);
    if (from == 0) for (const auto& e : fs::directory_iterator(proj / "model", ec)) fs::remove_all(e.path(), ec);   // a fresh start (not carrying on)

    auto dir = [&](const std::string& id) { for (size_t i = 0; i < st.size(); ++i) if (st[i].id == id) return dirOf(project, (int)i, id); return std::string(); };
    const std::string ci = dir("cameraInit"), fe = dir("featureExtraction"), im = dir("imageMatching"), fm = dir("featureMatching"),
                      sfm = dir("structureFromMotion"), pds = dir("prepareDenseScene"), dm = dir("depthMap"), dmf = dir("depthMapFilter"),
                      me = dir("meshing"), mf = dir("meshFiltering"), md = dir("meshDecimate"), tx = dir("texturing");
    const std::string describers = s.akaze ? "dspsift,akaze" : "dspsift";
    const std::string verbose = "info";
    const std::string threads = "0";

    p.running = true; p.stop = false;
    std::string err;
    FILE* logFile = nullptr;
#ifdef _WIN32
    logFile = _wfopen((proj / "build.log").wstring().c_str(), L"ab");
#endif
    auto log = [&](const std::string& l) { logOut(l); if (logFile) { fputs(l.c_str(), logFile); fputc('\n', logFile); fflush(logFile); } };
    for (size_t i = 0; i < st.size(); ++i) {
        const Step& step = st[i];
        const std::string out = dirOf(project, (int)i, step.id);
        if (fs::exists(fs::u8path(out) / "done", ec)) continue;   // finished before
        fs::remove_all(fs::u8path(out), ec);
        fs::create_directories(fs::u8path(out), ec);
        p.step = (int)i; p.stepFrac = 0;
        log("== " + step.title);

        std::string exe; std::vector<std::string> a;
        auto arg = [&](const char* k, const std::string& v) { a.push_back(k); a.push_back(v); };
        if (step.id == "cameraInit") {
            exe = "aliceVision_cameraInit";
            arg("--imageFolder", pdir.u8string());
            arg("--sensorDatabase", (fs::u8path(Proc::runtimeDir()) / "share" / "aliceVision" / "cameraSensors.db").u8string());
            arg("--defaultFieldOfView", "45.0");
            arg("--groupCameraFallback", "folder");
            arg("--allowSingleView", "1");
            arg("--output", at(out, "cameraInit.sfm"));
        } else if (step.id == "featureExtraction") {
            exe = "aliceVision_featureExtraction";
            arg("--input", at(ci, "cameraInit.sfm"));
            arg("--describerTypes", describers);
            arg("--describerPreset", DETAIL[s.featureDetail]);
            arg("--describerQuality", DETAIL[s.featureDetail]);
            arg("--forceCpuExtraction", s.cpuFeatures ? "1" : "0");
            arg("--contrastFiltering", "GridSort");
            arg("--relativePeakThreshold", "0.01");
            arg("--gridFiltering", "1");
            arg("--workingColorSpace", "sRGB");
            arg("--output", out);
        } else if (step.id == "imageMatching") {
            exe = "aliceVision_imageMatching";
            arg("--input", at(ci, "cameraInit.sfm"));
            arg("--featuresFolders", fe);
            arg("--method", "SequentialAndVocabularyTree");
            arg("--tree", (fs::u8path(Proc::runtimeDir()) / "share" / "aliceVision" / "vlfeat_K80L3.SIFT.tree").u8string());
            arg("--nbNeighbors", "5");
            arg("--nbMatches", std::to_string(s.matchNeighbours));
            arg("--minNbImages", "200");
            arg("--maxDescriptors", "500");
            arg("--output", at(out, "imageMatches.txt"));
        } else if (step.id == "featureMatching") {
            exe = "aliceVision_featureMatching";
            arg("--input", at(ci, "cameraInit.sfm"));
            arg("--featuresFolders", fe);
            arg("--imagePairsList", at(im, "imageMatches.txt"));
            arg("--describerTypes", describers);
            arg("--output", out);
        } else if (step.id == "structureFromMotion") {
            exe = "aliceVision_incrementalSfM";
            arg("--input", at(ci, "cameraInit.sfm"));
            arg("--featuresFolders", fe);
            arg("--matchesFolders", fm);
            arg("--describerTypes", describers);
            arg("--output", at(out, "sfm.abc"));
            arg("--outputViewsAndPoses", at(out, "cameras.sfm"));
            arg("--extraInfoFolder", out);
            arg("--interFileExtension", ".abc");
            arg("--useLocalBA", "1");
            arg("--filterTrackForks", "0");
            arg("--lockScenePreviouslyReconstructed", "0");
            arg("--observationConstraint", "Scale");
            arg("--computeStructureColor", "1");
            arg("--useAutoTransform", "1");
        } else if (step.id == "prepareDenseScene") {
            exe = "aliceVision_prepareDenseScene";
            arg("--input", at(sfm, "sfm.abc"));
            arg("--outputFileType", "exr");
            arg("--output", out);
        } else if (step.id == "depthMap") {
            exe = "aliceVision_depthMapEstimation";
            arg("--input", at(sfm, "sfm.abc"));
            arg("--imagesFolder", pds);
            arg("--downscale", std::to_string(s.depthScale));
            arg("--sgmDepthListPerTile", "1");
            arg("--output", out);
        } else if (step.id == "depthMapFilter") {
            exe = "aliceVision_depthMapFiltering";
            arg("--input", at(sfm, "sfm.abc"));
            arg("--depthMapsFolder", dm);
            arg("--output", out);
        } else if (step.id == "meshing") {
            exe = "aliceVision_meshing";
            arg("--input", at(sfm, "sfm.abc"));
            if (s.depthMaps) arg("--depthMapsFolder", dmf);
            arg("--maxPoints", std::to_string((long long)(s.meshDetail * 1e6)));
            // Meshroom's own values where the program's defaults differ
            arg("--maxPointsPerVoxel", "1000000");
            arg("--pixSizeMarginFinalCoef", "4");
            arg("--minAngleThreshold", "1");
            arg("--outputMesh", at(out, "mesh.obj"));
            arg("--output", at(out, "densePointCloud.abc"));
        } else if (step.id == "meshFiltering") {
            exe = "aliceVision_meshFiltering";
            arg("--inputMesh", at(me, "mesh.obj"));
            arg("--keepLargestMeshOnly", s.largestOnly ? "1" : "0");
            arg("--smoothingIterations", std::to_string(s.smoothing));
            arg("--filterLargeTrianglesFactor", "60");
            arg("--outputMesh", at(out, "mesh.obj"));
        } else if (step.id == "meshDecimate") {
            exe = "aliceVision_meshDecimate";
            arg("--input", at(mf, "mesh.obj"));
            arg("--simplificationFactor", std::to_string(1.0f - s.simplify));
            arg("--output", at(out, "mesh.obj"));
        } else if (step.id == "texturing") {
            exe = "aliceVision_texturing";
            arg("--input", at(me, "densePointCloud.abc"));
            arg("--imagesFolder", pds);
            arg("--inputMesh", at(s.simplify < 0.999f ? md : mf, "mesh.obj"));
            arg("--textureSide", std::to_string(s.textureSize));
            arg("--downscale", std::to_string(s.textureScale));
            arg("--colorMappingFileType", "png");
            arg("--fillHoles", s.fillHoles ? "1" : "0");
            arg("--outputMeshFileType", "obj");
            arg("--output", out);
        }
        arg("--verboseLevel", verbose);

        // progress: "N/M" counts in its log, and the stars of its progress bars
        static const std::regex frac(R"((\d+)\s*/\s*(\d+))");
        int stars = 0;
        const int code = Proc::run(exe, a, out, [&](const std::string& line) {
            log(line);
            stars += (int)std::count(line.begin(), line.end(), '*');
            std::smatch m;
            if (std::regex_search(line, m, frac)) {
                const double n = atof(m[1].str().c_str()), d = atof(m[2].str().c_str());
                if (d >= 2 && n <= d) p.stepFrac = (float)std::max<double>(p.stepFrac, n / d);
            } else if (stars > 0) p.stepFrac = std::min(1.0f, stars / 51.0f);
        }, p.stop);
        if (code == -2) { err = "Stopped"; break; }
        if (code != 0) { err = step.title + " didn't work (" + exe + " ended with " + std::to_string(code) + "): see the log"; break; }
        if (FILE* f = fopen((fs::u8path(out) / "done").string().c_str(), "wb")) fclose(f);
        p.stepFrac = 1;

        // what the window can show along the way
        if (step.id == "structureFromMotion") {
            fs::copy_file(fs::u8path(at(out, "cameras.sfm")), fs::u8path(camerasFile(project)), fs::copy_options::overwrite_existing, ec);
            std::atomic<bool> no{false};
            Proc::run("aliceVision_convertSfMFormat", {"--input", at(out, "sfm.abc"), "--output", pointCloud(project), "--describerTypes", describers,
                      "--views", "0", "--intrinsics", "0", "--extrinsics", "0", "--structure", "1", "--observations", "0"}, out, [&](const std::string& l) { log(l); }, no);
        }
        if (step.id == "texturing") {
            for (const auto& e : fs::directory_iterator(fs::u8path(out), ec)) {
                const std::string ext = e.path().extension().u8string();
                if (ext == ".obj" || ext == ".mtl" || ext == ".png")
                    fs::copy_file(e.path(), proj / "model" / e.path().filename(), fs::copy_options::overwrite_existing, ec);
            }
        }
    }
    if (!err.empty()) log("!! " + err);
    if (logFile) fclose(logFile);
    p.step = -1; p.running = false;
    return err;
}

}
