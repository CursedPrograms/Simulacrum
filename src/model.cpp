// Terminus - reading and writing models (see model.h)
#include "model.h"
#include "json.h"

#include <stb_image.h>

#include <algorithm>
#include <array>
#include <unordered_map>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace fs = std::filesystem;

static FILE* openU8(const std::string& p, const char* mode) {
#ifdef _WIN32
    return _wfopen(fs::u8path(p).wstring().c_str(), std::wstring(mode, mode + strlen(mode)).c_str());
#else
    return fopen(p.c_str(), mode);
#endif
}

static bool readAll(const std::string& p, std::string& out) {
    FILE* f = openU8(p, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    const bool ok = n <= 0 || fread(out.data(), 1, out.size(), f) == out.size();
    fclose(f);
    return ok;
}

static void loadMtl(const fs::path& path, std::map<std::string, int>& byName, std::vector<Material>& mats) {
    std::string t;
    if (!readAll(path.u8string(), t)) return;
    std::istringstream in(t);
    std::string line;
    Material* cur = nullptr;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ls(line);
        std::string k; ls >> k;
        if (k == "newmtl") {
            std::string n; std::getline(ls >> std::ws, n);
            byName[n] = (int)mats.size(); mats.push_back(Material{n}); cur = &mats.back();
        } else if (k == "map_Kd" && cur) {
            std::string f; std::getline(ls >> std::ws, f);
            cur->texturePath = (path.parent_path() / fs::u8path(f)).u8string();
        }
    }
}

bool loadObj(const std::string& path, Mesh& m, std::string& err) {
    m = Mesh();
    std::string t;
    if (!readAll(path, t)) { err = "Couldn't read " + path; return false; }
    std::vector<float> v, vt, vn;
    std::map<std::string, int> byName;
    int mat = 0;
    const char* s = t.c_str();
    const char* end = s + t.size();
    std::vector<int> fv, ft, fnn;
    while (s < end) {
        const char* eol = (const char*)memchr(s, '\n', end - s);
        if (!eol) eol = end;
        // strtof, not sscanf: MSVC's sscanf measures the whole rest of the file on every call (minutes on a big model)
        char* q = nullptr;
        if (s[0] == 'v' && s[1] == ' ') { q = (char*)s + 2; for (int k = 0; k < 3; ++k) v.push_back(strtof(q, &q)); }
        else if (s[0] == 'v' && s[1] == 't') { q = (char*)s + 3; for (int k = 0; k < 2; ++k) vt.push_back(strtof(q, &q)); }
        else if (s[0] == 'v' && s[1] == 'n') { q = (char*)s + 3; for (int k = 0; k < 3; ++k) vn.push_back(strtof(q, &q)); }
        else if (s[0] == 'f' && s[1] == ' ') {
            fv.clear(); ft.clear(); fnn.clear();
            const char* p = s + 2;
            while (p < eol) {
                while (p < eol && (*p == ' ' || *p == '\t' || *p == '\r')) ++p;
                if (p >= eol) break;
                int a = 0, b = 0, c = 0;
                a = (int)strtol(p, (char**)&p, 10);
                if (*p == '/') { ++p; if (*p != '/') b = (int)strtol(p, (char**)&p, 10); if (*p == '/') { ++p; c = (int)strtol(p, (char**)&p, 10); } }
                while (p < eol && *p != ' ' && *p != '\t') ++p;
                auto fix = [](int i, size_t n) { return i < 0 ? (int)n + i : i - 1; };
                fv.push_back(fix(a, v.size() / 3)); ft.push_back(b ? fix(b, vt.size() / 2) : -1); fnn.push_back(c ? fix(c, vn.size() / 3) : -1);
            }
            for (size_t k = 1; k + 1 < fv.size(); ++k)
                for (size_t q : {(size_t)0, k, k + 1}) {
                    const int i = fv[q];
                    if (i < 0 || (size_t)i * 3 + 2 >= v.size()) { err = "The OBJ is damaged"; return false; }
                    m.pos.insert(m.pos.end(), {v[i * 3], v[i * 3 + 1], v[i * 3 + 2]});
                    if (ft[q] >= 0 && (size_t)ft[q] * 2 + 1 < vt.size()) m.uv.insert(m.uv.end(), {vt[ft[q] * 2], vt[ft[q] * 2 + 1]});
                    else m.uv.insert(m.uv.end(), {0, 0});
                    if (fnn[q] >= 0 && (size_t)fnn[q] * 3 + 2 < vn.size()) m.nrm.insert(m.nrm.end(), {vn[fnn[q] * 3], vn[fnn[q] * 3 + 1], vn[fnn[q] * 3 + 2]});
                    if (q == k + 1) m.matOf.push_back(mat);
                }
        } else if (!strncmp(s, "usemtl ", 7)) {
            std::string n(s + 7, eol); while (!n.empty() && (n.back() == '\r' || n.back() == ' ')) n.pop_back();
            auto it = byName.find(n);
            if (it == byName.end()) { byName[n] = (int)m.mats.size(); m.mats.push_back(Material{n}); mat = (int)m.mats.size() - 1; }
            else mat = it->second;
        } else if (!strncmp(s, "mtllib ", 7)) {
            std::string n(s + 7, eol); while (!n.empty() && (n.back() == '\r' || n.back() == ' ')) n.pop_back();
            loadMtl(fs::u8path(path).parent_path() / fs::u8path(n), byName, m.mats);
        }
        s = eol + 1;
    }
    if (m.pos.empty()) { err = "The model is empty"; return false; }
    if (m.nrm.size() != m.pos.size()) computeNormals(m);
    if (m.mats.empty()) m.mats.push_back(Material{"default"});
    for (auto& mt : m.mats) {
        if (mt.texturePath.empty()) continue;
        std::string bytes;
        if (!readAll(mt.texturePath, bytes)) continue;
        int w, h, n;
        stbi_set_flip_vertically_on_load(1);
        if (uint8_t* px = stbi_load_from_memory((const uint8_t*)bytes.data(), (int)bytes.size(), &w, &h, &n, 4)) {
            mt.w = w; mt.h = h; mt.rgba.assign(px, px + (size_t)w * h * 4); stbi_image_free(px);
        }
    }
    for (int c = 0; c < 3; ++c) { m.lo[c] = 1e30f; m.hi[c] = -1e30f; }
    for (size_t i = 0; i < m.pos.size(); i += 3) for (int c = 0; c < 3; ++c) { m.lo[c] = std::min(m.lo[c], m.pos[i + c]); m.hi[c] = std::max(m.hi[c], m.pos[i + c]); }
    return true;
}

