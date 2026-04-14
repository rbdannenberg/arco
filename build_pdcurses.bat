@echo off
REM Build PDCurses for Windows console (wincon).
REM Run from a VS x64 Developer Command Prompt.
REM Set PROJECTS to the parent directory if pdcurses is not at ..\pdcurses
if not defined PROJECTS set PROJECTS=%~dp0..
cd /d "%PROJECTS%\pdcurses\wincon"
nmake /f Makefile.vc
