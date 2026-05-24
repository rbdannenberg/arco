@echo off
REM ============================================================
REM  Arco Windows Development Environment Setup
REM  Run from a VS Developer Command Prompt (x64)
REM ============================================================
REM
REM  Prerequisites you must install manually BEFORE running this:
REM
REM  1. Visual Studio with C++ Desktop workload
REM     (Community, Professional, or Enterprise edition)
REM
REM  2. Bonjour SDK from Apple (or softpedia mirror)
REM     Install to default: "C:\Program Files\Bonjour SDK"
REM     Required by O2 for Zeroconf/mDNS discovery on Windows.
REM
REM  After those are in place, open a VS x64 Developer Command Prompt
REM  and run this script from the arco directory:
REM     setup_arco_win.bat
REM
REM  Optional environment variables:
REM     PROJECTS  — parent directory for arco, o2, portaudio, etc.
REM                 (defaults to parent of this script's directory)
REM ============================================================

setlocal enabledelayedexpansion

if not defined PROJECTS set PROJECTS=%~dp0..
set ARCO=%~dp0
REM Remove trailing backslash
set ARCO=%ARCO:~0,-1%

REM --- Verify we're in a VS Developer environment ---
where cl >nul 2>&1
if errorlevel 1 (
    echo ERROR: cl.exe not found. Run this from a VS x64 Developer Command Prompt.
    exit /b 1
)
echo [OK] Visual Studio C++ compiler found.

where nmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: nmake not found. Run this from a VS x64 Developer Command Prompt.
    exit /b 1
)
echo [OK] nmake found.

REM --- Check Python ---
where python >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found on PATH.
    exit /b 1
)
echo [OK] Python found.

REM ============================================================
REM  STEP 1: Install CMake (if missing)
REM ============================================================
where cmake >nul 2>&1
if errorlevel 1 (
    echo.
    echo --- Installing CMake via winget ---
    winget install Kitware.CMake --accept-package-agreements --accept-source-agreements
    if errorlevel 1 (
        echo ERROR: CMake install failed. Install manually from https://cmake.org/download/
        exit /b 1
    )
    echo NOTE: You may need to restart your terminal for cmake to be on PATH.
    echo       After restart, re-run this script.
    exit /b 0
) else (
    echo [OK] CMake found.
)

REM ============================================================
REM  STEP 2: Install FAUST (if missing)
REM ============================================================
where faust >nul 2>&1
if errorlevel 1 (
    echo.
    echo --- FAUST compiler not found ---
    echo Please install FAUST manually:
    echo   1. Download the Windows installer from:
    echo      https://github.com/grame-cncm/faust/releases
    echo      (look for Faust-X.XX.XX-win64.exe)
    echo   2. Install it and note the path (e.g. C:\Program Files\Faust)
    echo   3. Add the bin directory to your PATH
    echo   4. Re-run this script
    echo.
    echo After install, verify with: faust --version
    pause
    exit /b 1
) else (
    echo [OK] FAUST compiler found.
)

REM ============================================================
REM  STEP 3: Check Bonjour SDK
REM ============================================================
if not exist "C:\Program Files\Bonjour SDK\Include\dns_sd.h" (
    echo.
    echo --- Bonjour SDK not found ---
    echo O2 requires the Bonjour SDK for discovery on Windows.
    echo Install it to "C:\Program Files\Bonjour SDK"
    echo Then re-run this script.
    pause
    exit /b 1
) else (
    echo [OK] Bonjour SDK found.
)

REM ============================================================
REM  STEP 4: Clone PortAudio (if missing)
REM ============================================================
if not exist "%PROJECTS%\portaudio" (
    echo.
    echo --- Cloning PortAudio ---
    cd /d %PROJECTS%
    git clone https://github.com/PortAudio/portaudio.git
    if errorlevel 1 (
        echo ERROR: Failed to clone PortAudio.
        exit /b 1
    )
) else (
    echo [OK] PortAudio source found.
)