void computeNormals(Mesh& m) {
    // smooth normals: each corner gets the area-weighted normals of the triangles sharing its position
    struct Key { uint32_t x, y, z; bool operator==(const Key& o) const { return x == o.x && y == o.y && z == o.z; } };
    struct Hash { size_t operator()(const Key& k) const { return ((size_t)k.x * 73856093u) ^ ((size_t)k.y * 19349663u) ^ ((size_t)k.z * 83492791u); } };
    auto key = [](const float* p) { Key k; memcpy(&k.x, p, 4); memcpy(&k.y, p + 1, 4); memcpy(&k.z, p + 2, 4); return k; };
    std::unordered_map<Key, std::array<float, 3>, Hash> acc;
    acc.reserve(m.pos.size() / 6);
    for (size_t t = 0; t < m.triangles(); ++t) {
        const float* a = &m.pos[t * 9]; const float* b = a + 3; const float* c = a + 6;
        const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]}, v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
        const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        for (int k = 0; k < 3; ++k) { auto& s = acc[key(a + k * 3)]; for (int c2 = 0; c2 < 3; ++c2) s[c2] += n[c2]; }
    }
    m.nrm.resize(m.pos.size());
    for (size_t i = 0; i < m.pos.size(); i += 3) {
        const auto& s = acc[key(&m.pos[i])];
        const float l = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]);
        for (int c = 0; c < 3; ++c) m.nrm[i + c] = l > 0 ? s[c] / l : 0;
    }
}

