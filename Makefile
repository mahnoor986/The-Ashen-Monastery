# The Ashen Monastery - Makefile for w64devkit + raylib 6.0 (Windows)
#
#   make            debug build -> AshenMonastery.exe
#   make run        build and run
#   make autotest   build and run the screenshot self-test (writes shots/*.png)
#   make release    optimized build + self-contained Windows ZIP in dist/
#   make clean
#
# Raylib 6.0 layout:
#   RAYLIB_PATH/include/raylib.h
#   RAYLIB_PATH/lib/libraylib.a
#
# Example (paths may contain spaces, they are quoted below):
#   make RAYLIB_PATH="C:/Users/HP GM/Downloads/raylib-6.0_win64_mingw-w64"

CC          := gcc
TARGET      := AshenMonastery.exe

RAYLIB_PATH ?= C:/Users/HP GM/Downloads/raylib-6.0_win64_mingw-w64

SRC  := $(wildcard src/*.c)
OBJ  := $(patsubst src/%.c,build/%.o,$(SRC))
DEP  := $(OBJ:.o=.d)

CFLAGS  := -std=c99 -Wall -Wextra -Wno-unused-parameter -g -O0 -Isrc -I"$(RAYLIB_PATH)/include"
LDFLAGS := -L"$(RAYLIB_PATH)/lib"
LDLIBS  := -lraylib -lopengl32 -lgdi32 -lwinmm

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

build:
	mkdir -p build

run: $(TARGET)
	./$(TARGET)

autotest: $(TARGET)
	./$(TARGET) --autotest

release: clean
	"$(MAKE)" CFLAGS='-std=c99 -Wall -Wextra -Wno-unused-parameter -O2 -Isrc -I"$(RAYLIB_PATH)/include"' LDLIBS="-lraylib -lopengl32 -lgdi32 -lwinmm -mwindows -s"
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/package-release.ps1

clean:
	rm -rf build $(TARGET)

-include $(DEP)

.PHONY: all run autotest release clean
