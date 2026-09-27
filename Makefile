CC ?= clang
AR ?= llvm-ar
CFLAGS ?= -std=c23 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
SDL3_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LIBS ?= $(shell pkg-config --libs sdl3 2>/dev/null)

CFLAGS += $(SDL3_CFLAGS)

Library := build/libneogeorecomp.a
Sources := \
	src/neogeorecomp.c \
	src/m68k.c \
	src/bus.c \
	src/func_table.c \
	src/video.c \
	src/palette.c \
	src/io.c \
	src/ym2610.c \
	src/z80.c \
	src/timer.c \
	src/platform.c \
	src/debug.c

Objects := $(Sources:%.c=build/%.o)

.PHONY: all clean test debug

all: $(Library)

$(Library): $(Objects)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(Objects)

build/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

debug: CFLAGS += -g -O0
debug: clean all

test: all
	@echo "No test executable is defined yet."

clean:
	rm -rf build