bool loadPly(const std::string& path, Points& p, std::string& err) {
    p = Points();
    std::string t;
    if (!readAll(path, t)) { err = "Couldn't read " + path; return false; }
    const size_t he = t.find("end_header");
    if (t.compare(0, 3, "ply") != 0 || he == std::string::npos) { err = "Not a PLY file"; return false; }
    std::istringstream h(t.substr(0, he));
    std::string line;
    bool binary = false, inVertex = false;
    long long count = 0;
    struct Prop { std::string name, type; };
    std::vector<Prop> props;
    while (std::getline(h, line)) {
        std::istringstream ls(line); std::string k; ls >> k;
        if (k == "format") { std::string f; ls >> f; binary = f == "binary_little_endian"; if (f == "binary_big_endian") { err = "Big-endian PLY isn't supported"; return false; } }
        else if (k == "element") { std::string e; ls >> e >> count; inVertex = e == "vertex"; if (!inVertex && count > 0 && props.empty()) {} }
        else if (k == "property" && inVertex) { std::string ty, n; ls >> ty; if (ty == "list") { err = "Unexpected PLY layout"; return false; } ls >> n; props.push_back({n, ty}); }
    }
    size_t at = t.find('\n', he) + 1;
    auto size = [](const std::string& ty) { return ty == "double" || ty == "float64" ? 8 : ty == "float" || ty == "float32" || ty == "int" || ty == "uint" || ty == "int32" || ty == "uint32" ? 4 : ty == "short" || ty == "ushort" || ty == "int16" || ty == "uint16" ? 2 : 1; };
    p.pos.reserve((size_t)count * 3); p.rgb.reserve((size_t)count * 3);
    std::istringstream asc(binary ? std::string() : t.substr(at));
    for (long long i = 0; i < count; ++i) {
        float xyz[3] = {0, 0, 0}; uint8_t col[3] = {200, 200, 200};
        for (const auto& pr : props) {
            double val = 0;
            if (binary) {
                const int n = size(pr.type);
                if (at + n > t.size()) { err = "The PLY is cut short"; return false; }
                const char* q = &t[at]; at += n;
                if (pr.type == "float" || pr.type == "float32") { float f; memcpy(&f, q, 4); val = f; }
                else if (pr.type == "double" || pr.type == "float64") { memcpy(&val, q, 8); }
                else if (n == 1) val = (uint8_t)*q;
                else if (n == 2) { uint16_t u; memcpy(&u, q, 2); val = u; }
                else { int32_t u; memcpy(&u, q, 4); val = u; }
            } else asc >> val;
            if (pr.name == "x") xyz[0] = (float)val; else if (pr.name == "y") xyz[1] = (float)val; else if (pr.name == "z") xyz[2] = (float)val;
            else if (pr.name == "red" || pr.name == "r") col[0] = (uint8_t)val; else if (pr.name == "green" || pr.name == "g") col[1] = (uint8_t)val;
            else if (pr.name == "blue" || pr.name == "b") col[2] = (uint8_t)val;
        }
        p.pos.insert(p.pos.end(), xyz, xyz + 3); p.rgb.insert(p.rgb.end(), col, col + 3);
    }
    return true;
}

bool loadCameras(const std::string& sfmJson, std::vector<Camera>& cams, std::string& err) {
    cams.clear();
    std::string t;
    if (!readAll(sfmJson, t)) { err = "Couldn't read the cameras"; return false; }
    Json::Value root; size_t i = 0;
    if (!Json::parse(t, i, root)) { err = "The cameras file is damaged"; return false; }
    std::map<std::string, std::pair<std::string, float>> viewOf;   // poseId -> photo, aspect
    std::map<std::string, float> aspectOf;
    if (const Json::Value* in = root.get("intrinsics"))
        for (const auto& v : in->a) { const double w = v.num("width"), h = v.num("height"); if (h > 0) aspectOf[v.str("intrinsicId")] = (float)(w / h); }
    if (const Json::Value* vs = root.get("views"))
        for (const auto& v : vs->a) viewOf[v.str("poseId")] = {v.str("path"), aspectOf.count(v.str("intrinsicId")) ? aspectOf[v.str("intrinsicId")] : 1.5f};
    if (const Json::Value* ps = root.get("poses"))
        for (const auto& p : ps->a) {
            const Json::Value* pose = p.get("pose");
            const Json::Value* tr = pose ? pose->get("transform") : nullptr;
            if (!tr) continue;
            const Json::Value* r = tr->get("rotation"); const Json::Value* c = tr->get("center");
            if (!r || !c || r->a.size() != 9 || c->a.size() != 3) continue;
            Camera cam;
            for (int k = 0; k < 9; ++k) cam.R[k] = (float)(r->a[k].type == Json::Value::STR ? atof(r->a[k].s.c_str()) : r->a[k].n);
            for (int k = 0; k < 3; ++k) cam.C[k] = (float)(c->a[k].type == Json::Value::STR ? atof(c->a[k].s.c_str()) : c->a[k].n);
            auto it = viewOf.find(p.str("poseId"));
            if (it != viewOf.end()) { cam.photo = it->second.first; cam.aspect = it->second.second; }
            cams.push_back(cam);
        }
    return true;
}

