# Build history

The dated log: what was built, what ran on the console,
what the log said. Read it when you need to know *why* a rule exists.

[Back to the index](README.md).

---

# PS4 media player — build history

Extracted verbatim from `MANIFEST.md` §4 on 2026-11-24, when that section had grown
to 827 lines and was crowding out the rules and the current state. This file is the
record: it grows without limit, in date order, oldest first. Nothing here was
rewritten. The dated index stays behind in `MANIFEST.md` §4.

Append-only, newest last. Exact commands and results, failures included. Older
rounds are summarized in `files/round2/README.md` and `files/round3/README.md`.

### 2026-11-23 — v113 (Liquid: water body, depth, framing)

- Changed `src/vis_art.c` only: body lit from a smoothed height field instead of
  the per-column crest value; depth ramp green under crests to deep blue; new
  `[liquid] surface_lift` (default 0.08) in `src/appcfg.c`.
- Command: `OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain
  PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH make
  PKG_LD=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu`
- Result: build OK, `make check` ALL TESTS PASSED, pkg MD5
  `ce7c55f0749743620a86630792bc2449`. Hardware: 50.0–60.8 ms at step 3 against
  62.5–63.2 in v112; the ladder dropped 1→2→3 and stayed at 3.

### 2026-11-23 — v114 (Liquid: glass, floor, spray, glint)

- Removed the `s_paint_step <= 2` gates on the side panes, the glow over the
  water and the pane highlights (the console never climbs back to step 2, so the
  glass was missing on hardware). Added lit inner glass edges, a wet floor band
  driven by the smoothed body height, more floor lines and caustics, bigger and
  longer-lived spray, and five travelling glints five rows under the lip.
- `preview/preview.c`: new `seq <mode> <style> <frames> <path>` mode, because the
  solver, foam and spray do not exist on frame one.
- Result: build OK, tests pass, pkg MD5
  `b98a943f1c72f7a2582cb4e61475b985`. Host: 27.13 / 13.93 / 10.27 ms at steps
  1/2/3 against 22.19 / 12.05 / 9.34 for v112 — raw +22%/+16%/+10%, but the
  untouched Rave look ran 6–10% slower in the same session, so net ≈ +14%/+9%/0.
  NEON, WORM and RAVE DJ render byte-identical to v112.
- Console: step ladder reached 3, Liquid 50–61 ms; television photos of
  full-screen Liquid came back and were reviewed (see open items).

### 2026-11-23 — v115 (the 467 ms freeze on Square)

- Cause: a skin change rebuilt the 960×540 room background in one frame —
  435 ms of a 467.1 ms frame, twice in one log, to a tenth of a millisecond,
  with `flush` unchanged. The startup bake measures 441 ms.
- Fix in `src/ui.c`: `bg_rows()` and `bg_upscale_rows()` split out; a job builds
  the new room in 12 ms slices (`BG_JOB_MS`) while the outgoing skin keeps
  drawing; one upload at the end; a second theme change abandons the old build.
- `preview/preview.c`: new `skin [frames] [switch_at] [path]` mode that times
  every frame across a skin change.
- Result: build OK, `make check` ALL TESTS PASSED, pkg MD5
  `25f3af25c68a57fb8509de28fd83e40e`.
  Host: worst frame after the press 110.9 → 19.1 ms, zero frames over 20 ms;
  output BMP byte-identical to a build forced to bake in one frame
  (`-DBG_JOB_MS=100000`); AddressSanitizer clean; Liquid renders byte-identical
  to v114 (`ecaed6f6163045be31f92f9336b3b74d`, cited, not archived).

### 2026-11-23 — manifest: environment closed, GoldHEN and FTP researched

- Recorded the console from the owner's answers: PS4 Pro, firmware 12.52,
  GoldHEN v2.4b18.14 with a payload and a payload loader, and **FTP as the only
  transfer path** in both directions (§7).
- Research on GoldHEN v2.4b18.14 / firmware 12.52 with five phrasings across the
  GoldHEN repository and release pages, SiSTRo's Ko-fi shop, a GitHub issue, the
  ConsoleMods wiki and two package-over-FTP guides. Findings, and the three
  places where sources disagree, are in §9.
- Delivery loop changed to match: build → upload the pkg to `/data/pkg/` →
  install from Package Installer's Hdd source → pull `app.log` and `shots/`
  back over FTP. Written into `AGENTS.md` §Delivery and test loop.
- No source changed, so no rebuild and no new package; v115 remains current.

### 2026-11-23 — v116 (glint, spray, floor — the three look tweaks)

- `src/vis_art.c`, Liquid only: glint alpha 170–230 → 215–255 with a streak
  14–26 px wide and a wide dim halo under it (still five rows below the lip, so
  the crest and the foam are untouched); 72% of spray now steps a random column
  to the highest water within a twentieth of the width before spawning; a
  built-in floor lift of 0.03 of the frame height on top of
  `[liquid] floor_y`, so an older `config.ini` gets it too.
- Result: build OK, `make check` ALL TESTS PASSED, pkg MD5
  `7e215289a0f6ec58a55c50e4058e6b3c`. NEON, WORM and RAVE DJ still render
  byte-identical to v114.
- Glint effect (same frame with the glint disabled, then differenced):
  +66 luma maximum over 567 pixels, every changed row at least two rows below
  the lip. v114 measured +52 over 262.
- Host bench, medians of 3×100 frames at steps 1/2/3: 26.74 / 15.08 / 11.36 ms,
  against 27.13 / 13.93 / 10.27 for v114 and a Rave control of 62.85 / 23.66 /
  14.71 (v114 session: 57.36 / 22.26 / 14.80). Step 3 reads +11% net, but
  reverting each tweak on its own at step 3 measured 11.89 (no floor lift),
  11.47 (no halo) and 12.13 (uniform spray) against 11.36 for all three —
  removing work never made it faster, and the flush alone swung 4.3–6.1 ms
  between runs. So the three are free within noise and the step-3 gap is machine
  state. Notes: `files/round5/`.
- Package builds are not byte-reproducible: `eboot.bin` embeds `__DATE__` and
  `__TIME__`, so two builds of identical source give different pkgs. The MD5
  identifies a build; it does not prove the source. Prove that by the source
  hash (`src/vis_art.c` `ae28b0f97614260bfa2fbb5075061d50` at this build) and by
  the build line's version in the log.

### 2026-11-23 — on hardware: the freeze, and Liquid's step

- Run header: `build v116`, `build Nov 23 2026 21:50:29`, started 21:58:51, 6.5
  minutes, 2921 tracks and 22 videos scanned. So the Round 5 package was
  installed over FTP, and this one log answers both the v115 freeze question and
  the v116 look question.
- **Freeze: gone.** Over forty `button 2` presses between 9.6 s and 20.9 s: no
  `long frame` line anywhere in that window, `max=50.1` in every summary,
  `worst_paint` 18.1–18.5 ms, `paint` 9.2–11.8 ms. `paint` stayed high for the
  whole stretch, which is the signature of a bake in progress on those frames
  (12 ms slices), so the fix was exercised, not merely idle. Before v115 the
  same press produced 467.1 ms with 435 ms of paint.
- Four `long frame` lines in the whole run, none on a key press: 116.8 ms on the
  first visualizer frame (the bake), 383.8 ms and 250.2 ms around a video open
  and close. Startup still has six frames of 119–661 ms between 0.9 s and 5.2 s
  while the library scans (1539 ms for 12376 entries) — boot, unchanged, and
  behind the loading screen.
