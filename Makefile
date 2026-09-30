# Gothic Dungeon - Makefile for w64devkit + raylib (Windows)
#
#   make            debug build -> game.exe
#   make run        build and run
#   make release    optimized build, no console window
#   make clean
#
# Raylib 6.0 layout:
#   RAYLIB_PATH/include/raylib.h
#   RAYLIB_PATH/lib/libraylib.a
#
# Example:
#   make RAYLIB_PATH=C:/Users/NAT/Documents/raylib-6.0_win64_mingw-w64

CC          := gcc
TARGET      := game.exe

RAYLIB_PATH ?= C:/Users/NAT/Documents/raylib-6.0_win64_mingw-w64

SRC  := $(wildcard src/*.c)
OBJ  := $(patsubst src/%.c,build/%.o,$(SRC))
DEP  := $(OBJ:.o=.d)

CFLAGS  := -std=c99 -Wall -Wextra -Wno-unused-parameter -g -O0 -Isrc -I$(RAYLIB_PATH)/include
LDFLAGS := -L$(RAYLIB_PATH)/lib
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

release: clean
	$(MAKE) CFLAGS="-std=c99 -Wall -O2 -Isrc -I$(RAYLIB_PATH)/include" LDLIBS="-lraylib -lopengl32 -lgdi32 -lwinmm -mwindows -s"

clean:
	rm -rf build $(TARGET)

-include $(DEP)

.PHONY: all run release clean