REM ============================================================
REM  STEP 5: Build PortAudio
REM ============================================================
if not exist "%PROJECTS%\portaudio\build" (
    echo.
    echo --- Building PortAudio ---
    mkdir "%PROJECTS%\portaudio\build"
)
cd /d "%PROJECTS%\portaudio\build"
if not exist "Release\portaudio_static_x64.lib" (
    if not exist "Release\portaudio_static.lib" (
        echo --- Configuring PortAudio ---
        cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPA_BUILD_SHARED_LIBS=OFF
        if errorlevel 1 (
            echo ERROR: PortAudio CMake configure failed.
            exit /b 1
        )
        echo --- Compiling PortAudio (Release) ---
        nmake
        if errorlevel 1 (
            echo ERROR: PortAudio build failed.
            exit /b 1
        )
    )
)
echo [OK] PortAudio built.

REM Also build Debug if not present
if not exist "%PROJECTS%\portaudio\build-debug" (
    mkdir "%PROJECTS%\portaudio\build-debug"
)
cd /d "%PROJECTS%\portaudio\build-debug"
if not exist "portaudio_static.lib" (
    echo --- Building PortAudio (Debug) ---
    cmake "%PROJECTS%\portaudio" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DPA_BUILD_SHARED_LIBS=OFF
    nmake
)
echo [OK] PortAudio Debug built.

REM ============================================================
REM  STEP 6: Build libsndfile
REM ============================================================
if not exist "%PROJECTS%\libsndfile" (
    echo.
    echo --- Cloning libsndfile ---
    cd /d %PROJECTS%
    git clone https://github.com/libsndfile/libsndfile.git
)
if not exist "%PROJECTS%\libsndfile\build" (
    mkdir "%PROJECTS%\libsndfile\build"
)
cd /d "%PROJECTS%\libsndfile\build"
if not exist "Release\sndfile.lib" (
    if not exist "sndfile.lib" (
        echo --- Configuring libsndfile ---
        cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
            -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF ^
            -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=OFF ^
            -DINSTALL_MANPAGES=OFF
        if errorlevel 1 (
            echo ERROR: libsndfile CMake configure failed.
            exit /b 1
        )
        echo --- Compiling libsndfile (Release) ---
        nmake
        if errorlevel 1 (
            echo ERROR: libsndfile build failed.
            exit /b 1
        )
    )
)
echo [OK] libsndfile built.

REM Debug build
if not exist "%PROJECTS%\libsndfile\build-debug" (
    mkdir "%PROJECTS%\libsndfile\build-debug"
)
cd /d "%PROJECTS%\libsndfile\build-debug"
if not exist "sndfile.lib" (
    echo --- Building libsndfile (Debug) ---
    cmake "%PROJECTS%\libsndfile" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ^
        -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF ^
        -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=OFF ^
        -DINSTALL_MANPAGES=OFF
    nmake
)
echo [OK] libsndfile Debug built.

REM ============================================================
REM  STEP 7: Build PDCurses (for the server UI)
REM ============================================================
if not exist "%PROJECTS%\pdcurses" (
    echo.
    echo --- Cloning PDCurses ---
    cd /d %PROJECTS%
    git clone https://github.com/wmcbrine/PDCurses.git pdcurses
)
if not exist "%PROJECTS%\pdcurses\wincon\pdcurses.lib" (
    echo --- Building PDCurses (wincon) ---
    cd /d "%PROJECTS%\pdcurses\wincon"
    nmake /f Makefile.vc
    if errorlevel 1 (
        echo ERROR: PDCurses build failed.
        exit /b 1
    )
)
echo [OK] PDCurses built.

