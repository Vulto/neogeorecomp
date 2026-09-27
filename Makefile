CC ?= gcc
AR ?= ar
CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS ?= -Iinclude
SDL2_CFLAGS ?= $(shell sdl2-config --cflags 2>/dev/null)
SDL2_LIBS ?= $(shell sdl2-config --libs 2>/dev/null)

CFLAGS += $(SDL2_CFLAGS)

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
