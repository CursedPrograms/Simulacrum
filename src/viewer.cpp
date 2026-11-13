// Terminus - the 3D view (see viewer.h). OpenGL 3.3 core, its functions fetched from the driver through SDL.
#include "viewer.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>

// ---- the GL 3.3 functions this file uses
#ifndef APIENTRY
#define APIENTRY
#endif
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
#define GLF(ret, name, args) typedef ret(APIENTRY* PFN_##name) args; static PFN_##name name##_ = nullptr;
GLF(GLuint, glCreateShader, (GLenum))
GLF(void, glShaderSource, (GLuint, GLsizei, const GLchar* const*, const GLint*))
GLF(void, glCompileShader, (GLuint))
GLF(void, glGetShaderiv, (GLuint, GLenum, GLint*))
GLF(GLuint, glCreateProgram, (void))
GLF(void, glAttachShader, (GLuint, GLuint))
GLF(void, glLinkProgram, (GLuint))
GLF(void, glUseProgram, (GLuint))
GLF(GLint, glGetUniformLocation, (GLuint, const GLchar*))
GLF(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat*))
GLF(void, glUniform1i, (GLint, GLint))
GLF(void, glUniform1f, (GLint, GLfloat))
GLF(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat))
GLF(void, glGenVertexArrays, (GLsizei, GLuint*))
GLF(void, glBindVertexArray, (GLuint))
GLF(void, glDeleteVertexArrays, (GLsizei, const GLuint*))
GLF(void, glGenBuffers, (GLsizei, GLuint*))
GLF(void, glBindBuffer, (GLenum, GLuint))
GLF(void, glBufferData, (GLenum, GLsizeiptr, const void*, GLenum))
GLF(void, glDeleteBuffers, (GLsizei, const GLuint*))
GLF(void, glEnableVertexAttribArray, (GLuint))
GLF(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*))
GLF(void, glGenFramebuffers, (GLsizei, GLuint*))
GLF(void, glBindFramebuffer, (GLenum, GLuint))
GLF(void, glFramebufferTexture2D, (GLenum, GLenum, GLenum, GLuint, GLint))
GLF(void, glGenRenderbuffers, (GLsizei, GLuint*))
GLF(void, glBindRenderbuffer, (GLenum, GLuint))
GLF(void, glRenderbufferStorage, (GLenum, GLenum, GLsizei, GLsizei))
GLF(void, glFramebufferRenderbuffer, (GLenum, GLenum, GLenum, GLuint))
GLF(void, glGenerateMipmap, (GLenum))
GLF(void, glActiveTexture, (GLenum))
#undef GLF
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_TEXTURE0 0x84C0
#ifndef GL_PROGRAM_POINT_SIZE
#define GL_PROGRAM_POINT_SIZE 0x8642
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