REM ============================================================
REM  STEP 8: Build O2
REM ============================================================
if not exist "%PROJECTS%\o2\build" (
    mkdir "%PROJECTS%\o2\build"
)
cd /d "%PROJECTS%\o2\build"
if not exist "o2_static.lib" (
    echo.
    echo --- Configuring O2 ---
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release ^
        -DTESTS_BUILD=OFF -DBUILD_SHARED_LIBS=OFF
    if errorlevel 1 (
        echo ERROR: O2 CMake configure failed. Check Bonjour SDK install.
        exit /b 1
    )
    echo --- Compiling O2 (Release) ---
    nmake
    if errorlevel 1 (
        echo ERROR: O2 build failed.
        exit /b 1
    )
)
echo [OK] O2 built.

REM Debug build
if not exist "%PROJECTS%\o2\build-debug" (
    mkdir "%PROJECTS%\o2\build-debug"
)
cd /d "%PROJECTS%\o2\build-debug"
if not exist "o2_static.lib" (
    echo --- Building O2 (Debug) ---
    cmake "%PROJECTS%\o2" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ^
        -DTESTS_BUILD=OFF -DBUILD_SHARED_LIBS=OFF
    nmake
)
echo [OK] O2 Debug built.

REM ============================================================
REM  STEP 9: Install o2litepy for PyArco
REM ============================================================
echo.
echo --- Installing o2litepy ---
pip install -e "%PROJECTS%\o2\o2litepy\py3pkg" >nul 2>&1
if errorlevel 1 (
    python -m pip install -e "%PROJECTS%\o2\o2litepy\py3pkg"
)
echo [OK] o2litepy installed.

REM ============================================================
REM  STEP 10: Generate libraries.txt
REM ============================================================
echo.
echo --- Generating apps\common\libraries.txt ---

REM Find the actual library files
set PA_RELEASE_LIB=
for %%f in (
    "%PROJECTS%\portaudio\build\portaudio_static_x64.lib"
    "%PROJECTS%\portaudio\build\portaudio_static.lib"
    "%PROJECTS%\portaudio\build\portaudio.lib"
) do (
    if exist %%f set PA_RELEASE_LIB=%%~f
)
set PA_DEBUG_LIB=
for %%f in (
    "%PROJECTS%\portaudio\build-debug\portaudio_static_x64.lib"
    "%PROJECTS%\portaudio\build-debug\portaudio_static.lib"
    "%PROJECTS%\portaudio\build-debug\portaudio.lib"
) do (
    if exist %%f set PA_DEBUG_LIB=%%~f
)

set SNDFILE_RELEASE_LIB=
for %%f in (
    "%PROJECTS%\libsndfile\build\sndfile.lib"
    "%PROJECTS%\libsndfile\build\Release\sndfile.lib"
) do (
    if exist %%f set SNDFILE_RELEASE_LIB=%%~f
)
set SNDFILE_DEBUG_LIB=
for %%f in (
    "%PROJECTS%\libsndfile\build-debug\sndfile.lib"
    "%PROJECTS%\libsndfile\build-debug\Debug\sndfile.lib"
) do (
    if exist %%f set SNDFILE_DEBUG_LIB=%%~f
)

REM Find FAUST path
for /f "tokens=*" %%i in ('where faust 2^>nul') do set FAUST_EXE=%%i
if defined FAUST_EXE (
    for %%i in ("%FAUST_EXE%") do set FAUST_BIN_DIR=%%~dpi
    REM Remove trailing backslash
    set FAUST_BIN_DIR=!FAUST_BIN_DIR:~0,-1!
)