- **Liquid settles at step 2 = 15 fps.** At 27.98 s it stepped 1 → 2 (85.0 ms >
  56 ms) and never moved again: 66.7 ms a frame, paint 33.4, `over=0` because
  step 2's budget is `quality.budget_ms` = 88 ms. The ladder is behaving as
  designed (`appcfg.c`: full resolution is the default until a look cannot hold
  ~11 fps), so 15 fps is what this console gets. Host paint at step 2 is 15.1 ms,
  so the console is about 2.2× — in line with every other comparison. Not a v116
  regression.
- `[quality] budget_ms` is the dial if 20 fps is wanted instead: set it to ~62
  and 66.7 ms > 62 drops Liquid to step 3, at a third of the canvas. Not changed
  here — the look is the thing being judged, and a softer canvas would muddy it.

### 2026-11-23 — manifest merge: the graphics standard becomes part of the goal

- Merged the owner's context reset into this file. Nothing was built, no source
  changed, no package was made: this entry is documentation only, and v116 stays
  current.
- §1 gained the standing graphics requirement verbatim plus the five ways it has
  to be carried out (five-phrasing research, audit, gaps first, no accepting a
  worse method, trade-offs logged and revisited). §3 gained a graphics-standard
  block of seven steps. §2 was rewritten to the new bar: the audit first, the
  step-2 / 15 fps finding counted against the standard instead of accepted, and
  the video defect queued behind it. §5 gained two decisions and an amendment to the
  software-rasterizer decision. §6 gained rows for the video 16 fps / 15,397 pull
  failures, Liquid at step 2, the four remaining long frames, and a correction to
  the "step-2/3 detail lost is accepted" row, which the new standard withdraws.
  §8 gained the GPU sources checked so far. §9 gained "GPU: verified and
  unknown". §10 gained three questions. §11 was rewritten to the owner's order.
- Verified this session while writing the section (commands and results are in
  `AGENTS.md`): there are **eighteen** styles in `vis_style_name()`; no GPU code
  exists in `src/`; the toolchain ships `libSceGnmDriver.so` and the
  `libSceGnmDebug*` stubs; the toolchain include root has `EGL/`, `GLES2/` and
  `KHR/` headers but no GLES/EGL/Vulkan stub library was found in `lib/`; and
  the app **already** starts `libSceGnmDriver.sprx` at boot (`sceKernelLoadStartModule`, sandbox path then
  `/system` fallback) and calls `sceGnmSubmitDone()` when the system UI is
  overlaid — so the GNM driver is loadable on this console today, and the
  renderer question is about drawing through it, not about whether it exists.
- Environment re-confirmed by the owner against §7 with no differences: PS4 Pro,
  12.52, GoldHEN v2.4b18.14, payload and loader present, FTP 2121 both ways,
  `/data/pkg/` + Package Installer Hdd source, boot log printing 12.508.001.

### 2026-11-23 — manifest: GPU paths, console facts, techniques, and other players

- Documentation only. No source change, no build, no package; v116 stays
  current.
- §1's standing requirement now carries the owner's restated wording verbatim,
  plus the reason it is being enforced now: Liquid's 15 fps on a reduced canvas
  is the signature of CPU rendering.
- §3 gained the "verify a GPU path, smallest sample first" step and the optional
  feature candidates. §5 gained two decisions (test paths in that order; a
  warp-feedback effect only after a path is proven). §6 gained rows for the
  unconfirmed piglet system modules, the pPlay software-decoder comparison, and
  the GPU path that still has no result. §8 and §9 gained the research index.
  §10 gained three questions. §11 gained the GPU verification step.
- **What was actually verified this session, by search snippet only:** OpenOrbis
  `CHANGELOG.md` v0.5.2 (21 Dec 2021) does list "Added OpenGL/piglet GPU
  rendering sample + headers"; `samples/piglet/README.md` describes it as the
  Piglet system library, "an OpenGL ES 2.0 and EGL 1.4 specs implementation",
  Title ID BREW00071, based on flatz's `ps4_gl_test`; the newest release listed
  is v0.5.3; `PS4-OpenGNM/opengnm` describes itself as a GNM library with
  `sceGnm*`/`sceGpa*` compatibility and a hardware-validated shader compiler on
  **FW 9.00** (not 12.52); pPlay is `Cpasjuste/pplay`, GPL-3.0, mpv/ffmpeg based,
  and its author states it uses a **software decoder** (4K lags).
- **Not verified, and marked as such everywhere below:** the ps4-kit, PS4GL_GNM,
  OpenGNM-stack, jocover TrueHD and PS-PLAY 3.0 entries came from the owner's
  search snippets and were not opened here; no URL was confirmed for them.

### 2026-11-23 — GPU path test: the piglet sample, built and packaged

- Toolchain: **OpenOrbis 0.5.2** — `$HOME/openorbis/PS4Toolchain/CHANGELOG.md`
  head reads "v0.5.2 - December 21st, 2021", which is the release that added the
  OpenGL/piglet sample; the directory is not a git checkout, so the release is
  the only version marker available. Clang **18.1.8**.
- Sample built **exactly as shipped**, from
  `$HOME/openorbis/PS4Toolchain/samples/piglet`, no source edits. Its own
  Makefile sets `TITLE_ID := BREW00093`,
  `CONTENT_ID := IV0000-BREW00093_00-PIGLETGPUR000000`, title "OpenOrbis Piglet
  Sample" — checked in the finished PKG, which contains `BREW00093` and **not**
  `BREM00001`, so it cannot replace the player. (The sample's README says
  BREW00071; the Makefile and the built PKG say BREW00093. Recorded as a
  documentation defect in §6.)
- Three build failures before it packaged, all environment, none in the sample:
  1. `make: clang++: No such file or directory` — the conda clang package here
     has `clang` → `clang-18` but **no `clang++`**. Fixed with
     `make CCX=clang` (clang picks C++ from the `.cpp` suffix); no file created.
  2. `PkgTool.Core` → `Process terminated. Couldn't find a valid ICU package`.
     Fixed with `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1`, the same flag the
     project's own Makefile documents.
  3. `PkgTool.Core pkg_build` → `No usable version of libssl was found`. Fixed
     with `LD_LIBRARY_PATH=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu`.
  Working command, exactly:
  `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 LD_LIBRARY_PATH=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH make CCX=clang`
- Result: `IV0000-BREW00093_00-PIGLETGPUR000000.pkg`, 6,619,136 bytes.
  Archived as `files/Piglet-GPU-Test-BREW00093-shipped.pkg`, MD5
  **`0204bfbc2c718512f3a101b1fc623caf`**. `eboot.bin` 1,299,136 bytes.
  Linking `-lScePigletv2VSH -lScePrecompiledShaders` succeeded against the
  toolchain stubs (`lib/libScePigletv2VSH.so`, `libScePrecompiledShaders.so`).
- **Second package, logging variant** (the shipped sample writes only to stdout,
  which is unreachable on a console). A copy of the sample in
  `_scratch/gpu-test/piglet/` with **one** addition to `piglet/main.cpp`: a
  `freopen("/data/piglet-gpu-test.log", "w", stdout)` plus stderr appended to the
  same file and unbuffered stdout, called first in `main()`. Nothing else
  touched — same sources, same module loading, same rendering path, same title
  id. Archived as `files/Piglet-GPU-Test-BREW00093-logged.pkg`, MD5
  **`a635023753c6be4c0b57829b39c5c7a2`**.
- Delivered nowhere yet: both PKGs wait for FTP to `/data/pkg/`. **No console
  result exists**, so no GPU capability claim is made in this entry.

### 2026-11-23 — v117 (header overlap) and the video/audit analysis

- Video and audit were analysis only; no player source touched for them. The
  only code change is the header.
- `src/ui.c`: new `text_fit()` (measure with `text_width()`, trim until the text
  plus `...` fits, draw). `header()` now fits `m->app_title` (scale 3) and
  `m->status` (scale 1) into the column left of the emblem:
  `emb_x = DESIGN_W - SAFE_X - 226 - 62 - EMBLEM_W` = 1236 px, so the column is
  `1236 - 24 - 96` = **1116 px** at 1920 wide.
