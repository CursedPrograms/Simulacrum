#!/usr/bin/env bash
# Installs Terminus for this user on Linux: ~/.local/share/Terminus, ~/.local/bin/terminus and a menu entry.
set -e
cd "$(dirname "$0")"
DEST="$HOME/.local/share/Terminus"
mkdir -p "$DEST" "$HOME/.local/bin" "$HOME/.local/share/applications"
cp -r build/Terminus build/assets "$DEST/"
[ -d runtime/alicevision ] && cp -r runtime/alicevision "$DEST/"
printf '#!/bin/sh\nexec "%s/Terminus" "$@"\n' "$DEST" > "$HOME/.local/bin/terminus"
chmod +x "$HOME/.local/bin/terminus"
printf '[Desktop Entry]\nName=Terminus\nComment=Photos to a textured 3D model\nExec=%s/.local/bin/terminus %%F\nIcon=%s/assets/logo.png\nType=Application\nCategories=Graphics;3DGraphics;\n' "$HOME" "$DEST" > "$HOME/.local/share/applications/terminus.desktop"
echo "Installed: $DEST (run: terminus)"
