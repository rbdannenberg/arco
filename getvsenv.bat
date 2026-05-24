@echo off
REM Print the VS build environment variables after loading vcvarsall.
REM Usage: set VCVARSALL to your vcvarsall.bat path, then run this script.
REM Or run from a VS Developer Command Prompt directly.
if defined VCVARSALL (
    call "%VCVARSALL%" x64 >nul 2>&1
)
@echo PATH=%PATH%
@echo INCLUDE=%INCLUDE%
@echo LIB=%LIB%
