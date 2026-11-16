"""Simulacrum - assembles runtime/alicevision: just the AliceVision programs Simulacrum runs, the DLLs they load (followed
through each DLL's own imports) and the data they read, out of a Meshroom 2025.1 folder (its aliceVision\\).

    python tools/make_runtime.py MESHROOM_ALICEVISION_FOLDER [OUT]

Meshroom 2025.1 for Windows: https://zenodo.org/records/16887472 (its aliceVision\\ folder is all this needs).
Needs Visual Studio's dumpbin (to read the imports)."""
import glob
import os
import shutil
import subprocess
import sys

PROGRAMS = ["cameraInit", "featureExtraction", "imageMatching", "featureMatching", "incrementalSfM", "prepareDenseScene",
            "depthMapEstimation", "depthMapFiltering", "meshing", "meshFiltering", "meshDecimate", "texturing", "convertSfMFormat"]
SHARE = ["cameraSensors.db", "vlfeat_K80L3.SIFT.tree", "config.ocio", "luts", "COPYING.md", "LICENSE-MPL2.md", "LICENSE-MIT-libmv.md", "CONTRIBUTORS.md"]


def dumpbin():
    found = glob.glob(r"C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe")
    if not found:
        sys.exit("dumpbin.exe not found: install Visual Studio 2022 with C++")
    return found[0]


def imports(db, path):
    out = subprocess.run([db, "/dependents", path], capture_output=True, text=True).stdout
    names, on = [], False
    for line in out.splitlines():
        s = line.strip()
        if s.startswith("Image has the following"):
            on = True
            continue
        if on and s.lower().endswith(".dll"):
            names.append(s.lower())
        elif on and names and not s:
            break
    return names


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "runtime", "alicevision-min")
    db = dumpbin()
    local = {}
    for sub in ("bin", "lib"):
        d = os.path.join(src, sub)
        for f in os.listdir(d):
            if f.lower().endswith(".dll"):
                local[f.lower()] = os.path.join(d, f)
    exes = [os.path.join(src, "bin", f"aliceVision_{p}.exe") for p in PROGRAMS]
    need, todo, seen = set(), list(exes), set()
    while todo:
        p = todo.pop()
        if p.lower() in seen:
            continue
        seen.add(p.lower())
        for d in imports(db, p):
            if d in local and d not in need:
                need.add(d)
                todo.append(local[d])
    if os.path.exists(out):
        shutil.rmtree(out)
    os.makedirs(os.path.join(out, "bin"))
    for e in exes:
        shutil.copy2(e, os.path.join(out, "bin"))
    for d in sorted(need):
        shutil.copy2(local[d], os.path.join(out, "bin"))
    share = os.path.join(src, "share", "aliceVision")
    os.makedirs(os.path.join(out, "share", "aliceVision"))
    for s in SHARE:
        p = os.path.join(share, s)
        if os.path.isdir(p):
            shutil.copytree(p, os.path.join(out, "share", "aliceVision", s))
        elif os.path.exists(p):
            shutil.copy2(p, os.path.join(out, "share", "aliceVision"))
    size = sum(os.path.getsize(os.path.join(r, f)) for r, _, fs in os.walk(out) for f in fs)
    print(f"{len(exes)} programs, {len(need)} DLLs, {size / 1e6:.0f} MB in {out}")


if __name__ == "__main__":
    main()
