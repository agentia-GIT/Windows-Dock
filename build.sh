#!/usr/bin/env bash
# Compile Win-Dock avec Zig (cross-compilation depuis Linux/macOS/Windows).
#   pip install ziglang    puis    ./build.sh
set -e
cd "$(dirname "$0")"
mkdir -p build
FLAGS="-O2 -std=c++17 -static -s -Wl,--subsystem,windows src/main.cpp resources/app.rc"
LIBS="-lgdiplus -lgdi32 -luser32 -lshell32 -lole32 -luuid -ldwmapi -lcomdlg32 -lshlwapi -ladvapi32 -luxtheme -lwinmm -lcomctl32 -lwinhttp"
python3 -m ziglang c++ -target x86_64-windows-gnu $FLAGS -o build/WinDock_x64.exe $LIBS
python3 -m ziglang c++ -target x86-windows-gnu    $FLAGS -o build/WinDock_x86.exe $LIBS
ls -la build
