// Simulacrum - the splash: a carved pillar (Simulacrum's own mark) as a cloud of measured points, the cameras that
// photographed it ringed round it in pink, the name, the version, the credits and what's loading. Atelier greys and
// the Cursed pink, solid; drawn with ImGui's draw list so it's sharp at any size.
#include "splash.h"

#include <imgui.h>

#include <cfloat>
#include <cmath>

#ifndef SIMULACRUM_VERSION
#define SIMULACRUM_VERSION "1.0.0"
#endif

namespace Splash {

static ImFont *titleFont = nullptr, *smallFont = nullptr;
void setFonts(ImFont* title, ImFont* small) { titleFont = title; smallFont = small; }

static const ImU32 BG = IM_COL32(0x1a, 0x1a, 0x1a, 255), LINE = IM_COL32(0x33, 0x33, 0x33, 255),
                   TEXT = IM_COL32(0xd6, 0xd6, 0xd6, 255), DIM = IM_COL32(0x9a, 0x9a, 0x9a, 255),
                   PINK = IM_COL32(0x00, 0x7b, 0xb7, 255), DEEP = IM_COL32(0x00, 0x40, 0x67, 255);

static void text(ImDrawList* dl, ImFont* f, float size, ImVec2 p, ImU32 col, const char* s) { dl->AddText(f ? f : ImGui::GetFont(), size, p, col, s); }
static float textWidth(ImFont* f, float size, const char* s) { return (f ? f : ImGui::GetFont())->CalcTextSizeA(size, FLT_MAX, 0, s).x; }

static float hash(int i, int k) { unsigned h = (unsigned)i * 0x8da6b343u ^ (unsigned)k * 0xd8163841u; h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return (h & 0xffffff) / 16777215.0f; }

void draw(ImDrawList* dl, float x, float y, const char* status, float progress) {
    const ImVec2 o(x, y), end(x + W, y + H);
    dl->PushClipRect(o, end, true);
    dl->AddRectFilled(o, end, BG);

    // the pillar: a rounded-top pillar (the thing being copied), as points on its surface, turned a little
    const float cx = x + 470, cy = y + 205, yaw = 0.5f;
    for (int i = 0; i < 1500; ++i) {
        const float u = hash(i, 1), v = hash(i, 2);
        const float a = u * 6.2832f;
        float px, py, pz;
        if (v < 0.78f) { const float hgt = v / 0.78f; px = std::cos(a) * 52; pz = std::sin(a) * 52; py = 95 - hgt * 170; }       // the shaft
        else { const float t = (v - 0.78f) / 0.22f * 1.5708f; px = std::cos(a) * 52 * std::cos(t); pz = std::sin(a) * 52 * std::cos(t); py = -75 - std::sin(t) * 52; }   // the dome
        const float rx = px * std::cos(yaw) + pz * std::sin(yaw), rz = -px * std::sin(yaw) + pz * std::cos(yaw);
        const float s = 300 / (300 + rz);
        const float shade = 0.45f + 0.55f * (0.5f - 0.5f * rz / 52);
        const int g = (int)(0x50 + 0x80 * shade);
        dl->AddRectFilled(ImVec2(cx + rx * s, cy + py * s), ImVec2(cx + rx * s + 2, cy + py * s + 2), IM_COL32(g, g, g, 255));
    }
    // the plinth it stands on
    dl->AddLine(ImVec2(cx - 90, cy + 112), ImVec2(cx + 90, cy + 112), LINE, 2);
    // the cameras that photographed it: little frustums on a ring, looking in
    for (int k = 0; k < 9; ++k) {
        const float a = k / 9.0f * 6.2832f + 0.3f;
        const float rx = std::cos(a) * 130, rz = std::sin(a) * 130 * 0.35f;
        if (rz < -10) continue;   // the far side is hidden behind the pillar
        const ImVec2 c(cx + rx, cy + 20 + rz);
        const float dx = -std::cos(a), len = 16;
        const ImVec2 tip(c.x + dx * len, c.y - 2);
        dl->AddTriangle(c, ImVec2(tip.x - 0, tip.y - 9), ImVec2(tip.x, tip.y + 9), k % 3 == 0 ? PINK : DEEP, 1.6f);
        dl->AddCircleFilled(c, 2.5f, PINK);
    }

    // the name: "Termi" in the text colour, "nus" in pink
    const float ts = 50, tx = x + 40, ty = y + 92;
    text(dl, titleFont, ts, ImVec2(tx, ty), TEXT, "Termi");
    text(dl, titleFont, ts, ImVec2(tx + textWidth(titleFont, ts, "Termi"), ty), PINK, "nus");
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 62), DIM, "Version " SIMULACRUM_VERSION);
    text(dl, nullptr, 16, ImVec2(tx + 2, ty + 84), DIM, "Photos to 3D by Cursed Entertainment");

    const float cs = 13, cry = y + 306;
    text(dl, smallFont, cs, ImVec2(tx, cry), DIM, "Photogrammetry by AliceVision (MPL-2.0), as Meshroom runs it.");
    text(dl, smallFont, cs, ImVec2(tx, cry + 17), DIM, "Built with Dear ImGui, SDL2 and stb.");
    text(dl, smallFont, cs, ImVec2(tx, cry + 34), DIM, "Farica\xe2\x98\x86Kimora \xc2\xa9 Cursed Entertainment");

    if (status) text(dl, nullptr, 16, ImVec2(tx, y + 368), TEXT, status);
    if (progress >= 0) {
        const float p = progress > 1 ? 1 : progress;
        dl->AddRectFilled(ImVec2(x, end.y - 3), end, LINE);
        dl->AddRectFilled(ImVec2(x, end.y - 3), ImVec2(x + W * p, end.y), PINK);
    }
    dl->AddRect(o, end, LINE);
    dl->PopClipRect();
}

}