namespace Viewer {

static Look lk;
Look& look() { return lk; }

static const Mesh* mesh = nullptr;
static const Points* points = nullptr;
static const std::vector<Camera>* cams = nullptr;
static bool meshDirty = false, pointsDirty = false, camsDirty = false;

static GLuint progMesh = 0, progPts = 0, progLine = 0;
struct Part { GLuint vao = 0, vbo[3] = {0, 0, 0}, tex = 0; int count = 0; };
static std::vector<Part> parts;
static Part ptsPart, camPart, gridPart;
static GLuint fbo = 0, fboTex = 0, fboDepth = 0; static int fbW = 0, fbH = 0;

// the camera: orbiting a target
static float yaw = 0.6f, pitch = 0.35f, dist = 5, target[3] = {0, 0, 0}, sceneR = 1;

static bool load() {
#define L(name) name##_ = (PFN_##name)SDL_GL_GetProcAddress(#name); if (!name##_) return false;
    L(glCreateShader) L(glShaderSource) L(glCompileShader) L(glGetShaderiv) L(glCreateProgram) L(glAttachShader) L(glLinkProgram)
    L(glUseProgram) L(glGetUniformLocation) L(glUniformMatrix4fv) L(glUniform1i) L(glUniform1f) L(glUniform3f) L(glGenVertexArrays)
    L(glBindVertexArray) L(glDeleteVertexArrays) L(glGenBuffers) L(glBindBuffer) L(glBufferData) L(glDeleteBuffers)
    L(glEnableVertexAttribArray) L(glVertexAttribPointer) L(glGenFramebuffers) L(glBindFramebuffer) L(glFramebufferTexture2D)
    L(glGenRenderbuffers) L(glBindRenderbuffer) L(glRenderbufferStorage) L(glFramebufferRenderbuffer) L(glGenerateMipmap) L(glActiveTexture)
#undef L
    return true;
}

static GLuint program(const char* vs, const char* fs) {
    auto sh = [](GLenum t, const char* src) { GLuint s = glCreateShader_(t); glShaderSource_(s, 1, &src, nullptr); glCompileShader_(s); return s; };
    GLuint p = glCreateProgram_();
    glAttachShader_(p, sh(GL_VERTEX_SHADER, vs)); glAttachShader_(p, sh(GL_FRAGMENT_SHADER, fs));
    glLinkProgram_(p);
    return p;
}

bool init() {
    if (!load()) return false;
    progMesh = program(R"(#version 330
layout(location=0) in vec3 p; layout(location=1) in vec3 n; layout(location=2) in vec2 t;
uniform mat4 mvp; uniform float flip; out vec3 N; out vec2 T;
void main() { vec3 q = vec3(p.x, p.y * flip, p.z * flip); N = vec3(n.x, n.y * flip, n.z * flip); T = t; gl_Position = mvp * vec4(q, 1); })",
                       R"(#version 330
in vec3 N; in vec2 T; uniform sampler2D tex; uniform int mode; uniform float light; uniform vec3 eye; out vec4 o;
void main() {
  vec3 n = normalize(N); if (!gl_FrontFacing) n = -n;
  float d = max(dot(n, normalize(vec3(0.4, 0.8, 0.5))), 0.0) * 0.75 + max(dot(n, normalize(eye)), 0.0) * 0.35;
  if (mode == 0) { vec3 c = texture(tex, T).rgb; o = vec4(c * mix(1.0, 0.35 + d, light), 1); }
  else if (mode == 3) o = vec4(0.0, 0.482, 0.718, 1);
  else { vec3 clay = vec3(0.78, 0.76, 0.74); o = vec4(clay * (0.25 + d), 1); }
})");
    progPts = program(R"(#version 330
layout(location=0) in vec3 p; layout(location=1) in vec3 c; uniform mat4 mvp; uniform float flip; uniform float size; out vec3 C;
void main() { C = c; gl_Position = mvp * vec4(p.x, p.y * flip, p.z * flip, 1); gl_PointSize = size; })",
                      R"(#version 330
in vec3 C; out vec4 o; void main() { o = vec4(C, 1); })");
    progLine = progPts;
    return progMesh && progPts;
}

void setMesh(const Mesh* m) { mesh = m; meshDirty = true; }
void setPoints(const Points* p) { points = p; pointsDirty = true; }
void setCameras(const std::vector<Camera>* c) { cams = c; camsDirty = true; }

static void freePart(Part& p) {
    if (p.vao) glDeleteVertexArrays_(1, &p.vao);
    glDeleteBuffers_(3, p.vbo);
    if (p.tex) glDeleteTextures(1, &p.tex);
    p = Part();
}

static void upload(Part& p, int attr, int comps, const void* data, size_t bytes, GLenum type = GL_FLOAT, bool norm = false) {
    if (!p.vao) glGenVertexArrays_(1, &p.vao);
    glBindVertexArray_(p.vao);
    glGenBuffers_(1, &p.vbo[attr]);
    glBindBuffer_(GL_ARRAY_BUFFER, p.vbo[attr]);
    glBufferData_(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, data, GL_STATIC_DRAW);
    glEnableVertexAttribArray_(attr);
    glVertexAttribPointer_(attr, comps, type, norm, 0, nullptr);
}

