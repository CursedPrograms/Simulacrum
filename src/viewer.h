// Terminus - the 3D view: the model (textured, plain clay or wireframe), the matched points and the cameras that
// took the photos, turned with the mouse. Drawn into its own texture, shown inside the window.
#pragma once
#include "model.h"

namespace Viewer {

bool init();                                   // after the GL context exists
void setMesh(const Mesh* m);                   // nullptr: none (the mesh must stay alive while shown)
void setPoints(const Points* p);
void setCameras(const std::vector<Camera>* c);
void frameAll();                               // look at all of it

struct Look {
    int shading = 0;                           // 0 textured, 1 clay, 2 wireframe over clay
    bool points = true, cameras = true, grid = true;
    bool flipUp = false;                       // turn it over (the build already stands it upright, Y up)
    float pointSize = 2.0f;
    float light = 0.8f;                        // how much the light shapes the textured model (0: flat photo colours)
};
Look& look();

// draws into a w x h texture; returns it (for ImGui::Image). The mouse: drag turns, right-drag or middle-drag
// moves, wheel zooms; hovered tells it whether the mouse is over the view.
unsigned render(int w, int h, bool hovered);
// the last render, read back as RGBA (top row first), for a picture of the model
void readPixels(std::vector<unsigned char>& rgba, int& w, int& h);

}
