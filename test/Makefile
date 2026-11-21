# Host build of the platform-independent audio path: eq, resampler, volume,
# limiter, MP3 decoding. No PS4 and no OpenOrbis toolchain needed.
# No audio file is committed: pass MP3=/path/to/file.mp3 to `make run`.
# Any short MP3 works, e.g. ffmpeg -f lavfi -i "sine=1" -t 1 test.mp3
MP3     ?= data/test.mp3
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
INCS    := -I../src -I../third_party
SRC     := ../src/eq.c ../src/resampler.c ../src/audio_core.c ../src/mp3_source.c \
           ../src/subtitles.c host_test.c
BIN     := build/host_test

.PHONY: all run clean
all: $(BIN)

$(BIN): $(SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(INCS) $(SRC) -lm -o $@

run: $(BIN)
	./$(BIN) $(MP3)

clean:
	rm -rf build