- First build failed with four argument errors: `text_at` takes
  `(r, x, y, scale, s, cr, cg, cb)` and the color macros expand to three
  arguments, so `text_fit` needed the same shape. Fixed, rebuilt.
- Result: build OK, `make check` **ALL TESTS PASSED**, preview builds clean,
  screens re-render (modes 13/4/5). `strings eboot.bin | grep -c v117` = 1.
  Package `files/Ultimate-Media-Player-v117.pkg`, MD5
  **`290e7edab55e864e35d497b369828193`**; `src/ui.c` sha256
  `4cfd7c6dba29049e…`. v116 stays the rollback.
- Video investigation (from the v116 logs, no console): `pull_fail` is incremented
  in the drain loop on the first empty `sceAvPlayerGetVideoDataEx` and then the
  loop breaks (`src/video_orbis.c` ~1463), so it counts empty polls, not dropped
  frames; `fps` in the FILE line is `frames / played`, so 16 fps is our display
  rate, not the file's. Present path dominates: `flush` 35–39 ms, `present` 6–19,
  `conv_avg` 5.91, `paint` 0.9. The 383.8 ms frame is at open (after "first audio
  packet") and the 250.2 ms at teardown (after "player torn down") — same class.
- Audit: eighteen looks benched on the host, 1640×560, 40 frames, one run each.
  Step 1: 27.8 (LIQUID) to 75.4 ms (WORM). Step 3: 11.2 to 17.8 ms. No look fits
  a 60 Hz frame at full resolution even on the host.

### 2026-11-23 — GPU test v2: a log written with kernel calls

- **Cause of v1 being empty: not established.** v1's `freopen` return value was
  discarded, so the build could not report its own failure. Verified in the
  package: its `eboot.bin` does carry `/data/piglet-gpu-test.log` (one string),
  so the redirect was compiled in. See §6 for the ranked causes.
- What changed, in a copy under `_scratch/gpu-test/v2/piglet/` (the v1 folder is
  untouched): a new `piglet/gputest_log.cpp` + `.hpp`, and `piglet/main.cpp`
  reduced to `gt_init()` → `Init()` → `gt_dump_gl()` → loop with `gt_tick()`.
  `PigletApplication.cpp/.hpp`, `ApplicationBase.cpp`, the assets, the modules
  and every draw call are byte-identical to the shipped sample.
- The logger: `sceKernelOpen` with `O_WRONLY|O_CREAT|O_TRUNC` on six paths in
  order, recording each return code; first success gets `dup2` onto fd 1 and 2
  with `setvbuf(stdout, _IONBF)`; a `sceKernelWrite` + `sceKernelFsync` per line
  to every sink that opened. Paths, in order: `/data/piglet-gpu-test-v2.log`,
  `/data/pkg/piglet-gpu-test-v2.log`,
  `/data/ultimate-ps4-media-player/piglet-gpu-test-v2.log`,
  `/mnt/usb0/…`, `/mnt/usb1/…`, `/mnt/ext0/…` (all with the same file name).
- The toolchain's `fcntl.h` defines only `O_RDONLY`/`O_WRONLY`/`O_RDWR`, so the
  module defines the FreeBSD 9 values for `O_CREAT` 0x0200, `O_TRUNC` 0x0400 and
  `O_APPEND` 0x0008 itself. Every open prints its own return code, so a wrong
  flag appears as `path FAIL … rc=0x…` in the log instead of as silence.
- `main.cpp` also queries EGL and GL after `Init()`: `EGL_VERSION`,
  `EGL_VENDOR`, `EGL_CLIENT_APIS`, the config count, `EGL_EXTENSIONS`,
  `GL_VERSION`, `GL_VENDOR`, `GL_RENDERER`, `GL_SHADING_LANGUAGE_VERSION`,
  `GL_MAX_TEXTURE_SIZE`, `GL_MAX_VIEWPORT_DIMS`, `GL_EXTENSIONS` (in 1000-byte
  pieces). Heartbeat: one line a second for the first 30 s of the render loop.
- Package: `TITLE := GPU Test v2 (piglet log)`, `VERSION := 2.00`, same
  `TITLE_ID BREW00093` and content id `IV0000-BREW00093_00-PIGLETGPUR000000`, so
  it replaces the earlier logged build. 6,619,136 bytes. Command, exactly:
  `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 LD_LIBRARY_PATH=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH make CCX=clang`.
- Result: exit 0, `param.sfo` reads `TITLE = GPU Test v2 (piglet log)`,
  `VERSION = 2.00`, `CONTENT_ID = IV0000-BREW00093_00-PIGLETGPUR000000`. `eboot.bin`
  contains `gpu-test-log v2`, `/data/piglet-gpu-test-v2.log`,
  `/mnt/usb0/piglet-gpu-test-v2.log`, `GL_MAX_TEXTURE_SIZE` and `heartbeat` —
  one string each. Archived as
  `files/Piglet-GPU-Test-BREW00093-logged-v2.pkg`, MD5
  **`3f7555a468962dd9e97150f4b3bcb95a`**. No console result exists for it.

### 2026-11-23 — GPU test v3: the stage log (the one to run)

- v2 was built and never run; v3 supersedes it before its first run, so there
  is one package to try, not two. v2 stays archived as a record, not as a
  candidate.
- Added to v3, all in a copy under `_scratch/gpu-test/v3/piglet/`: numbered
  stages — 1 log open, 2 module load, 3 EGL/config, 4 GL strings, 5 texture,
  6 shaders, 7 setup, 8 `Init()` done, 9 first frame, 10 clean exit — plus a
  `frame N presented` line every 60 frames, the exact return code beside each
  `module_start` and `eglChooseConfig`, the EGL/GL string dump of v2, and a scan
  of `GL_EXTENSIONS` for `OES_texture_float`,
  `OES_texture_float_linear`, `EXT_color_buffer_float`,
  `WEBGL_color_buffer_float`, `OES_texture_half_float`,
  `OES_texture_half_float_linear`, `EXT_color_buffer_half_float`,
  `EXT_float_blend`, `EXT_shader_framebuffer_fetch`,
  `NV_shader_framebuffer_fetch` and `EXT_sRGB`, each printed PRESENT or absent
  with its meaning. `GL_MAX_RENDERBUFFER_SIZE` and the implementation color-read
  format/type were added for the same reason: they say what a render target may
  be, without creating one.
- **No on-screen stage text, and deliberately no stage tint either.** A drawn
  number needs a texture and a program the sample does not have, and a tint
  needs an edit in `Render()`, which is the drawing path this build promises not
  to change. What the screen still tells the owner: a rotating cat means the
  frame loop is running; anything else, matched against the last stage line in
  the log, names where it stopped. The heartbeat every 60 frames covers a hang
  inside a frame.
- Files changed against the shipped sample, checked with `cmp`: `main.cpp` and
  `PigletApplication.cpp` (gt() calls between existing statements only), plus two
  new files `gputest_log.cpp`/`.hpp`. Unchanged: `PigletApplication.hpp`,
  `ApplicationBase.cpp/.hpp`, the assets, the modules, the shaders and every GL
  call.
- Package: `TITLE := GPU Test v3 (stage log)`, `VERSION := 3.00`, same title id
  and content id as before, so it replaces earlier samples. `eboot.bin`
  1,316,304 bytes, PKG 6,619,136 bytes. Build command is the one in the v2 entry
  above with the same environment; exit 0. Archived as
  `files/Piglet-GPU-Test-BREW00093-logged-v3.pkg`, MD5
  **`4d556822b249fcf1c3f55a044d806fa2`**.
- Source hashes (sha256): `main.cpp`
  `fef14209bec69037f9065f77c268c866a6cf766e047bfb771a00f23f2ffc5aac`,
  `PigletApplication.cpp`
  `81b91fc42b40156e2d567c4b117785b92b628ef0b73383f36795168bcc74ed1b`,
  `gputest_log.cpp`
  `ab96919a4034281624a1ce7a3528319a555a6ce8a54aab777c29ab7412eadc68`,
  `gputest_log.hpp`
  `601f8f9e2e2422bbdc0c6f24a95b221e2f3172f046e59a51abb5f11d8e3b1877`.
  Strings verified in `eboot.bin`: `gpu-test-log v3`,
  `/data/piglet-gpu-test-v3.log`, `STAGE %d`,
  `OES_texture_half_float` (twice, the token and its linear variant),
  `frame %d presented`. **No console result exists for it.**

### 2026-11-23 — the Piglet v3 log read: the GPU path is confirmed

- Files read, in full: `piglet-gpu-test-v3.log` (8,343 bytes),
  `piglet-gpu-test-v3 (2).log` (7,206 bytes) and `app.log` (60,365 bytes, the
  v117 player run). Nothing was built, no source changed, no package made.
- **The two Piglet files are one run, not two.** Same first line at
  `[    0.026]`, same last line at `[   28.104] frame 3300 presented, still
  drawing`, the same 56 heartbeats to the millisecond. The 1,137-byte difference
  is exactly the sample's own `printf` output (`Loading modules...`,
  `GL_RENDERER: Piglet`, the PNG block, the uniform dump), which only reaches fd 1
  and therefore only the first sink. So the long file is
  `/data/piglet-gpu-test-v3.log` and the short one is one of the two secondary
  sinks (`/data/pkg/` or `/data/ultimate-ps4-media-player/`), which are
  byte-identical to each other (§6).