static void rebuild() {
    if (meshDirty) {
        for (auto& p : parts) freePart(p);
        parts.clear();
        if (mesh && !mesh->empty())
            for (size_t k = 0; k < mesh->mats.size(); ++k) {
                std::vector<float> P, N, T;
                for (size_t t = 0; t < mesh->triangles(); ++t) {
                    if (mesh->matOf[t] != (int)k) continue;
                    P.insert(P.end(), &mesh->pos[t * 9], &mesh->pos[t * 9] + 9);
                    N.insert(N.end(), &mesh->nrm[t * 9], &mesh->nrm[t * 9] + 9);
                    T.insert(T.end(), &mesh->uv[t * 6], &mesh->uv[t * 6] + 6);
                }
                if (P.empty()) continue;
                Part p;
                upload(p, 0, 3, P.data(), P.size() * 4); upload(p, 1, 3, N.data(), N.size() * 4); upload(p, 2, 2, T.data(), T.size() * 4);
                p.count = (int)(P.size() / 3);
                const Material& mt = mesh->mats[k];
                if (!mt.rgba.empty()) {
                    glGenTextures(1, &p.tex);
                    glBindTexture(GL_TEXTURE_2D, p.tex);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mt.w, mt.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, mt.rgba.data());
                    glGenerateMipmap_(GL_TEXTURE_2D);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                }
                parts.push_back(p);
            }
        meshDirty = false;
    }
    if (pointsDirty) {
        freePart(ptsPart);
        if (points && !points->empty()) {
            std::vector<float> c(points->rgb.size());
            for (size_t i = 0; i < c.size(); ++i) c[i] = points->rgb[i] / 255.0f;
            upload(ptsPart, 0, 3, points->pos.data(), points->pos.size() * 4);
            upload(ptsPart, 1, 3, c.data(), c.size() * 4);
            ptsPart.count = (int)(points->pos.size() / 3);
        }
        pointsDirty = false;
    }
    if (camsDirty) {
        freePart(camPart);
        if (cams && !cams->empty()) {
            // each camera: a little pyramid from its centre towards what it saw, pink
            std::vector<float> P, C;
            const float s = sceneR * 0.04f;
            for (const auto& c : *cams) {
                // camera axes in the world: rows of R
                const float* R = c.R;
                auto w = [&](float x, float y, float z, float* o) { for (int k = 0; k < 3; ++k) o[k] = c.C[k] + (R[k] * x + R[3 + k] * y + R[6 + k] * z) * s; };
                float o[3] = {c.C[0], c.C[1], c.C[2]}, q[4][3];
                const float a = c.aspect;
                w(-a, -1, 2, q[0]); w(a, -1, 2, q[1]); w(a, 1, 2, q[2]); w(-a, 1, 2, q[3]);
                for (int k = 0; k < 4; ++k) {
                    P.insert(P.end(), o, o + 3); P.insert(P.end(), q[k], q[k] + 3);
                    P.insert(P.end(), q[k], q[k] + 3); P.insert(P.end(), q[(k + 1) % 4], q[(k + 1) % 4] + 3);
                }
            }
            C.resize(P.size());
            for (size_t i = 0; i < C.size(); i += 3) { C[i] = 0.88f; C[i + 1] = 0.25f; C[i + 2] = 0.54f; }
            upload(camPart, 0, 3, P.data(), P.size() * 4); upload(camPart, 1, 3, C.data(), C.size() * 4);
            camPart.count = (int)(P.size() / 3);
        }
        camsDirty = false;
    }
}

void frameAll() {
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    bool any = false;
    auto add = [&](const float* p, size_t n) {
        // the middle 96% of the points, so a few stray ones don't push the view far out
        if (!n) return;
        for (int c = 0; c < 3; ++c) {
            std::vector<float> v(n);
            for (size_t i = 0; i < n; ++i) v[i] = p[i * 3 + c] * (c > 0 && lk.flipUp ? -1.0f : 1.0f);
            const size_t a = n / 50, b = n - 1 - n / 50;
            std::nth_element(v.begin(), v.begin() + a, v.end()); const float l = v[a];
            std::nth_element(v.begin(), v.begin() + b, v.end()); const float h = v[b];
            lo[c] = std::min(lo[c], l); hi[c] = std::max(hi[c], h);
        }
        any = true;
    };
    if (mesh && !mesh->empty()) add(mesh->pos.data(), mesh->pos.size() / 3);
    else if (points && !points->empty()) add(points->pos.data(), points->pos.size() / 3);
    if (!any) { target[0] = target[1] = target[2] = 0; sceneR = 1; dist = 5; return; }
    for (int c = 0; c < 3; ++c) target[c] = (lo[c] + hi[c]) / 2;
    sceneR = std::max(1e-3f, std::sqrt((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2])) / 2);
    dist = sceneR * 2.6f;
    camsDirty = true;
}