(
echo # CMake file to define local libraries - auto-generated by setup_arco_win.bat
echo # Machine-specific paths for Windows build on %COMPUTERNAME%
echo # Generated: %DATE% %TIME%
echo.
echo # --- libsndfile ---
echo set(SNDFILE_INCL "%PROJECTS:\=/%/libsndfile/include" CACHE
echo         PATH "Main libsndfile include directory" FORCE^)
echo set(SNDFILE_OPT_LIB "%SNDFILE_RELEASE_LIB:\=/%" CACHE
echo         FILEPATH "libsndfile library - optimized version" FORCE^)
echo set(SNDFILE_DBG_LIB "%SNDFILE_DEBUG_LIB:\=/%" CACHE
echo         FILEPATH "libsndfile library - debug version" FORCE^)
echo.
echo # --- PortAudio ---
echo set(PA_INCL "%PROJECTS:\=/%/portaudio/include" CACHE
echo         PATH "Main PortAudio include directory" FORCE^)
echo set(PA_OPT_LIB "%PA_RELEASE_LIB:\=/%" CACHE
echo         FILEPATH "PortAudio library - optimized version" FORCE^)
echo set(PA_DBG_LIB "%PA_DEBUG_LIB:\=/%" CACHE
echo         FILEPATH "PortAudio library - debug version" FORCE^)
echo.
echo # --- O2 ---
echo set(O2_INCL "%PROJECTS:\=/%/o2/src" CACHE PATH "Main O2 include directory" FORCE^)
echo set(O2_OPT_LIB "%PROJECTS:\=/%/o2/build/o2_static.lib"
echo         CACHE FILEPATH "O2 library - optimized version" FORCE^)
echo set(O2_DBG_LIB "%PROJECTS:\=/%/o2/build-debug/o2_static.lib"
echo         CACHE FILEPATH "O2 library - debug version" FORCE^)
echo.
echo # --- FAUST ---
if defined FAUST_BIN_DIR (
echo set(FAUST_PATH "!FAUST_BIN_DIR:\=/!" CACHE PATH
echo         "This is added to shell PATH so a shell can run FAUST" FORCE^)
) else (
echo # FAUST_PATH not set - faust was not found on PATH during setup
echo # set(FAUST_PATH "C:/Program Files/Faust/bin" CACHE PATH
echo #         "This is added to shell PATH so a shell can run FAUST" FORCE^)
)
echo.
echo # --- Compression libraries (disabled - built libsndfile without external libs) ---
echo # If you need OGG/FLAC/Vorbis support, build them and uncomment:
echo # set(OGG_INCL ... ^)
echo # set(OGG_OPT_LIB ... ^)
echo # set(FLAC_INCL ... ^)
echo # set(VORBIS_INCL ... ^)
echo.
echo # --- FluidSynth (optional - for flsyn unit generator) ---
echo # set(FLSYN_INCL "..." CACHE PATH "Include directory for Fluid Synth" FORCE^)
echo # set(FLSYN_OPT_LIB "..." CACHE PATH "Release version of Fluid Synth library" FORCE^)
echo # set(FLSYN_DBG_LIB "..." CACHE PATH "Debug version of Fluid Synth library" FORCE^)
) > "%ARCO%\apps\common\libraries.txt"

echo [OK] libraries.txt generated at apps\common\libraries.txt

REM ============================================================
REM  SUMMARY
REM ============================================================
echo.
echo ============================================================
echo  Setup complete! Next steps:
echo ============================================================
echo.
echo  1. Review apps\common\libraries.txt and fix any paths
echo     that look wrong (search for empty quotes).
echo.
echo  2. The basic server (apps/basic) uses curses (ncurses/form).
echo     You may need to adjust arcoserver.cmakeinclude to link
echo     PDCurses instead. The PDCurses lib is at:
echo       %PROJECTS%\pdcurses\wincon\pdcurses.lib
echo.
echo  3. Build the Arco basic server:
echo       cd apps\basic
echo       cmake . -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
echo       nmake
echo.
echo  4. Run the server:
echo       cd apps\basic
echo       arcobasic.exe
echo.
echo  5. Connect from PyArco:
echo       o2litepy is installed and ready to use.
echo.
echo  NOTE: libsndfile was built WITHOUT external codec libs
echo  (OGG, FLAC, Vorbis, Opus). WAV files will work fine.
echo  If you need compressed formats, build those libs and
echo  update libraries.txt.
echo ============================================================

endlocal
