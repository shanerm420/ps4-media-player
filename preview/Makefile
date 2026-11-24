# Host renderer for src/ui.c: draws the real interface to a BMP on a PC using
# SDL2's dummy video driver. No PS4, no OpenOrbis toolchain.
#
#   make -C preview run                      -> out-main.bmp, out-eq.bmp
#   make -C preview run FONT=fonts/inter     -> uses that glyph atlas directory
#
# FONT points at a directory holding ui_font_atlas.h (see tools/make-font-atlas.py).
# Without it the interface uses the bundled 8x8 pixel font, which is what ships.

SDL_ROOT ?= $(HOME)/hostdeps/root
CC       ?= cc
CFLAGS   ?= -O2 -Wall -Wextra -std=c11
PREFIX   ?= out

# The preview renders the same font the console build uses: the Inter atlas in
# src/font_inter.h. Nothing to point at, nothing to install.
FONTFLAGS := -DUI_FONT_ATLAS

INCS     := -I$(SDL_ROOT)/usr/include/x86_64-linux-gnu -I$(SDL_ROOT)/usr/include \
            -I../src -I../third_party $(FONTFLAGS)
LIBS     := -L$(SDL_ROOT)/usr/lib/x86_64-linux-gnu -lSDL2 -lm -lpthread -Wl,--allow-shlib-undefined
SRC      := preview.c ../src/ui.c ../src/eq.c ../src/spectrum.c ../src/font_atlas.c ../src/vis_art.c \
            ../src/appcfg.c ../src/applog.c \
            ../src/stb_image_impl.c ../src/ps_mark.c ../third_party/font8x8.c

RUNPATH  := $(SDL_ROOT)/usr/lib/x86_64-linux-gnu:$(SDL_ROOT)/usr/lib/x86_64-linux-gnu/pulseaudio

all: build/preview

build/preview: $(SRC) ../src/ui.h ../src/spectrum.h ../src/font_atlas.h ../src/font_inter.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCS) $(SRC) $(LIBS) -o $@

run: build/preview
	LD_LIBRARY_PATH=$(RUNPATH) SDL_VIDEODRIVER=dummy ./build/preview $(PREFIX)

clean:
	rm -rf build out-*.bmp

.PHONY: all run clean