// ---- small matrix helpers (column major, as GL wants)
static void mul(const float* a, const float* b, float* o) {
    float r[16];
    for (int c = 0; c < 4; ++c) for (int rr = 0; rr < 4; ++rr) { float s = 0; for (int k = 0; k < 4; ++k) s += a[k * 4 + rr] * b[c * 4 + k]; r[c * 4 + rr] = s; }
    memcpy(o, r, sizeof r);
}

static void viewProj(int w, int h, float* mvp, float eye[3]) {
    const float cp = std::cos(pitch), sp = std::sin(pitch), cy = std::cos(yaw), sy = std::sin(yaw);
    const float e[3] = {target[0] + dist * cp * sy, target[1] + dist * sp, target[2] + dist * cp * cy};
    float f[3] = {target[0] - e[0], target[1] - e[1], target[2] - e[2]};
    float fl = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]); for (float& v : f) v /= fl;
    float s[3] = {f[1] * 0 - f[2] * 1, f[2] * 0 - f[0] * 0, f[0] * 1 - f[1] * 0};
    float sl = std::sqrt(s[0] * s[0] + s[1] * s[1] + s[2] * s[2]); for (float& v : s) v /= sl;
    const float u[3] = {s[1] * f[2] - s[2] * f[1], s[2] * f[0] - s[0] * f[2], s[0] * f[1] - s[1] * f[0]};
    const float V[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0,
                         -(s[0] * e[0] + s[1] * e[1] + s[2] * e[2]), -(u[0] * e[0] + u[1] * e[1] + u[2] * e[2]), f[0] * e[0] + f[1] * e[1] + f[2] * e[2], 1};
    const float n = std::max(dist * 0.01f, sceneR * 0.001f), fr = dist + sceneR * 20, t = 1.0f / std::tan(0.4f), a = (float)w / std::max(1, h);
    const float P[16] = {t / a, 0, 0, 0, 0, t, 0, 0, 0, 0, -(fr + n) / (fr - n), -1, 0, 0, -2 * fr * n / (fr - n), 0};
    mul(P, V, mvp);
    for (int k = 0; k < 3; ++k) eye[k] = e[k] - target[k];
}