- **Paths:** `path OK /data/piglet-gpu-test-v3.log (fd 5)`,
  `path OK /data/pkg/… (fd 6)`, `path OK /data/ultimate-ps4-media-player/…
  (fd 7)`, and `path FAIL /mnt/usb0|usb1|ext0/… (rc 0x80020002)`. The `/data`
  root is writable by a title on 12.52; the answer to the v1 mystery is the
  redirect, not the filesystem.
- **Modules:** `module prefix: /BgZIO2hcQg/common/lib/`,
  `Piglet module_start() = 0 (0x00000000), handle 21`,
  `PrecompiledShaders module_start() = 0 (0x00000000), handle 22`. No error, no
  patch step, no devkit files. System modules only.
- **Driver:** `EGL_VERSION: 1.4`, `EGL_VENDOR: Sony Interactive Entertainment
  Inc.`, `EGL_CLIENT_APIS: OpenGL_ES`, `eglChooseConfig: 1 config(s)`,
  `EGL config count: 30`; `GL_VERSION: OpenGL ES 2.0 Piglet`,
  `GL_SHADING_LANGUAGE_VERSION: OpenGL ES GLSL ES 2.0`,
  `GL_MAX_TEXTURE_SIZE: 4096`, `GL_MAX_RENDERBUFFER_SIZE: 4096`,
  `GL_MAX_VIEWPORT_DIMS: 4096 x 4096`. Float and half-float textures PRESENT
  with linear filtering; `EXT_color_buffer_half_float` PRESENT (half-float render
  targets); `EXT_color_buffer_float`, `WEBGL_color_buffer_float`,
  `EXT_float_blend`, both framebuffer-fetch names and `EXT_sRGB` absent.
- **Frame rate:** 60 frames every ~0.5005 s from `[    1.073]` to `[   28.104]`,
  i.e. **119.9 fps**, about 8.3 ms a frame, on a textured quad. The driver
  presents at 120 Hz; the sample's swap does not wait for vsync.
- **Stopped partway, cleanly:** highest stage `STAGE 9  first frame presented`,
  no `STAGE 10` (clean exit) because the owner closed it with the PS button. The
  last heartbeat is an ordinary one, 0.5 s after the one before, so this is a
  close, not a crash.
- Verdict recorded in §5 and §9; what it changes for the visualizers is in
  `files/round7/README.md` §4.

### 2026-11-23 — the v117 log read: nothing regressed, the header needs the screen

- `app.log` only, 60,365 bytes, 83.9 s: run started 23:33:48, build stamp
  `Nov 23 2026 22:44:01`, `build v117`.
- Boot and scan: `2921 tracks (2899 music, 22 video), 1252 pictures, 12380
  entries read in 1518 ms` against v116's 12376 entries in 1539 ms. Same, a
  touch faster.
- Twelve `long frame` lines, all between 0.55 s and 6.45 s: 503.9, 407.7, 541.1,
  223.2, 228.4, 174.6, 133.0, 137.8, 201.0, 524.7, 148.9 and 116.8 ms — boot,
  library scan and the first visualizer frame, the same class v116 had.
- The ladder dropped `1 -> 2` at 8.553 s (119.7 ms, style 17 TESSERACT) and
  `2 -> 3` at 9.209 s (99.4 ms), then never climbed back, so the owner's whole
  eighteen-look tour ran at step 3. Same known behaviour as v116.
- About twenty Triangle presses from 30.5 s to 79.5 s: no long frame, `max`
  66.7–66.8 ms, `worst_paint` up to 40.9 ms (style 16 RAVE DJ at step 3). No
  video file was opened in this run, so the video metadata question is untouched
  and the 383.8/250.2 ms open/close frames did not occur.
- New fact: `boot: libGLESv2 rc=0x80020002` and `libGLESv2 fallback
  rc=0x80020002`, the same code the v3 log gets for an unmounted USB stick —
  there is no `libGLESv2.sprx` to load on this console, which is why SDL's GL
  window fails and why the GL work links Piglet by name instead (§6).

### 2026-11-23 — manifest sync: graphics principles and the three rounds

- Documentation only. No source changed, no build, no package; v117 stays
  current and v118 stays the next player build.
- Compared the owner's consolidated instruction against every section of this
  file and `AGENTS.md`. Four of its six graphics suggestions were already here
  (the proof of concept, the logging round, the television check, and its "own
  package, 1080p, half-float bloom, frame times logged" detail) and were merged
  rather than duplicated. Two were new and are now recorded: the **SDL/Piglet
  handover plan** and the **60 fps budget for all eighteen looks**, both Round 3
  and both plans only.
- Added §9 **Graphics principles**: the research → hypothesis → test → console
  measurement → written rule loop, the three statuses a rule can carry
  (measured, researched only, assumed), and the rules established so far —
  including what is already ruled out by the driver's own capability list.
- Corrected nothing factual: every line of the owner's "where we are" matched
  the file as written, including the narrowing of the empty-log cause to
  `freopen` and the note that the sample's 120 fps drew only a textured quad.
- Added the owner's three unreported observations as pending (above), so they
  cannot be mistaken for findings.

### 2026-11-23 — v118 (logging only) built

- Source changed in exactly two files, logging statements only, no player logic:
  `src/video_orbis.c` (new `src_probe()` helper, `video: SRC`, `video: RATE`,
  `pull_rounds`/`rounds_per_frame` in the `FILE` line and in the periodic
  `video: pulls` line, timed `Stop`, `Close`, teardown and open stages) and
  `src/main.c` (`BUILD_ID "v118"`, and a `library: name` line per non-ASCII
  filename from `log_scan_summary`).
- What the firmware does **not** expose, and therefore what had to be derived:
  `AvStreamInfo` (40 bytes, `_Static_assert`ed) has type, width, height, aspect,
  language, sample rate and duration but **no codec field and no frame rate**.
  So the codec fourcc is read from the file's own header (`ftyp` brand at offset
  4, first sample entry inside `stsd`), the bitrate is file size over duration,
  and the file's nominal rate comes from the decoder's own timestamps — 30 of
  them, which separates 23.976 from 25 from 29.97.