// ---------- exports ----------
static void outPos(const Mesh& m, size_t i, float scale, bool flipUp, float o[3]) {
    // flipUp: AliceVision's Y points down; most 3D apps want Y up
    o[0] = m.pos[i * 3] * scale;
    o[1] = (flipUp ? -m.pos[i * 3 + 1] : m.pos[i * 3 + 1]) * scale;
    o[2] = (flipUp ? -m.pos[i * 3 + 2] : m.pos[i * 3 + 2]) * scale;
}

bool saveStl(const Mesh& m, const std::string& path, float scale, bool flipUp, std::string& err) {
    FILE* f = openU8(path, "wb");
    if (!f) { err = "Couldn't write " + path; return false; }
    char head[80] = "Terminus - photos to 3D";
    fwrite(head, 1, 80, f);
    const uint32_t n = (uint32_t)m.triangles();
    fwrite(&n, 4, 1, f);
    for (size_t t = 0; t < n; ++t) {
        float p[3][3];
        for (int k = 0; k < 3; ++k) outPos(m, t * 3 + k, scale, flipUp, p[k]);
        const float u[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]}, v[3] = {p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2]};
        float nn[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        const float l = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
        if (l > 0) for (float& c : nn) c /= l;
        fwrite(nn, 4, 3, f); fwrite(p, 4, 9, f);
        const uint16_t attr = 0; fwrite(&attr, 2, 1, f);
    }
    const bool ok = !ferror(f);
    fclose(f);
    if (!ok) err = "Couldn't write " + path;
    return ok;
}

bool saveObj(const Mesh& m, const std::string& path, float scale, bool flipUp, std::string& err) {
    const fs::path p = fs::u8path(path);
    const std::string mtl = p.stem().u8string() + ".mtl";
    std::ofstream o(p);
    if (!o) { err = "Couldn't write " + path; return false; }
    o << "# Terminus - photos to 3D\nmtllib " << mtl << "\n";
    for (size_t i = 0; i < m.pos.size() / 3; ++i) { float q[3]; outPos(m, i, scale, flipUp, q); o << "v " << q[0] << ' ' << q[1] << ' ' << q[2] << '\n'; }
    for (size_t i = 0; i < m.uv.size() / 2; ++i) o << "vt " << m.uv[i * 2] << ' ' << m.uv[i * 2 + 1] << '\n';
    int cur = -1;
    for (size_t t = 0; t < m.triangles(); ++t) {
        if (m.matOf[t] != cur) { cur = m.matOf[t]; o << "usemtl " << m.mats[cur].name << '\n'; }
        o << "f " << t * 3 + 1 << '/' << t * 3 + 1 << ' ' << t * 3 + 2 << '/' << t * 3 + 2 << ' ' << t * 3 + 3 << '/' << t * 3 + 3 << '\n';
    }
    std::ofstream mo(p.parent_path() / mtl);
    std::error_code ec;
    for (const auto& mt : m.mats) {
        mo << "newmtl " << mt.name << "\nKd 1 1 1\n";
        if (!mt.texturePath.empty()) {
            const fs::path src = fs::u8path(mt.texturePath), dst = p.parent_path() / (p.stem().u8string() + "_" + src.filename().u8string());
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            mo << "map_Kd " << dst.filename().u8string() << '\n';
        }
    }
    return true;
}