unsigned render(int w, int h, bool hovered) {
    if (w < 8 || h < 8) return fboTex;
    rebuild();
    // the mouse
    ImGuiIO& io = ImGui::GetIO();
    if (hovered) {
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) { yaw -= io.MouseDelta.x * 0.008f; pitch = std::clamp(pitch + io.MouseDelta.y * 0.008f, -1.55f, 1.55f); }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
            const float k = dist * 0.0015f, cy = std::cos(yaw), sy = std::sin(yaw);
            target[0] -= io.MouseDelta.x * k * cy; target[2] += io.MouseDelta.x * k * sy; target[1] += io.MouseDelta.y * k;
        }
        if (io.MouseWheel != 0) dist = std::clamp(dist * std::pow(0.88f, io.MouseWheel), sceneR * 0.05f, sceneR * 50);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) frameAll();
    }
    if (w != fbW || h != fbH) {
        if (!fbo) { glGenFramebuffers_(1, &fbo); glGenTextures(1, &fboTex); glGenRenderbuffers_(1, &fboDepth); }
        glBindTexture(GL_TEXTURE_2D, fboTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindRenderbuffer_(GL_RENDERBUFFER, fboDepth);
        glRenderbufferStorage_(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
        glFramebufferRenderbuffer_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fboDepth);
        fbW = w; fbH = h;
    }
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glClearColor(0x14 / 255.0f, 0x14 / 255.0f, 0x14 / 255.0f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_PROGRAM_POINT_SIZE);
    float mvp[16], eye[3];
    viewProj(w, h, mvp, eye);
    const float flip = lk.flipUp ? -1.0f : 1.0f;

    // the floor grid, under everything
    if (lk.grid) {
        freePart(gridPart);
        std::vector<float> P, C;
        const float step = std::pow(10.0f, std::floor(std::log10(sceneR))) * 0.5f, y = target[1] - sceneR * 0.9f;
        for (int i = -10; i <= 10; ++i) {
            const float a = i * step, b = 10 * step;
            P.insert(P.end(), {target[0] + a, y, target[2] - b, target[0] + a, y, target[2] + b, target[0] - b, y, target[2] + a, target[0] + b, y, target[2] + a});
        }
        C.assign(P.size(), 0.23f);
        // (the grid is in view space up already: undo the flip the shader applies)
        for (size_t i = 0; i < P.size(); i += 3) { P[i + 1] *= flip; P[i + 2] *= flip; }
        upload(gridPart, 0, 3, P.data(), P.size() * 4); upload(gridPart, 1, 3, C.data(), C.size() * 4);
        gridPart.count = (int)(P.size() / 3);
        glUseProgram_(progLine);
        glUniformMatrix4fv_(glGetUniformLocation_(progLine, "mvp"), 1, GL_FALSE, mvp);
        glUniform1f_(glGetUniformLocation_(progLine, "flip"), flip);
        glUniform1f_(glGetUniformLocation_(progLine, "size"), 1);
        glBindVertexArray_(gridPart.vao);
        glDrawArrays(GL_LINES, 0, gridPart.count);
    }
    if (!parts.empty()) {
        glUseProgram_(progMesh);
        glUniformMatrix4fv_(glGetUniformLocation_(progMesh, "mvp"), 1, GL_FALSE, mvp);
        glUniform1f_(glGetUniformLocation_(progMesh, "flip"), flip);
        glUniform1f_(glGetUniformLocation_(progMesh, "light"), lk.light);
        glUniform3f_(glGetUniformLocation_(progMesh, "eye"), eye[0], eye[1], eye[2]);
        glUniform1i_(glGetUniformLocation_(progMesh, "tex"), 0);
        glActiveTexture_(GL_TEXTURE0);
        for (const auto& p : parts) {
            glUniform1i_(glGetUniformLocation_(progMesh, "mode"), lk.shading == 0 && p.tex ? 0 : 1);
            glBindTexture(GL_TEXTURE_2D, p.tex);
            glBindVertexArray_(p.vao);
            glDrawArrays(GL_TRIANGLES, 0, p.count);
            if (lk.shading == 2) {
                glUniform1i_(glGetUniformLocation_(progMesh, "mode"), 3);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glEnable(GL_POLYGON_OFFSET_LINE); glPolygonOffset(-1, -1);
                glDrawArrays(GL_TRIANGLES, 0, p.count);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glDisable(GL_POLYGON_OFFSET_LINE);
            }
        }
    }
    glUseProgram_(progPts);
    glUniformMatrix4fv_(glGetUniformLocation_(progPts, "mvp"), 1, GL_FALSE, mvp);
    glUniform1f_(glGetUniformLocation_(progPts, "flip"), flip);
    if (lk.points && ptsPart.count && parts.empty()) {
        glUniform1f_(glGetUniformLocation_(progPts, "size"), lk.pointSize);
        glBindVertexArray_(ptsPart.vao);
        glDrawArrays(GL_POINTS, 0, ptsPart.count);
    }
    if (lk.cameras && camPart.count) {
        glUniform1f_(glGetUniformLocation_(progPts, "size"), 1);
        glBindVertexArray_(camPart.vao);
        glDrawArrays(GL_LINES, 0, camPart.count);
    }
    glBindVertexArray_(0);
    glUseProgram_(0);
    glDisable(GL_DEPTH_TEST);
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    return fboTex;
}

void readPixels(std::vector<unsigned char>& rgba, int& w, int& h) {
    w = fbW; h = fbH;
    rgba.resize((size_t)w * h * 4);
    std::vector<unsigned char> raw(rgba.size());
    glBindFramebuffer_(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    for (int y = 0; y < h; ++y) memcpy(&rgba[(size_t)y * w * 4], &raw[(size_t)(h - 1 - y) * w * 4], (size_t)w * 4);
}

}