- Command, exactly: `OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain
  PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH
  PKG_LD=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu make`
- Result: build exit 0, **zero compiler warnings**, `make check`
  **ALL TESTS PASSED (0 failures)**. Package
  `files/Ultimate-Media-Player-v118.pkg`, 6,619,136 bytes, MD5
  **`349943cbd65ddb1580a9e47aec7de1b0`**. Strings verified in `eboot.bin`, one
  each: `v118`, `video: SRC`, `video: RATE`, `rounds_per_frame`, `utf8_bad`.
- Source hashes (sha256): `src/main.c`
  `efeab9cdf2a8b454a07f224b693cb171412095806c6cdb93f805a6f91fbaa792`,
  `src/video_orbis.c`
  `8a702848389a82fa9acd8298c6284d7965a705a9b150a876696eefc26eabf9bb`.
  **v117 stays the rollback** (`290e7edab55e864e35d497b369828193`).
- **No console result exists for v118.** The host cannot answer any of its
  questions: the fourcc, the rate, the bitrate and the filename bytes only exist
  on the console.

### 2026-11-23 — Round 2 started, and stopped at the shader gate

- **The finding, verified by listing the toolchain:** OpenOrbis 0.5.2 ships
  **no GLSL compiler and no shader-binary tool**. `$HOME/openorbis/PS4Toolchain/bin/linux`
  contains exactly `LICENSE.txt`, `PkgTool.Core`, `README.md`, `create-fself`,
  `create-gp4`, `readoelf` — and `bin/` has nothing else. Nothing named
  `glsl*`, `shader*`, `orbis-*` exists anywhere in the toolchain.
- What the sample does instead, read in
  `samples/piglet/piglet/PigletApplication.cpp`: it declares
  `extern "C" OrbisShaderBlob scePrecompiledShaderEntries[]` (line 29), walks that
  table in `fetchShaderByName()` (line 36) and prints
  `Found shader texmap/v_2.vert at 0x… of size …`, and the comment at line 356
  says where they come from — *"these shaders come from precompiled lib, the
  webkit ones ;-;"*. They are fetched from `libScePrecompiledShaders.sprx`, a
  system module. **A program can only use shaders whose blobs are already in
  that table.**
- The runtime compiler is not an option either: `libSceShaccVSH.sprx` is the
  devkit shader compiler, and the v3 run showed the sample never asking for it
  (§9). Writing new GLSL therefore has **no path on this console today**.
- **What that costs the Round 2 plan as written:** a bloom chain (bright-pass,
  separable blur, composite), a per-pixel liquid/refraction look, and a raymarch
  all need fragment programs we cannot produce. Fixed-function ES2 can still do
  textured quads with additive blending, so *some* of the chain (downsample,
  additive accumulation, tint) is reachable — but a true bright-pass threshold
  is not, and neither is any per-pixel effect.
- **Not built and not packaged this round.** The next package is the one that
  makes this decidable instead of an argument: a dump of the whole
  `scePrecompiledShaderEntries[]` table — every name and size the system module
  carries — plus a real half-float framebuffer test and a timed 1080p fill loop.
  If the table holds a blur, a composite or a gradient program, the bloom chain
  is assemblable from existing blobs; if it does not, the visualizer work has to
  go through GNM or the experimental stacks (§9 index), not Piglet ES2.
- The player is untouched; v118 remains the player build and there is no new
  package.

### 2026-11-23 — the shader catalogue package (BREW00094) built

- New scratch tree `_scratch/gpu-test/cat/`, copied from the v3 tree, sources in
  `cat/` (the sample's Makefile expects a subdirectory named like the working
  dir). `main.cpp` replaced: the render path of the sample is untouched, and the
  new code runs before the loop.
- What it does, in order: initialise as the sample does; `gt_gl_info()`; walk
  `scePrecompiledShaderEntries[]` and log every `ident` and `len` (the table
  layout is copied from `PigletApplication.cpp:28-32`, since it is private to
  that file); then two framebuffer tests — RGBA with type `0x8D61`
  (`GL_HALF_FLOAT_OES`) and RGBA with `GL_FLOAT`, 480x270 each — logging
  `glTexImage2D`'s error, `glCheckFramebufferStatus`, and a clear plus `glFinish`
  when the target is complete; then the sample's normal render loop, so the
  screen still shows its rotating quad and the heartbeats prove the loop ran.
- Package identity: title **GPU Shader Catalogue**, version **1.00**, title id
  **BREW00094**, content id `IV0000-BREW00094_00-GPUSHCAT00000000` — its own
  title, so it neither replaces the player nor the earlier BREW00093 samples, and
  no player version number is used.
- Log: `/data/gpu-shader-cat.log` (plus `/data/pkg/` and
  `/data/ultimate-ps4-media-player/`), same fsynced kernel-call writer, same
  six-path table, first line `gpu-cat-log v1`. Stages 11 (catalogue) and 12
  (render targets) are new; the old 1-9 stages still fire.
- Build: exit 0, **zero errors**, zero warnings. Package
  `files/GPU-Shader-Catalogue-BREW00094.pkg`, 6,619,136 bytes, MD5
  **`107771998233b11c633005c64c064860`**. Strings verified in `eboot.bin`:
  `gpu-shader-cat.log` (6, one per path), `catalogue:` (4),
  `TARGET half-float` (1). Command, exactly:
  `DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
  LD_LIBRARY_PATH=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu
  OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain
  PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH make CCX=clang`.
- Two build failures on the way, both layout, not code: the Makefile derives
  `PROJDIR` from the working directory's name and expects sources in a
  subdirectory of that name (v3 worked because its dir was called `piglet`), and
  `OrbisShaderBlob` is private to `PigletApplication.cpp`, so its layout had to
  be restated in `main.cpp`.
- **No console result exists.** The catalogue count and the half-float verdict
  are the two things this package exists to print.

### 2026-11-23 — GNM checked, and the 60 fps skeleton written

- **GNM does not escape the shader gate by itself.** `include/orbis/GnmDriver.h`
  is present and the toolchain carries `libSceGnmDriver.so` plus its compat and
  debug companions, so drawing is linkable — but GNM takes **shader binaries**, and
  the toolchain still ships no compiler for them. The only offline PS4 shader
  compiler seen so far is OpenGNM's `opengnm-psbc`, reported hardware-validated
  on FW 9.00 and **not installed here**; that is the next thing to try, not a
  conclusion. Marked **researched only**.
- So the order stands: the catalogue decides whether Piglet can do it from
  existing blobs; if not, GNM via OpenGNM is the next test, and it needs its own
  compiler working on 12.52 before anything is drawn.
- `files/round7/README.md` §5 now holds the **60 fps budget skeleton**: the
  16.7 ms split across audio analysis, scene, bloom and present, every cost marked
  unknown; the three tiers the eighteen looks fall into; and a five-step fallback
  ladder with what each step trades. It is structure, not a plan, and it is
  labelled that way so nobody reads it as measured.
- No build, no package, no console work. v118 and the catalogue package are both
  built and unchanged.

### 2026-11-23 — manifest: owner statements are folded in automatically

