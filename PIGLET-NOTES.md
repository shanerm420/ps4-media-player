# Piglet / PS4 graphics notes carried from other homebrew

Adopted 2026-11-24 after the owner asked whether work was being redone. Rule R25:
before a round, read what other PS4 homebrew already solved and what the ES2 spec
says, before spending a console run. Everything here is a **finding**, not code:
each item was written up by its author from their own hardware, and anything on
this project's side is written independently (clean-room note, rule G7).

Status: `researched only` here until this project measures it per rule R15/R24.

## Sources

- **`alechurri/shipofharkinian-ps4` — `docs/TECHNICAL.md`**
  <https://github.com/alechurri/shipofharkinian-ps4/blob/main/docs/TECHNICAL.md>
  A full commercial-scale Piglet port with a "things that only showed up on
  hardware" section. The single most useful public document for this work.
- **`Navinas3460/Stremio-PS4` — `src/platform/platform_ps4.cpp`**
  <https://raw.githubusercontent.com/Navinas3460/Stremio-PS4/main/src/platform/platform_ps4.cpp>
  A video app on the same stack: Piglet context, DualShock 4 input, precompiled
  shaders, a 1/60 frame constant and its own frame timer.
- **`shadPS4` — `src/core/libraries/videoout/video_out.cpp`, `driver.cpp`**
  <https://deepwiki.com/squidbus/shadPS4/7.2-video-output-system>
  The clearest public statement of the display contract.
- **The OpenOrbis Piglet sample** — `samples/piglet/piglet/PigletApplication.cpp`,
  which is what this project's probe is built on.

## Display and vsync

- `VblankPeriod = 16666667ns`; flips are queued per vblank and
  `sceVideoOutSetFlipRate` divides it (30, 20, 15 fps…). That is where 16.67 ms
  comes from and why any CPU-side stopwatch lands on whole multiples of it.
- `eglSwapInterval(1)` called **after** `eglMakeCurrent` makes `eglSwapBuffers`
  block forever on the first frame; called **before** the surface exists it
  works, but shipofharkinian-ps4 found interval 0 plus the engine's own frame
  timer clearly better than waiting on the display.
- The Piglet sample already calls `eglSwapInterval(s_display, 0)` before
  `eglChooseConfig` (`PigletApplication.cpp:182`), and it measures ~8.3 ms a
  frame on a textured quad, so this project's probe has always run unlocked.
  Consequence: BREW00102's 16.66 ms per frame **with no present at all** is not
  vsync and still needs an explanation - it is what BREW00103's GPU-clock
  measurement is for.

## Memory

- An app packaged as `CATEGORY gde` with Piglet's auth info gets about **768 MiB of direct memory and 255 MiB of flexible memory**, not the
gigabytes a game gets - the mini-app default, where a `gd` package on a base PS4
  measures 4608 MiB. Textures and render targets come out of flexible memory.
- The media player's own log reports a 4,608 MB direct pool, which cannot both
  be true of the same process class. Measure before trusting either number in
  the memory budget (rule G1).
- `scePigletSetConfigurationVSH` accepts almost any configuration and fails
  later, in `eglGetDisplay`, with "Out of memory".

## Renderer limits seen in practice

- **No `glBlitFramebuffer`**: framebuffer copies are a textured quad - which is
  what this project's `draw_quad` already does.
- **No depth readback**; offscreen depth is **16-bit** in practice.
- No VAOs, no MSAA, no unsized texture formats.
- GLSL ES 1.00 only: `attribute`/`varying`/`texture2D`/`gl_FragColor`; never
  index a uniform array with a non-constant expression; avoid `textureSize()`
  and `mix()` with a boolean selector.
- **Shader compilation costs 110-260 ms per program** and cannot be cached,
  because Piglet rejects `GL_PROGRAM_BINARY_LENGTH`. In a media player that is
  the likely shape of a "music stall" the first time a look compiles, and the
  reason to pre-compile shaders behind the splash screen.
- `glCopyTexImage2D` from the default framebuffer takes seconds: render into your
  own framebuffer and copy with a quad.

## OpenOrbis header traps

- **`struct stat` is misdeclared**: `<bits/alltypes.h>` makes `mode_t` 32-bit
  while the kernel uses 16, so every member after `st_mode` is 8 bytes off and
  `st_size` actually reads `st_blocks`. Their fix force-includes a header that
  defines `mode_t` before any system header. Relevant the day this project
  trusts `stat()` for a file size.
- **Relative paths fail with `EINVAL`, not `ENOENT`**, which makes
  `std::filesystem::exists` throw. Every path here is absolute already.
- The bundled libc++ is old (11): `std::filesystem::file_size` and
  `last_write_time` are not reliable.

## What is still genuinely ours to measure

Nothing in the sources above answers these, so a console round on them is not
wasted work:

- what one 1080p bloom chain costs on this console (BREW00103);
- why a frame from which the present was removed still costs 16.66 ms;
- whether a Piglet surface can share a screen with SDL's chrome, which the
  player's GPU path depends on.
