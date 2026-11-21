#!/bin/sh
# Fetches the two things this repository does not vendor:
#
#   third_party/minimp3.h   CC0 MP3 decoder (upstream single header)
#   src/font_inter.h        Inter glyph atlas, generated from the OFL font
#
# Both are pinned by SHA-256. Re-run after a fresh clone, before `make`.
# Needs curl (or wget) and, for the atlas, python3 with Pillow.
set -eu

cd "$(dirname "$0")/.."

MINIMP3_URL="https://raw.githubusercontent.com/lieff/minimp3/master/minimp3.h"
MINIMP3_SHA="f9592be69e8b94044e9da6f45f993c9708000abae38cf6f44b70c699de55b509"

INTER_URL="https://raw.githubusercontent.com/google/fonts/main/ofl/inter/Inter%5Bopsz%2Cwght%5D.ttf"
INTER_SHA="29160a80ff49ddcab2c97711247e08b1fab27a484a329ce8b813d820dc559031"

fetch() {
    out="$1"
    sha="$2"
    url="$3"

    if [ -f "$out" ] && echo "$sha  $out" | sha256sum -c - >/dev/null 2>&1; then
        echo "ok   $out"
        return 0
    fi

    mkdir -p "$(dirname "$out")"
    echo "get  $out"
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL -o "$out.part" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$out.part" "$url"
    else
        echo "error: need curl or wget" >&2
        exit 1
    fi

    echo "$sha  $out.part" | sha256sum -c - >/dev/null 2>&1 || {
        echo "error: checksum mismatch for $url" >&2
        rm -f "$out.part"
        exit 1
    }
    mv "$out.part" "$out"
}

fetch third_party/minimp3.h "$MINIMP3_SHA" "$MINIMP3_URL"
fetch build/Inter.ttf     "$INTER_SHA"   "$INTER_URL"

python3 -c "import PIL" 2>/dev/null || {
    echo "error: the atlas generator needs Pillow: pip install pillow" >&2
    exit 1
}
python3 tools/make-font-atlas.py build/Inter.ttf --name font_inter --size 36 \
    > src/font_inter.h
echo "done: third_party/minimp3.h and src/font_inter.h are ready"