- Rule added (owner's instruction): information the owner gives that bears on the
  build, on this console, on the toolchain or on future projects is written into
  the matching section on arrival, filtered by usefulness rather than volume, with
  its date and status; contradictions keep both readings and a plan to test. This
  is what keeps the file a knowledge base rather than a build diary.
- No source, no build, no package. v118 and the catalogue package are unchanged
  and still awaiting their runs.

### 2026-11-24 — the catalogue log read: a bloom chain CAN be assembled

- Files read: `gpu-shader-cat.log` (17,477 bytes) and `app.log` (92,437 bytes,
the v118 player run). Quoted below.
- **The table is not just texmap. It has 82 entries:**
  `catalogue: 82 entries, 82 printed`. The useful ones, by name from the log:
  `cssfilters/blur1.vert` + `cssfilters/blur1.frag` (1076/904) and
  `cssfilters/blur2.vert` + `cssfilters/blur2.frag` (1076/904) — a **separable
  two-pass blur pair**; `cssfilters/brightness.frag` (548) and
  `cssfilters/contrast.frag` (580) for a bright-pass approximation;
  `cssfilters/opacity.frag` (540) and `texmap/f_2.frag` (552) for the composite;
  `swordfish/fpBlur.frag` (872), `swordfish/fpAlphaTextureFade.frag` (704),
  `polymer/orbis_highlight.frag` (788); plus the whole CSS filter set
  (grayscale, sepia, hue_rotate, saturate, invert, dropshadow1/2, transform) and
  `khronos/earth` and `khronos/spiritbox`.
- **Half-float targets are real, not just advertised:**
  `TARGET half-float RGBA16F 480x270 status=0x8CD5 COMPLETE` and
  `TARGET half-float RGBA16F clear+finish glerr=0x0000`. The negative control
  behaved as predicted: `TARGET float RGBA32F 480x270 status=0x8CDD INCOMPLETE`
  with the driver's own words `Texture attachment format not supported
  1908:1908:1406`.
- `TARGET verdict half_float=USABLE float32=not usable (bloom needs half-float)`.
- The render loop ran to 5,940 frames in 50.8 s — 117 fps on the sample's quad,
  so the app and the swap are healthy after the FBO tests.
- **Decision: bloom is assemblable on Piglet, and the proof of concept can be
  built.** Scene into a half-float target, bright-pass approximated by
  `brightness`+`contrast` (or by drawing only emissive geometry, which needs no
  threshold at all), two separable blur passes, composite through `texmap/f_2`
  with additive blending. What is still missing is any per-pixel custom maths —
  no fluid solve, no raymarch, no refraction — because no such program exists in
  the table and none can be compiled.
- Two defects found in the catalogue package itself, not in the driver: its log
  header still says `package: GPU Test v3 (piglet stage log), title BREW00093`
  (stale string in `gputest_log.cpp`), and the `video: SRC` probe in v118 printed
  `codec=` empty — see §6.

### 2026-11-24 — GPU Bloom POC built (BREW00095, no player version)

- Package: `files/GPU-Bloom-POC-BREW00095.pkg`, 6,619,136 B, MD5
  `542ce735b6bf96d90056eef7f29785c4`; content id
  `IV0000-BREW00095_00-GPUBLOOM00000000`, title "GPU Bloom POC" 1.00. Build
  exit 0, no warnings, no errors in `build.log`.
- Log: `/data/gpu-bloom.log` (`gpu-bloom-log v1`), then `/data/pkg/`,
  `/data/ultimate-ps4-media-player/` and the USB sinks.
- What it does: draws one 1920x1080 scene (CPU plasma with emissive blobs) into
  a **half-float RGBA16F target**, bright-passes at 480x270 with
  `cssfilters/brightness`, runs `cssfilters/blur1` then `blur2`, and composites
  scene + bloom with `texmap/f_2` and `glBlendFunc(GL_ONE, GL_ONE)`. FBOs are
  created once and bound, never per frame, so the timings are not polluted.
- It logs **every uniform and attribute each program declares**
  (`UNI <tag> name type=0x…. size=…`, `ATT <tag> …`), which is the unknown that
  could stop the chain at the first draw: the blur and brightness programs' names
  are not documented anywhere and are only known once the console prints them.
- Timings per 60-frame report: `BLOOM frame=… fps=… span=… scene=… bright=…
  blur1=… blur2=… comp=… total=…ms` from `sceKernelGetProcessTime()`, plus
  `BLOOM glerr=0x….`
- Source: `_scratch/gpu-test/bloom/bloom/main.cpp` (new); the rest of the tree is
  the BREW00094 catalogue copied forward, with the log tag, title, title id and
  content id updated. Known cosmetic leftover: only the first sink path was
  renamed, so sinks 2-6 still say `gpu-shader-cat.log`.

### 2026-11-24 — the bloom log: the chain compiles, then dies at the first frame

Log: `gpu-bloom.log`, 8,345 bytes, pulled 2026-11-24. **No frame was ever drawn,
so this package produced no timings.** What it did produce is worth more than
timings were, because it closes the gate the whole GPU port was waiting on.

- **All five programs link.** `PROG texmap link=1 glerr=0x0000`, then
  `bright`, `blur1`, `blur2`, `comp`, all `link=1` with a clean error state. The
  fear that the blur blobs would not assemble was wrong.
- **The uniform set is now known, from the console, not guessed:**
  `UNI bright u_filterAmount type=0x1406` (float);
  `UNI blur1 u_blurRadius type=0x8B50` (vec4 — an x/y radius pair with room for
  two more components) **[superseded 2026-11-24: `0x8B50` is `GL_FLOAT_VEC2`, not
  vec4 — see AGENTS.md amendment A2]**; `UNI blur1 u_gaussianKernel[0] type=0x1406 size=11` —
  **an 11-tap Gaussian kernel**, identical in `blur2`. Both blur programs also
  carry `s_sampler`; all five carry the three matrices and `a_vertex` at
  location 0.
- **1080p half-float works:** `TARGET scene16f 1920x1080 status=0x8CD5 COMPLETE`,
  and both 480x270 bloom targets complete as well. The catalogue only proved
  480x270; the full-size target was the one that could have failed.
- **Then it stopped:** `glViewport failed: 502` one line after
  `STAGE 12  bloom: entering the timed loop`, followed by
  `Exit is requested, quitting...` and a **clean** shutdown
  (`reaching this line means a clean exit, not a kill`). So the app did not
  crash: our own error check asked it to quit. Nothing after the first
  `glViewport` ran, which is why there is not a single `BLOOM frame=` line.
- The likely cause is stated as a hypothesis, not a finding: the sample's
  `Render()` checks `glGetError` after its own `glViewport`, so an error left
  pending by my setup — most plausibly from the first `glViewport` after binding
  a freshly created FBO, or from a call the sample was not written to expect —
  surfaces there and is reported as a viewport failure. 502 is not a GL error
  enum, so it is Piglet's own code. The next package instruments every setup
  call with its own `glGetError` tag to name the call that raises it.
- Second defect confirmed in the log: the secondary sinks still carry the old
  name — `path OK /data/pkg/gpu-shader-cat.log (fd 6)` and the
  `ultimate-ps4-media-player` one — because only the first path was renamed.
  Harmless, but it writes a catalogue log from the bloom package.

### 2026-11-24 — BLOOM 2 built (BREW00096): per-call error tagging

- Package: `files/GPU-Bloom-POC-BREW00096.pkg`, 6,619,136 B, MD5
  `133b70e48f7d2c5ebe5145ab0be4160b`; content id
  `IV0000-BREW00096_00-GPUBLOOM96000000`, title "GPU Bloom POC v2"; build exit 0,
  no warnings. Log: `/data/gpu-bloom96.log`, tag `gpu-bloom-log v2 (BREW00096)`
  — a **new name**, so the v1 log with the 502 stays intact for comparison.
- One change only, the diagnosis: every setup call and every pass now reads and
  clears `glGetError` through `chk(tag)`, which logs only on error as
  `CHK <tag> glerr=0x….`. Tags: `scene16f`/`bloomA16f`/`bloomB16f` from the
  target builder, `src-tex`, `vbo`, `pre-loop`, `frame0-entry`, `pass-scene`,
  `pass-bright`, `pass-blur`, `pass-comp`, `pre-render`, `post-render`.
- `pre-render` is the decisive one: it reads the error state **before** the
  sample's own `Render()` touches `glViewport`, so whichever line appears says
  whether the 502 was raised by my frame setup (a `pass-*` tag) or was already
  pending when the sample ran (`pre-render`), or belongs to the sample itself
  (`post-render`).
- Nothing else moved: same five programs, same three targets, same pass order,
  same timing report. The renames, title and content id are the only other
  differences from BREW00095.

### 2026-11-24 — the BLOOM 2 log: the chain runs, 85–100 fps, and the timings are not GPU timings

Log: `gpu-bloom96.log`, 344,071 bytes, a long run. **The chain runs end to end.**
Every program links, every target is complete, and nothing on our side raises a
GL error: `BLOOM glerr=0x0000` after every report.

- **Frame rate: 77–114 fps, typically 85–100.** `BLOOM frame=59 fps=113.9
  span=527ms scene=4.82 bright=0.03 blur1=0.00 blur2=0.04 comp=0.03
  total=8.78ms` and `frame=1799 fps=78.2 span=767ms scene=2.93 bright=0.02
  blur1=0.00 blur2=0.04 comp=0.03 total=12.78ms`. That is a 1080p half-float
  target, a bright pass, two blurs and a composite — around a quad, not a look.
- **The 502 is not ours.** Every frame logs `CHK post-render glerr=0x0502` and
  never `pass-scene`, `pass-bright`, `pass-blur`, `pass-comp` or `pre-render`.
  0x0502 is `GL_INVALID_OPERATION`, raised inside the sample's own `Render()`:
  our state (bound FBO, viewport, program) is not what its quad expects. It is
  harmless to the measurement and must not ship — the sample's draw has to go, or
  its state has to be restored around our passes.
- **The per-pass numbers are submission times, not GPU times.** `blur1=0.00` and
  a 1920x1080 composite at `comp=0.03` cannot be real work; only `scene` at
  2.6–4.9 ms looks like anything, and that is the pass whose FBO bind and clear
  force a flush. `sceKernelGetProcessTime` around `glDrawArrays` measures how
  long it took to hand commands to Piglet, not how long the GPU took.
- **So the honest answer to "can a visualizer hold 60 fps" is still no.** What
  this run proves is that the path executes and that a trivial scene leaves
  ~9–13 ms a frame to spare, not what a real look costs. The next package must
  measure with `GL_EXT_disjoint_timer_query` (present in the extension list)
  and/or `glFinish` at pass boundaries, and must draw a scene with real work in
  it before any number is quoted as a budget.
- Defect to fix before any port: the sample's own texture draw still runs inside
  the loop and throws 0x0502 every frame. A port cannot inherit that.

### 2026-11-24 — BLOOM 3 built (BREW00097): sample draw out, GPU timings, real workload

- Package: `files/GPU-Bloom-POC-BREW00097.pkg`, 6,619,136 B, MD5
  `eafc1a23fd0f751d3c9c42c1d3a95e85`; content id
  `IV0000-BREW00097_00-GPUBLOOM97000000`; build exit 0, no warnings. Log
  `/data/gpu-bloom97.log`, tag `gpu-bloom-log v3 (BREW00097)`.
- Three changes, all of them measurement fixes, none of them look changes:
  1. **The sample's draw is gone.** `Render()` now runs the clear, blend and viewport
     and then skips `doFrame()` behind a `g_skip_sample_draw` flag declared in
     `PigletApplication.cpp`; `eglSwapBuffers` still runs, so the screen still
     presents. The per-frame `GL_INVALID_OPERATION` had no other source, so it
     should disappear from the log entirely — that is the check.
  2. **GPU-side timings** **[superseded 2026-11-24: per-pass `glFinish` measures
     its own ~13–14 ms round trip, not the pass — see AGENTS.md amendment A1]**.
     `glFinish()` at every pass boundary, plus
     `EGL_SCE`-style entry points looked up with `eglGetProcAddress` for
     `EXT_disjoint_timer_query` (`glGenQueriesEXT`, `glBeginQueryEXT`,
     `glEndQueryEXT`, `glGetQueryObjectui64vEXT`). `TIMER mode=` in the log says
     which is in use: 1 query, 2 finish-only. If the entry points are absent the
     finish timings still stand, so the run cannot be wasted.
  3. **Real workload.** The scene is now a full-screen ramp plus **2,000 additive
     sprites in one draw call**, 1080p into the half-float target; the bright
     pass runs at 480x270 with `u_filterAmount 0.6`; both blurs now set
     `u_blurRadius` and an 11-tap Gaussian kernel computed as a normalized
     sigma estimate — the first time the blur programs have been driven with
     their real uniforms rather than defaults.
- Report lines: `GPU scene=… bright=… blur1=… blur2=… comp=… total=…ms` and
  `BLOOM frame=… fps=… sprites=2000 glerr=0x…`.
- What it will and will not settle: it gives the cost of a 1080p pass chain with
  a sprite load in milliseconds of **GPU** time, which is the number the 60 fps
  plan needs. It does not measure a specific visualizer, and 2,000 sprites is a
  stand-in chosen for load, not a look.

### 2026-11-24 — the BLOOM 3 log: my measurement is the bottleneck, and a uniform bug broke the blur

Log: `gpu-bloom97.log`, 349,146 bytes, 92 s, 22 reports at a steady **15 fps**.
The run is honest and the numbers are stable, which is exactly what makes them
easily misread.

- `TIMER mode=2 (glFinish only) gen=0 get64=1 get32=1` — the query path was not
  available (`glGenQueriesEXT` missing although the two `glGetQueryObject*EXT`
  entry points resolve), so every pass was timed with `glFinish()`.
- **Headline numbers, stable to ±0.2 ms for 90 seconds:**
  `GPU scene=18.30 bright=14.61 blur1=16.86 blur2=0.00 comp=14.44 total=66.73ms`,
  `BLOOM frame=1319 fps=15.0 sprites=2000 glerr=0x0000`.
- **Those are not GPU costs.** `bright` at **480x270** costs 14.6 ms while
  `scene` at **1920x1080** costs 18.3 ms — sixteen times the pixels for four
  milliseconds more. A 480x270 pass cannot cost 14 ms of fill. The fixed ~13–14 ms
  per pass is the `glFinish` round trip, not the work. Four finishes a frame is
  the 55 ms difference between this run's 66.7 ms and v2's 10.3 ms with the same
  chain and no finish.
- **A real bug made `blur1` wrong on top of that.** The log carries
  `[PIG]E-glUniform4f:8900 - illegal to set uniform data with different types`
  and `Failed to retrieve uniform info!` once per blur setup, then
  `CHK pass-blur glerr=0x0502`. The cause is mine: `u_blurRadius` is
  **`0x8B50`, which is `GL_FLOAT_VEC2`**, not the vec4 I called it in the last
  report; I set it with `glUniform4f`, Piglet rejected the type, the radius and
  the kernel were never applied, and `blur2` reading 0.00 ms is a symptom of the
  error state, not a fast pass. §9's earlier note calling it a vec4 is corrected
  here.
- **What the run does prove:** 2,000 additive sprites plus a ramp at 1080p, a
  bright pass, two blurs and a composite all execute with `glerr=0x0000` at
  report time and no crash for 92 seconds; the chain is stable over time (no
  thermal drift visible: 14.9–15.0 fps for 84 of the 92 seconds, with three
  dips to 11–13 fps).
- **What it does not prove:** whether any of this fits 16.7 ms. The apparatus
  dominates the measurement, so the honest state is "unknown, and the previous
  package's 10.3 ms is the only end-to-end number we have".

### 2026-11-24 — other Piglet projects consulted, then BLOOM 4 built (BREW00098)

Research first, per the new "existing work is a starting point" rule. Four
findings changed the design, all from other people's working code:

- **`alechurri/shipofharkinian-ps4` `docs/TECHNICAL.md`** — Piglet is GLES 2.0 /
  GLSL ES 1.00; **no `glBlitFramebuffer`**, framebuffer copies are done by drawing
  a textured quad (exactly our composite); no VAOs, no MSAA, unsized texture
  formats; the depth buffer cannot be read back; **`eglSwapInterval(1)` set after
  `eglMakeCurrent` makes `eglSwapBuffers` block forever** — it must be set before
  the surface is created; and `scePigletSetConfigurationVSH` accepts almost
  anything, failing later as "Out of memory". Their working memory split:
  **128 MiB system shared, 512 MiB video shared, 236 MiB flexible, 4 MiB command
  buffers**, with textures and render targets taken from the flexible limit, and
  a 1080p framebuffer pair needing **more than 64 MiB**.
- **`Mayo1970/ioQuake3-PS4`** — 124 shader blobs compiled offline and loaded with
  `glShaderBinary` at boot, the same route we are on; **depth-only FBOs return
  `GL_FRAMEBUFFER_UNSUPPORTED`**; and they report **"no `GL_RGBA16F` in GLES 2"**,
  forcing RGBA8. That does not contradict our measurement — it names the trap:
  the *internalformat* `GL_RGBA16F` does not exist in ES2, while
  `internalformat GL_RGBA` with **type `GL_HALF_FLOAT_OES` (0x8D61)** does work,
  which is what our `TARGET … 0x8CD5 COMPLETE` line proves.
- **`Navinas3460/Stremio-PS4`** — same `eglSwapInterval(g_display, 0)` workaround
  with the identical comment "with vsync on, eglSwapBuffers blocks forever on
  Piglet", and the same `scePigletSetConfigurationVSH` then `eglGetDisplay`
  order we use.
- **Timing, from the general literature rather than PS4 code** (no PS4 project
  was found that times individual passes): `glFinish` per pass is the classic
  mistake — it stalls both CPU and GPU; the accepted techniques are timer
  queries, fence syncs, or simply counting swaps. That is rule R15 arriving from
  the other direction.

What was built from it: `files/GPU-Bloom-POC-BREW00098.pkg` (renamed
`GPU-Bloom-Cost-Probe-BREW00098.pkg` on 2026-11-24 to say what it measures; same
MD5, same bytes), 6,619,136 B, MD5
`a5607cc90de5a17491566a4e949ef4d1`, content id
`IV0000-BREW00098_00-GPUBLOOM98000000`, log `/data/gpu-bloom98.log`
(`gpu-bloom-log v4`). Build exit 0, no warnings.

- **No `glFinish` anywhere.** Frame time is measured around the four passes plus
  the swap, so the swap's own pacing is inside the number and nothing is added
  by the measurement.
- **Five configurations, 120 frames each, cycled and repeated:**
  `A empty` (clear + swap only — the floor), `B scene` (ramp + 2000 additive
  sprites into the half-float target, then composited to the screen), `C bloom`
  (B plus bright, blur1, blur2, additive composite), `D bloom6k` (C with 6,000
  sprites), `E bloomHalf` (C with 960x540 bloom targets instead of 480x270).
  Each logs `CONFIG <name> frames=120 ms=… fps=… sprites=… bloom=… targets=…
  err=0x…`.
- **The differences are the measurement:** B−A is the cost of the 1080p scene,
  C−B is the bloom chain, D−C is 4,000 more sprites, E−C is double-resolution
  bloom targets. No timer query needed, which matters because
  `glGenQueriesEXT` does not resolve on this firmware.
- **`u_blurRadius` is now set as a vec2** (`glUniform2f`, horizontal
  `(1/w, 0)`, vertical `(0, 1/h)`) per rule R16, with the 11-tap kernel
  normalized — the first build in which the blur has its real uniforms.

### 2026-11-24 — BREW00098, the bloom cost probe (two of five configs)

- Built `files/GPU-Bloom-Cost-Probe-BREW00098.pkg` (renamed from
  `GPU-Bloom-POC-BREW00098.pkg` the same day; same bytes), MD5
  `a5607cc90de5a17491566a4e949ef4d1`. Ran on the console, log
  `/data/gpu-bloom98.log`, 4,107 bytes, 120 frames per config.
- What ran: modules loaded (`Piglet module_start() = 0`, handle 21;
  `PrecompiledShaders module_start() = 0`, handle 22), EGL 1.4, four programs
  linked (`PROG texmap link=1`, `bright`, `blur1`, `blur2`), five half-float
  targets completed at 480x270 and 960x540 plus 1920x1080
  (`status=0x8CD5 COMPLETE`).
- What the loop produced:
  `CONFIG empty frames=120 ms=8.19 fps=122.1 sprites=2000 bloom=off targets=0x0 err=0x0000`
  and `CONFIG scene frames=120 ms=8.32 fps=120.2 … err=0x0000`.
- Where it stopped: `[PIG]E-glUniform2f:8390 - illegal to set uniform data with
  different types`, `[PIG]E-glUniform1fv:8073 - Failed to retrieve uniform info!`,
  `glViewport failed: 502`, `Exit is requested, quitting...`. Configs C, D and E
  never ran. The exit was clean (`reaching this line means a clean exit, not a
  kill`), so this is a caught error, not a crash of the console app.
- Two findings that change the method: the 502 belongs to the uniform calls, not
  to `glViewport` (GL errors are sticky and were read one stage late); and both
  configs landed on the ~8.3 ms present floor, so differencing cannot separate
  draw cost from the flip — 2,000 sprites at 1080p differed by 0.13 ms. Became
  rule R20 and amendment A3; the next probe measures by repetition slope.
- Prediction verdicts: P0 missed (config C errored), P1 hit (8.19 ms), P2 void
  (the floor swallowed it), P3–P5 not run. The differencing method stated in the
  register is [superseded 2026-11-24 by A3 — see §9 R20: measure by repetition
  slope, keep differencing only for work above the floor].
- The banner in the log reads `package: GPU Bloom POC v2 (BREW00096)` while our
  own line reads `=== gpu-bloom-log v4 (BREW00098) ===`: the sample's version
  string was never updated for 98, so the log carries two identities. Not fixed.

### 2026-11-24 — publishing handover, and the defect it caught

- The publishing assistant ran the archive (MD5 `ac88446bc6e752943dbcc2e55822c56f`):
  hash matched, `check-links: 52 pages, 0 problems`, `md5sum -c` clean.
- Then it stopped before pushing, correctly: the instruction to relocate the root
  README to `docs/ROOT-POINTER.md` produced `FAIL docs/ROOT-POINTER.md ->
  docs/README.md` and `-> AGENTS.md`, because the pointer's links are
  root-relative. It refused to push a red `main`. Credentials were also absent on
  its side, so nothing was pushed.
- Fixed in the generator and the push script: the pointer is never copied over a
  real README, and the original README content is kept with the block prepended.
  Archive rebuilt, 62 files, MD5 `585ed7051a485e9fb78b045f07c3dc14`.
- Recorded in §6: a generated file is verified where it will live, not where it
  was made.

### 2026-11-24 — second review pass before publishing

- The outside reviewer verified the archive (MD5 `585ed7051a485e9fb78b045f07c3dc14`), then stopped
  again rather than push: `docs/RULES.index` carried 14 measured rules with an empty `evidence`
  field, and the CI assertion `measured rule with no evidence` would have failed. Cause: the
  generator validated the evidence cell but wrote a different field.
- Also corrected: the handover prose stated 60 files and 52 pages; the next build was 62 and 54.
  The prose no longer states counts — `CHECKSUMS.txt` states contents.
- Both recorded in §6. The generator now fails when a measured rule reaches the index without
  evidence, so the local check and the CI gate test the same thing.
