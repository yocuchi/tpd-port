#!/bin/sh
# Compila la versión de escritorio (macOS/Linux con SDL2): ./build_mac.sh  -> ./tetris-mac
# Necesita clang++ o g++ y SDL2 (brew install sdl2 / apt install libsdl2-dev).
cd "$(dirname "$0")" || exit 1
CXX="${CXX:-c++}"
exec "$CXX" -std=c++17 -O2 main_sdl.cpp -I"$(sdl2-config --prefix)/include" $(sdl2-config --cflags --libs) -o tetris-mac
