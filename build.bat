@echo off
REM Blackthorn Manor - fallback build without make (double-click or run in cmd).
REM Edit the two paths below if your w64devkit / raylib live somewhere else.
set "W64=C:\Users\HP GM\Downloads\w64devkit\bin"
set "RAYLIB=C:\Users\HP GM\Downloads\raylib-6.0_win64_mingw-w64"
set "PATH=%W64%;%PATH%"

setlocal enabledelayedexpansion
set SRC=
for %%f in (src\*.c) do set SRC=!SRC! %%f

gcc !SRC! -o BlackthornManor.exe -std=c99 -Wall -Wextra -Wno-unused-parameter -O2 -Isrc ^
    -I"%RAYLIB%\include" -L"%RAYLIB%\lib" -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 (echo BUILD FAILED) else (echo Build OK - run BlackthornManor.exe)
