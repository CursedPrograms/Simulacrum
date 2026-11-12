// Terminus - running AliceVision's programs: no console windows, their output line by line, stoppable.
#pragma once
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace Proc {

// the AliceVision runtime Terminus ships with (bin\ and share\), next to Terminus.exe; empty if it's missing
std::string runtimeDir();
// set ALICEVISION_ROOT and friends, and put the runtime's bin first on PATH (once, at start)
void setupEnvironment();

// runs exe (a program in the runtime's bin, without .exe) with args; every output line goes to onLine.
// Returns the exit code, or -1 if it couldn't start; stop = true ends it early (exit code -2).
int run(const std::string& exe, const std::vector<std::string>& args, const std::string& workDir,
        const std::function<void(const std::string&)>& onLine, std::atomic<bool>& stop);

// an NVIDIA card with CUDA (the depth maps need one); name gets its name and memory, for the window
bool hasCuda(std::string& name);

// where Terminus keeps its projects: Documents\Terminus
std::string projectsDir();

}
