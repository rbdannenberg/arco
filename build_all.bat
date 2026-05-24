@echo off
REM Build all Arco dependencies on Windows.
REM Run from a VS x64 Developer Command Prompt, or set VCVARSALL to your
REM vcvarsall.bat path before running this script.
REM
REM Set PROJECTS to the parent directory containing arco, o2, portaudio, etc.
REM Defaults to the parent of this script's directory.

setlocal enabledelayedexpansion

REM --- Set up VS environment if not already active ---
where cl >nul 2>&1
if errorlevel 1 (
    if defined VCVARSALL (
        call "%VCVARSALL%" x64 >nul 2>&1
    ) else (
        echo ERROR: cl.exe not found. Run from a VS x64 Developer Command Prompt,
        echo        or set VCVARSALL to the path of vcvarsall.bat.
        exit /b 1
    )
)
set PATH=%PATH%;C:\Program Files\Faust\bin;C:\Program Files\CMake\bin

echo === Verifying tools ===
where cl >nul 2>&1 && echo [OK] cl.exe || echo [FAIL] cl.exe
where nmake >nul 2>&1 && echo [OK] nmake || echo [FAIL] nmake
where cmake >nul 2>&1 && echo [OK] cmake || echo [FAIL] cmake
where faust >nul 2>&1 && echo [OK] faust || echo [FAIL] faust
where python >nul 2>&1 && echo [OK] python || echo [FAIL] python

if not defined PROJECTS set PROJECTS=%~dp0..

echo.
echo === Step 1: Clone PortAudio ===
if not exist "%PROJECTS%\portaudio" (
    cd /d %PROJECTS%
    git clone https://github.com/PortAudio/portaudio.git
    if errorlevel 1 (echo [FAIL] clone portaudio & exit /b 1)
) else (
    echo [OK] portaudio already exists
)

echo.
echo === Step 2: Build PortAudio Release ===
if not exist "%PROJECTS%\portaudio\build" mkdir "%PROJECTS%\portaudio\build"
cd /d "%PROJECTS%\portaudio\build"
if not exist "portaudio_static.lib" (
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DPA_BUILD_SHARED_LIBS=OFF
    if errorlevel 1 (echo [FAIL] cmake portaudio & exit /b 1)
    nmake
    if errorlevel 1 (echo [FAIL] build portaudio & exit /b 1)
)
echo [OK] PortAudio Release

echo.
echo === Step 3: Build PortAudio Debug ===
if not exist "%PROJECTS%\portaudio\build-debug" mkdir "%PROJECTS%\portaudio\build-debug"
cd /d "%PROJECTS%\portaudio\build-debug"
if not exist "portaudio_static.lib" (
    cmake "%PROJECTS%\portaudio" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DPA_BUILD_SHARED_LIBS=OFF
    nmake
)
echo [OK] PortAudio Debug

echo.
echo === Step 4: Clone libsndfile ===
if not exist "%PROJECTS%\libsndfile" (
    cd /d %PROJECTS%
    git clone https://github.com/libsndfile/libsndfile.git
    if errorlevel 1 (echo [FAIL] clone libsndfile & exit /b 1)
) else (
    echo [OK] libsndfile already exists
)

echo.
echo === Step 5: Build libsndfile Release ===
if not exist "%PROJECTS%\libsndfile\build" mkdir "%PROJECTS%\libsndfile\build"
cd /d "%PROJECTS%\libsndfile\build"
if not exist "sndfile.lib" (
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=OFF -DINSTALL_MANPAGES=OFF
    if errorlevel 1 (echo [FAIL] cmake libsndfile & exit /b 1)
    nmake
    if errorlevel 1 (echo [FAIL] build libsndfile & exit /b 1)
)
echo [OK] libsndfile Release

echo.
echo === Step 6: Build libsndfile Debug ===
if not exist "%PROJECTS%\libsndfile\build-debug" mkdir "%PROJECTS%\libsndfile\build-debug"
cd /d "%PROJECTS%\libsndfile\build-debug"
if not exist "sndfile.lib" (
    cmake "%PROJECTS%\libsndfile" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DENABLE_EXTERNAL_LIBS=OFF -DBUILD_SHARED_LIBS=OFF -DINSTALL_MANPAGES=OFF
    nmake
)
echo [OK] libsndfile Debug

echo.
echo === Step 7: Clone PDCurses ===
if not exist "%PROJECTS%\pdcurses" (
    cd /d %PROJECTS%
    git clone https://github.com/wmcbrine/PDCurses.git pdcurses
    if errorlevel 1 (echo [FAIL] clone pdcurses & exit /b 1)
) else (
    echo [OK] pdcurses already exists
)

echo.
echo === Step 8: Build PDCurses ===
cd /d "%PROJECTS%\pdcurses\wincon"
if not exist "pdcurses.lib" (
    nmake /f Makefile.vc
    if errorlevel 1 (echo [FAIL] build pdcurses & exit /b 1)
)
echo [OK] PDCurses

echo.
echo === Step 9: Build O2 Release ===
if not exist "%PROJECTS%\o2\build" mkdir "%PROJECTS%\o2\build"
cd /d "%PROJECTS%\o2\build"
if not exist "o2_static.lib" (
    cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DTESTS_BUILD=OFF -DBUILD_SHARED_LIBS=OFF
    if errorlevel 1 (echo [FAIL] cmake o2 & exit /b 1)
    nmake
    if errorlevel 1 (echo [FAIL] build o2 & exit /b 1)
)
echo [OK] O2 Release

echo.
echo === Step 10: Build O2 Debug ===
if not exist "%PROJECTS%\o2\build-debug" mkdir "%PROJECTS%\o2\build-debug"
cd /d "%PROJECTS%\o2\build-debug"
if not exist "o2_static.lib" (
    cmake "%PROJECTS%\o2" -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DTESTS_BUILD=OFF -DBUILD_SHARED_LIBS=OFF
    nmake
)
echo [OK] O2 Debug

echo.
echo === Step 11: Install o2litepy ===
pip install -e "%PROJECTS%\o2\o2litepy\py3pkg" 2>/dev/null
if errorlevel 1 (
    python -m pip install -e "%PROJECTS%\o2\o2litepy\py3pkg"
)
echo [OK] o2litepy

echo.
echo === ALL DEPENDENCIES BUILT ===
echo.
echo Verify libraries exist:
if exist "%PROJECTS%\portaudio\build\portaudio_static.lib" (echo [OK] portaudio Release) else (echo [MISS] portaudio Release)
if exist "%PROJECTS%\portaudio\build-debug\portaudio_static.lib" (echo [OK] portaudio Debug) else (echo [MISS] portaudio Debug)
if exist "%PROJECTS%\libsndfile\build\sndfile.lib" (echo [OK] sndfile Release) else (echo [MISS] sndfile Release)
if exist "%PROJECTS%\libsndfile\build-debug\sndfile.lib" (echo [OK] sndfile Debug) else (echo [MISS] sndfile Debug)
if exist "%PROJECTS%\pdcurses\wincon\pdcurses.lib" (echo [OK] pdcurses) else (echo [MISS] pdcurses)
if exist "%PROJECTS%\o2\build\o2_static.lib" (echo [OK] o2 Release) else (echo [MISS] o2 Release)
if exist "%PROJECTS%\o2\build-debug\o2_static.lib" (echo [OK] o2 Debug) else (echo [MISS] o2 Debug)
