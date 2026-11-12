// Terminus - what a build makes: the textured mesh, the matched points and the cameras; read for the viewer and
// written out as GLB (one file, for any 3D app), OBJ or STL (for 3D printing).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct Material {
    std::string name, texturePath;   // the PNG beside the OBJ ("" = none)
    int w = 0, h = 0;
    std::vector<uint8_t> rgba;       // the texture, loaded
};

struct Mesh {
    std::vector<float> pos, uv, nrm;  // per corner (unindexed), 3 / 2 / 3 floats
    std::vector<int> matOf;           // per triangle
    std::vector<Material> mats;
    float lo[3] = {0, 0, 0}, hi[3] = {0, 0, 0};
    size_t triangles() const { return pos.size() / 9; }
    bool empty() const { return pos.empty(); }
};

struct Points {
    std::vector<float> pos;           // xyz
    std::vector<uint8_t> rgb;
    bool empty() const { return pos.empty(); }
};

struct Camera {
    float R[9];                       // world to camera rotation, row major
    float C[3];                       // centre in the world
    float aspect = 1.5f;
    std::string photo;
};

bool loadObj(const std::string& path, Mesh& m, std::string& err);
bool loadPly(const std::string& path, Points& p, std::string& err);
bool loadCameras(const std::string& sfmJson, std::vector<Camera>& cams, std::string& err);
void computeNormals(Mesh& m);

// exports; scale multiplies every position (metres are what the build guessed: set the real size here)
bool saveGlb(const Mesh& m, const std::string& path, float scale, bool flipUp, std::string& err);
bool saveStl(const Mesh& m, const std::string& path, float scale, bool flipUp, std::string& err);
bool saveObj(const Mesh& m, const std::string& path, float scale, bool flipUp, std::string& err);   // + .mtl and the PNGs beside it
