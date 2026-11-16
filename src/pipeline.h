// Simulacrum - photos to a textured 3D model: AliceVision's photogrammetry, one step after another, each step's
// output kept so a stopped build carries on where it was.
#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace Pipeline {

// every creative choice is a slider (or a switch); the quality preset just moves them all at once
struct Settings {
    int quality = 1;              // 0 draft, 1 normal, 2 high, 3 ultra: the preset the sliders came from
    int featureDetail = 1;        // 0 low, 1 normal, 2 high, 3 ultra: how many points it looks for in each photo
    bool akaze = false;           // a second kind of feature: helps with plain or shiny surfaces
    int matchNeighbours = 40;     // photos each one is compared with, picked by likeness (more: slower, finds more)
    int depthScale = 2;           // the photos at 1/depthScale for the depth maps (1 full size .. 8)
    bool depthMaps = true;        // off: the draft mesh from the matched points only (no NVIDIA card needed)
    float meshDetail = 5;         // millions of points the mesh may use (0.5 .. 30)
    int smoothing = 5;            // smoothing passes (0 .. 30)
    bool largestOnly = false;     // keep only the biggest piece (drops floating bits)
    float simplify = 1.0f;        // 1 keeps every triangle; 0.1 keeps a tenth
    int textureSize = 8192;       // 1024 .. 16384
    int textureScale = 2;         // the photos at 1/textureScale for the texture
    bool fillHoles = true;        // fill gaps in the texture
    bool cpuFeatures = true;      // find features on the CPU (the GPU path can fail on some cards)
    static Settings preset(int quality, bool cuda);
};

struct Step {
    std::string id;               // folder name and program
    std::string title;            // what the window says
    std::string hint;             // one line on what it does
    float weight;                 // rough share of the whole build's time
};
std::vector<Step> steps(const Settings& s);

struct Progress {
    std::atomic<int> step{-1};        // which step runs (-1 none)
    std::atomic<float> stepFrac{0};   // 0..1 inside it, when the program says
    std::atomic<bool> stop{false};
    std::atomic<bool> running{false};
};

// project: a folder (Documents\Simulacrum\<name>); photos: image files. Runs from the first step whose output is
// missing (or from `from`, discarding that step and the later ones). Returns "" or what went wrong.
std::string build(const std::string& project, const std::vector<std::string>& photos, const Settings& s, int from,
                  Progress& p, const std::function<void(const std::string&)>& log);

// where the results are, once there
std::string texturedMesh(const std::string& project);   // OBJ (+ MTL + textures beside it)
std::string pointCloud(const std::string& project);     // the matched points as PLY, after Structure from Motion
std::string camerasFile(const std::string& project);    // the cameras (sfm JSON)

}
