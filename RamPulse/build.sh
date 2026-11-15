#!/bin/bash
set -e
cd "$(dirname "$0")"; mkdir -p build dist
x86_64-w64-mingw32-windres src/app.rc -O coff -o build/app.res.o
x86_64-w64-mingw32-g++ -O2 -std=c++17 -Wall -Wno-unused-function -municode -mwindows -static -static-libgcc -static-libstdc++ \
  -D__USE_MINGW_ANSI_STDIO=1 -s src/main.cpp build/app.res.o -o dist/RamPulse.exe \
  -lgdiplus -lpsapi -lshell32 -ladvapi32 -lole32 -luser32 -lgdi32 -ldwmapi -lurlmon
ls -la dist/RamPulse.exe