bool saveGlb(const Mesh& m, const std::string& path, float scale, bool flipUp, std::string& err) {
    // one primitive per material, the PNG textures embedded as they are: one file holds everything
    std::string bin, json;
    auto pad4 = [](std::string& s, char c) { while (s.size() % 4) s += c; };
    struct View { size_t off, len; int target; };
    std::vector<View> views;
    std::string accessors, meshes = "{\"primitives\":[", images, textures, materials;
    int nAcc = 0;
    auto addView = [&](const void* d, size_t n, int target) { pad4(bin, '\0'); views.push_back({bin.size(), n, target}); bin.append((const char*)d, n); return (int)views.size() - 1; };
    int nTex = 0;
    std::vector<int> texOf(m.mats.size(), -1);
    for (size_t k = 0; k < m.mats.size(); ++k) {
        const auto& mt = m.mats[k];
        if (mt.texturePath.empty()) continue;
        std::string png;
        if (!readAll(mt.texturePath, png)) continue;
        const int v = addView(png.data(), png.size(), 0);
        images += std::string(nTex ? "," : "") + "{\"bufferView\":" + std::to_string(v) + ",\"mimeType\":\"image/png\"}";
        textures += std::string(nTex ? "," : "") + "{\"source\":" + std::to_string(nTex) + "}";
        texOf[k] = nTex++;
    }
    bool firstPrim = true;
    for (size_t k = 0; k < m.mats.size(); ++k) {
        std::vector<float> P, N, T;
        float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
        for (size_t t = 0; t < m.triangles(); ++t) {
            if (m.matOf[t] != (int)k) continue;
            for (int c = 0; c < 3; ++c) {
                const size_t i = t * 3 + c;
                float q[3]; outPos(m, i, scale, flipUp, q);
                P.insert(P.end(), q, q + 3);
                for (int a = 0; a < 3; ++a) { lo[a] = std::min(lo[a], q[a]); hi[a] = std::max(hi[a], q[a]); }
                const float sy = flipUp ? -1.0f : 1.0f;
                N.insert(N.end(), {m.nrm[i * 3], m.nrm[i * 3 + 1] * sy, m.nrm[i * 3 + 2] * sy});
                T.insert(T.end(), {m.uv[i * 2], 1 - m.uv[i * 2 + 1]});   // glTF's V runs down
            }
        }
        if (P.empty()) continue;
        const size_t n = P.size() / 3;
        const int vp = addView(P.data(), P.size() * 4, 34962), vn = addView(N.data(), N.size() * 4, 34962), vt = addView(T.data(), T.size() * 4, 34962);
        char a[512];
        snprintf(a, sizeof a, "%s{\"bufferView\":%d,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC3\",\"min\":[%g,%g,%g],\"max\":[%g,%g,%g]},"
                 "{\"bufferView\":%d,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC3\"},{\"bufferView\":%d,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC2\"}",
                 nAcc ? "," : "", vp, n, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], vn, n, vt, n);
        accessors += a;
        const int mi = (int)k;
        meshes += std::string(firstPrim ? "" : ",") + "{\"attributes\":{\"POSITION\":" + std::to_string(nAcc) + ",\"NORMAL\":" + std::to_string(nAcc + 1) +
                  ",\"TEXCOORD_0\":" + std::to_string(nAcc + 2) + "},\"material\":" + std::to_string(mi) + "}";
        nAcc += 3; firstPrim = false;
    }
    for (size_t k = 0; k < m.mats.size(); ++k) {
        materials += std::string(k ? "," : "") + "{\"name\":\"" + m.mats[k].name + "\",\"pbrMetallicRoughness\":{" +
                     (texOf[k] >= 0 ? "\"baseColorTexture\":{\"index\":" + std::to_string(texOf[k]) + "}," : std::string()) +
                     "\"metallicFactor\":0,\"roughnessFactor\":1}}";
    }
    meshes += "]}";
    std::string bv;
    for (size_t i = 0; i < views.size(); ++i)
        bv += std::string(i ? "," : "") + "{\"buffer\":0,\"byteOffset\":" + std::to_string(views[i].off) + ",\"byteLength\":" + std::to_string(views[i].len) +
              (views[i].target ? ",\"target\":" + std::to_string(views[i].target) : std::string()) + "}";
    pad4(bin, '\0');
    json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Terminus\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0,\"name\":\"model\"}],"
           "\"meshes\":[" + meshes + "],\"materials\":[" + materials + "],\"accessors\":[" + accessors + "],\"bufferViews\":[" + bv + "],"
           "\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]" +
           (nTex ? ",\"images\":[" + images + "],\"textures\":[" + textures + "]" : std::string()) + "}";
    pad4(json, ' ');
    FILE* f = openU8(path, "wb");
    if (!f) { err = "Couldn't write " + path; return false; }
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    u32(0x46546C67); u32(2); u32((uint32_t)(12 + 8 + json.size() + 8 + bin.size()));
    u32((uint32_t)json.size()); u32(0x4E4F534A); fwrite(json.data(), 1, json.size(), f);
    u32((uint32_t)bin.size()); u32(0x004E4942); fwrite(bin.data(), 1, bin.size(), f);
    const bool ok = !ferror(f);
    fclose(f);
    if (!ok) err = "Couldn't write " + path;
    return ok;
}
