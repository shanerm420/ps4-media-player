# Third-party material

This project is MIT licensed (see `LICENSE`). It builds on the following, none
of which is claimed as original work.

| Component | Role | Licence | Where |
|---|---|---|---|
| [minimp3](https://github.com/lieff/minimp3) | MP3 decoder (single header) | CC0 1.0 | fetched by `make deps` |
| [font8x8](https://github.com/dhepper/font8x8) | fallback 8x8 bitmap font | public domain | `third_party/font8x8*` |
| [stb_image](https://github.com/nothings/stb) | JPEG / PNG / BMP decoding for the picture viewer | public domain / MIT | `third_party/stb_image.h` |
| [Inter](https://github.com/rsms/inter) | interface typeface | SIL Open Font Licence 1.1 | fetched by `make deps`, rasterised into `src/font_inter.h` |
| [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) | clang target, linker script, fake-PKG packaging | GPL-2.0 / MIT as per upstream | not distributed here |
| [SDL-PS4](https://github.com/OpenOrbis/SDL-PS4) | SDL2 port used by the toolchain | zlib | not distributed here |
| [LibOrbisPkg](https://github.com/OpenOrbis/LibOrbisPkg) | PKG creation (`PkgTool.Core`) | GPL-2.0 | not distributed here |

## Not in this repository

* **Sony firmware libraries.** Video playback calls `libSceAvPlayer` from the
  console's own firmware at runtime. No Sony code, firmware blob or signing key
  is included or redistributed.
* **The built package.** `dist/` is ignored; build it yourself with `make`.
* **Sample media.** The host tests take any MP3 you have locally.
