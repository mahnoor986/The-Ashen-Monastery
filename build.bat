@echo off
REM Fallback build without make. Edit the two paths if yours differ.
set PATH=C:\w64devkit\bin;%PATH%
set RAYLIB=C:/raylib/raylib

gcc src/main.c src/game.c src/player.c src/enemy.c src/map.c src/draw.c src/lighting.c src/ui.c ^
    -o game.exe -std=c99 -Wall -g -Isrc -I%RAYLIB%/src -L%RAYLIB%/src ^
    -lraylib -lopengl32 -lgdi32 -lwinmm
if errorlevel 1 (echo BUILD FAILED) else (echo Build OK - run game.exe)
