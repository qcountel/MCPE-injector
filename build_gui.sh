#!/usr/bin/env bash
# build_gui.sh — MCPE UWP DLL Injector  (MSYS2 / MinGW 64-bit)

export PATH=/c/msys64/mingw64/bin:/c/msys64/usr/bin:$PATH
cd "$(dirname "$0")"

echo "==> windres..."
windres app.rc -o res.o || { echo "[!] windres failed"; exit 1; }

echo "==> g++..."
g++ -o mcpe_injector_gui.exe main.cpp res.o \
    -std=c++17 -O2 -m64 \
    -mwindows -municode \
    -DUNICODE -D_UNICODE \
    -static-libgcc \
    -Wl,-Bstatic -lstdc++ -lpthread \
    -Wl,-Bdynamic \
    -lkernel32 -luser32 -lgdi32 -ladvapi32 -lcomdlg32 \
    -ldwmapi -luxtheme -lcomctl32 \
    || { echo "[!] g++ failed"; exit 1; }

echo ""
echo "[+] OK: mcpe_injector_gui.exe"
ls -lh mcpe_injector_gui.exe
