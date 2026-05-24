#!/bin/bash
# Build all Arco dependencies using Git Bash on Windows.
# Run from a VS x64 Developer Command Prompt (so cl/nmake are on PATH),
# or set MSVC_VER, WINSDK_VER, and VS_ROOT before running.
#
# Set PROJECTS to the parent directory containing arco, o2, portaudio, etc.
# Defaults to the parent of this script's directory.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
: "${PROJECTS:="$(dirname "$SCRIPT_DIR")"}"

# If cl.exe is not on PATH, try to set up the VS environment.
# You may need to adjust these for your VS installation.
if ! command -v cl &>/dev/null; then
    : "${MSVC_VER:=14.50.35717}"
    : "${WINSDK_VER:=10.0.26100.0}"
    : "${VS_ROOT:=/c/Program Files/Microsoft Visual Studio/2022/Community/VC}"
    export PATH="$VS_ROOT/Tools/MSVC/$MSVC_VER/bin/Hostx64/x64:/c/Program Files (x86)/Windows Kits/10/bin/$WINSDK_VER/x64:/c/Program Files/CMake/bin:/c/Program Files/Faust/bin:$PATH"
    export INCLUDE="$(cygpath -w "$VS_ROOT")\Tools\MSVC\$MSVC_VER\include;C:\Program Files (x86)\Windows Kits\10\Include\$WINSDK_VER\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\$WINSDK_VER\shared;C:\Program Files (x86)\Windows Kits\10\Include\$WINSDK_VER\um;C:\Program Files (x86)\Windows Kits\10\Include\$WINSDK_VER\winrt"
    export LIB="$(cygpath -w "$VS_ROOT")\Tools\MSVC\$MSVC_VER\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\$WINSDK_VER\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\Lib\$WINSDK_VER\um\x64"
fi

echo "=== Verifying tools ==="
cl 2>&1 | head -1
cmake --version | head -1
faust --version 2>&1 | head -1
python --version

# Step 1: PortAudio
echo ""
echo "=== Step 1: PortAudio ==="
if [ ! -d "$PROJECTS/portaudio" ]; then
    cd "$PROJECTS"
    git clone https://github.com/PortAudio/portaudio.git
fi

mkdir -p "$PROJECTS/portaudio/build"
cd "$PROJECTS/portaudio/build"
if [ ! -f "portaudio_static.lib" ]; then
    echo "--- Configuring PortAudio Release ---"
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPA_BUILD_SHARED_LIBS=OFF
    echo "--- Building PortAudio Release ---"
    nmake
fi
echo "[OK] PortAudio Release"

mkdir -p "$PROJECTS/portaudio/build-debug"
cd "$PROJECTS/portaudio/build-debug"
if [ ! -f "portaudio_static.lib" ]; then
    echo "--- Building PortAudio Debug ---"
    cmake "$PROJECTS/portaudio" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DPA_BUILD_SHARED_LIBS=OFF
    nmake
fi
echo "[OK] PortAudio Debug"

# Step 2: libsndfile
echo ""
echo "=== Step 2: libsndfile ==="
if [ ! -d "$PROJECTS/libsndfile" ]; then
    cd "$PROJECTS"
    git clone https://github.com/libsndfile/libsndfile.git
fi

mkdir -p "$PROJECTS/libsndfile/build"
cd "$PROJECTS/libsndfile/build"
if [ ! -f "sndfile.lib" ]; then
    echo "--- Configuring libsndfile Release ---"
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
        -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=OFF -DINSTALL_MANPAGES=OFF
    echo "--- Building libsndfile Release ---"
    nmake
fi
echo "[OK] libsndfile Release"

mkdir -p "$PROJECTS/libsndfile/build-debug"
cd "$PROJECTS/libsndfile/build-debug"
if [ ! -f "sndfile.lib" ]; then
    echo "--- Building libsndfile Debug ---"
    cmake "$PROJECTS/libsndfile" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF \
        -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=OFF -DINSTALL_MANPAGES=OFF
    nmake
fi
echo "[OK] libsndfile Debug"

# Step 3: PDCurses
echo ""
echo "=== Step 3: PDCurses ==="
if [ ! -d "$PROJECTS/pdcurses" ]; then
    cd "$PROJECTS"
    git clone https://github.com/wmcbrine/PDCurses.git pdcurses
fi

cd "$PROJECTS/pdcurses/wincon"
if [ ! -f "pdcurses.lib" ]; then
    echo "--- Building PDCurses ---"
    nmake /f Makefile.vc
fi
echo "[OK] PDCurses"

# Step 4: O2
echo ""
echo "=== Step 4: O2 ==="
mkdir -p "$PROJECTS/o2/build"
cd "$PROJECTS/o2/build"
if [ ! -f "o2_static.lib" ]; then
    echo "--- Configuring O2 Release ---"
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release \
        -DTESTS_BUILD=OFF -DBUILD_SHARED_LIBS=OFF
    echo "--- Building O2 Release ---"
    nmake
fi
echo "[OK] O2 Release"

mkdir -p "$PROJECTS/o2/build-debug"
cd "$PROJECTS/o2/build-debug"
if [ ! -f "o2_static.lib" ]; then
    echo "--- Building O2 Debug ---"
    cmake "$PROJECTS/o2" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug \
        -DTESTS_BUILD=OFF -DBUILD_SHARED_LIBS=OFF
    nmake
fi
echo "[OK] O2 Debug"

# Step 5: o2litepy
echo ""
echo "=== Step 5: o2litepy ==="
pip install -e "$PROJECTS/o2/o2litepy/py3pkg" 2>/dev/null || \
    python -m pip install -e "$PROJECTS/o2/o2litepy/py3pkg"
echo "[OK] o2litepy"

# Summary
echo ""
echo "=== SUMMARY ==="
for f in \
    "$PROJECTS/portaudio/build/portaudio_static.lib" \
    "$PROJECTS/portaudio/build-debug/portaudio_static.lib" \
    "$PROJECTS/libsndfile/build/sndfile.lib" \
    "$PROJECTS/libsndfile/build-debug/sndfile.lib" \
    "$PROJECTS/pdcurses/wincon/pdcurses.lib" \
    "$PROJECTS/o2/build/o2_static.lib" \
    "$PROJECTS/o2/build-debug/o2_static.lib"; do
    if [ -f "$f" ]; then
        echo "[OK] $f"
    else
        echo "[MISS] $f"
    fi
done
