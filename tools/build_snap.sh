#!/bin/sh
# compila una herramienta: tools/build_snap.sh items_snap
cd "$(dirname "$0")/.."; export _ZO_DOCTOR=0; SDL=/opt/homebrew/Cellar/sdl2/2.32.10
clang++ -std=c++17 -O2 tools/$1.cpp -I$SDL/include -I$SDL/include/SDL2 -L$SDL/lib -lSDL2 -Wl,-rpath,$SDL/lib -o tools/$1
