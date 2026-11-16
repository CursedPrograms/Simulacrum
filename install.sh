#!/usr/bin/env bash
# Installs Simulacrum for this user on Linux: ~/.local/share/Simulacrum, ~/.local/bin/simulacrum and a menu entry.
set -e
cd "$(dirname "$0")"
DEST="$HOME/.local/share/Simulacrum"
mkdir -p "$DEST" "$HOME/.local/bin" "$HOME/.local/share/applications"
cp -r build/Simulacrum build/assets "$DEST/"
[ -d runtime/alicevision ] && cp -r runtime/alicevision "$DEST/"
printf '#!/bin/sh\nexec "%s/Simulacrum" "$@"\n' "$DEST" > "$HOME/.local/bin/simulacrum"
chmod +x "$HOME/.local/bin/simulacrum"
printf '[Desktop Entry]\nName=Simulacrum\nComment=Photos to a textured 3D model\nExec=%s/.local/bin/simulacrum %%F\nIcon=%s/assets/logo.png\nType=Application\nCategories=Graphics;3DGraphics;\n' "$HOME" "$DEST" > "$HOME/.local/share/applications/simulacrum.desktop"
echo "Installed: $DEST (run: simulacrum)"
