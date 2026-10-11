CC ?= gcc
PKG := sdl2 SDL2_image SDL2_ttf
CFLAGS ?= -O2 -Wall -Wextra -std=c11
CFLAGS += $(shell pkg-config --cflags $(PKG))
LDLIBS := $(shell pkg-config --libs $(PKG)) -lm

SRC := src/main.c src/game.c src/render.c src/ui.c src/config.c src/sound.c
HDR := src/winmine.h

winmine: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDLIBS)

clean:
	rm -f winmine

.PHONY: clean
