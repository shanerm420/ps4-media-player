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

### 2026-11-24 — the documentation tree is published on GitHub

- Pushed to `shanerm420/ps4-media-player` `main` as three commits in the
  prescribed order: `23e9ec4` rules, amendments, history → `213dfd6` the manifest
  in twelve parts → `0098f58` index, rules file, board and checkers; annotated tag
  `docs-2026-11-24` on the tip.
- Verified from outside afterwards: 61 files added, **0 modified, 0 deleted**;
  every one of the 61 byte-identical to the archive (`cmp`, 0 differ);
  `md5sum -c CHECKSUMS.txt` 60/60 OK; `check-links.py` `0 problems` on the
  repository copy; `README.md`, `src/`, `test/`, `preview/`, `third_party/`,
  `Makefile`, `LICENSE`, `THIRD_PARTY.md` all untouched against `fa187cc`.
- CI: workflow `docs` green on both the `main` push and the tag push.
- One deviation, harmless: topics were set in the requested set but GitHub
  returns them unordered, so the stored order differs from the prompt. A revised
  prompt should say "these topics, order not significant".
- The archive was rebuilt immediately before this pass to drop the root
  `README.md` it used to carry, so that copying it over the repository could not
  overwrite the existing one. MD5 `4e23426b1f3987141735b7a244043876`.

### 2026-11-24 — BREW00099, the repetition-slope probe (built, not run)

- Why: BREW00098 measured `empty` at 8.19 ms and `scene` at 8.32 ms, so
  differencing could not see the work at all (§9 R20, amendment A3). This build
  draws the same chain N times per frame, N = 1, 4 and 16, and reads the slope.
- Source: `_scratch/gpu-test/bloom99/`, from the BREW00098 tree. `main.cpp` is
  new; `gputest_log.cpp` has the banner corrected to `v5 (BREW00099)` and the
  log moved to `/data/gpu-bloom99.log`; `PigletApplication::Render()` returns
  after the swap when the probe owns the frame, because its `glGetError` checks
  had turned the probe's first sticky error into `glViewport failed` and a clean
  exit - the mechanism that ended BREW00098 after two of five configurations.
- Built with: `make CCX=clang` and
  `LD_LIBRARY_PATH=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu
  DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain
  PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH`. The compiler cache was rebuilt
  first with `sh files/bootstrap-toolchain.sh` (clang 18.1.8).
- Result: `IV0000-BREW00099_00-GPUBLOOM99000000.pkg`, MD5
  `ab8a38d0b8c71dfc4df9a2f67e348fa2`, archived as
  `files/GPU-Bloom-Slope-Probe-BREW00099.pkg`.
- Host check before packaging (no console involved): the probe compiled against
  stub GL headers and ran to completion, and with the stub charging 1 ms per
  draw the fit returned **3.005 ms per pass against an expected 3.000, r2
  1.0000**, with the floor reported separately as the intercept. That verifies
  the state machine and the arithmetic; it says nothing about this GPU.
- Not run on the console. No number in this entry is a console measurement.

### 2026-11-24 — BREW00099 on the console: the isolation held, the setters did not

- `CONFIG empty n=0 frames=60 ms=16.68 fps=53.1 state=ok err=0x0000` — 16.68 ms
  is one vsync, so the repeated swap was being timed and would have pinned every
  configuration to the same number.
- `CONFIG n1 … frames=1 ms=15.99 state=VOID err=0x0502`,
  `n4 … state=VOID err=0x0502`, `n16 … state=VOID err=0x0502`. The log carries
  `[PIG]E-glUniform2f:8390 - illegal to set uniform data with different types`
  and `[PIG]E-glUniform1fv:8073 - Failed to retrieve uniform info!`.
- The driver's own introspection says `u_blurRadius type=0x8B50 FLOAT_VEC2
  size=1 loc=49` and `u_gaussianKernel[0] type=0x1406 FLOAT size=11 loc=51`, so
  the type was right and the **location** is what the setter will not accept.
  A2 amended by A4.
- **What the build got right:** the per-config isolation held. Four
  configurations ran, three were voided by name, the run reached STAGE 13 and
  left a complete log. BREW00098 died after two of five; BREW00099 finished and
  reported. No cost number exists from this run and none is claimed.
- The on-screen panel came back as an empty black rectangle: the 1024x256 glyph
  atlas filled with `glTexSubImage2D` did not arrive, so the panel drew and the
  words did not. Owner report, with a photograph, 2026-11-24.

### 2026-11-24 — BREW00100 (built): probe the setter, time without the swap, draw the words as geometry

- Three changes, each from the run above. (1) Every uniform's set is tried once
  at link, the error drained and printed as `SET <prog> <name> loc=… type=…
  form=… -> accepted|REFUSED 0x…`; a refused uniform is skipped from then on, so
  the chain runs and its cost stays measurable. (2) The timed window closes at
  `glFinish`, before the swap, and the swap is reported separately as `swap_ms`.
  (3) The results panel is glyph quads over a 1x1 white texture, so nothing
  depends on a texture upload arriving.
- Host check, with the console's exact refusals simulated (2f refused, sticky
  0x0502): all four configurations ran,
  `RESULT slope ms_per_pass=3.004 intercept_ms=7.153 r2=1.0000 points=3` against
  a stub that charges exactly 3.000 ms per pass. The code path is verified; the
  GPU is not.
- Built with `make CCX=clang` and the same environment as BREW00099. Content ID
  `IV0000-BREW00100_00-GPUBLOOM10000000` (36 characters - the longer first
  attempt was rejected as `Content ID must be 36 characters long`).
- `files/GPU-Bloom-Slope-Probe-BREW00100.pkg`, MD5
  `cd5891a8fb36e8beb23a600cee69dc7f`. Not run on the console.

### 2026-11-24 — BREW00101: one folder for every probe log

- Owner asked for all log files in one place so they stop having to hunt for
  them. From this build on every probe log goes to
  **`/data/Alpha PS4 test kit/`**: `gt_init()` calls `sceKernelMkdir` on it at
  startup and logs the return code, then opens `<folder>/gpu-bloom101.log` as
  the first sink, with plain `/data/gpu-bloom101.log` as the second so a failure
  to create the folder still leaves a log.
- Everything from BREW00100 is unchanged: the per-uniform set probe (A4/R21), the
  swap-free timing window, and the results panel drawn as glyph quads.
- `files/GPU-Bloom-Slope-Probe-BREW00101.pkg`, MD5
  `0bf6f9ef61672e0494026256d1f50747`. Not run on the console.
- A copy of every log already pulled from the console now sits in the workspace
  at `files/Alpha PS4 test kit/`, with a read-me that lists which console path
  each older build wrote to. The player's own `app.log` stays where it is: moving
  it would break every dated instruction that says to pull it from
  `/data/ultimate-ps4-media-player/`.

### 2026-11-24 — BREW00101 on the console: the folder worked, the floor moved, the cause was ours

- The log landed at `/data/Alpha PS4 test kit/gpu-bloom101.log` as designed, and
  the panel drew as glyph quads; still no readable words on the photograph.
- All five configurations ran and reported `state=ok err=0x0000`, and every one
  measured the same number: `draw_ms=16.65` for N = 0, 1, 4 and 16. So the floor
  is not the flip any more, it is the **vblank**: the swap of the previous frame
  is already being waited on when the window opens.
- `SET` answered **`REFUSED 0x0502` for every uniform** with
  `[PIG]E-glUniform2f:8372 - Invalid program!` and
  `[PIG]E-glUniformMatrix4fv:9260 - Invalid program!`. That is the real cause of
  everything since BREW00097: `glUniform*` acts on the program that is **bound**,
  and none was. BREW00097's "different types" was the same mistake with another
  program current - loc 49 on `texmap` is `u_opacity`, a float. A4 was wrong and
  is replaced by A5; A2's type reading was right all along.
- No cost number from this run.

### 2026-11-24 — BREW00102: bind before setting, no swap in the window, one log file only

- Fixes from the run above: `glUseProgram` before every uniform set, including the
  link-time probe; the timed window now opens on the draws and closes at
  `glFinish` with **no swap inside it**, and the frame is presented only every
  30th frame, outside the window; a fifth configuration at N = 64 gives the line
  range if one chain turns out to be cheap.
- **One log, one place** (owner's instruction, rule R22): `GT_SINKS` is 1 and the
  only path is `/data/Alpha PS4 test kit/gpu-bloom102.log`. The six other sinks -
  `/data`, `/data/pkg`, the player's folder and three USB mounts - are gone, so a
  run leaves exactly one file and nothing to tidy up. The folder is created at
  startup and the open is retried once after a second `sceKernelMkdir`.
- Verified in the built `eboot.bin`: `strings` shows
  `/data/Alpha PS4 test kit/gpu-bloom102.log` and no other log path.
- **The cleanup is automatic** (owner: "part of that intelligent design is
  cleaning up old files automatically", rule R22). At startup, before the new
  file is opened, the build runs `sceKernelUnlink` over an exact-name table -
  `/data/gpu-bloom97..102.log`, `/data/gpu-shader-cat.log`, the
  `gpu-shader-cat.log` copies in `/data/pkg`, the player's folder and the three
  USB mounts, and the older probe logs inside the test kit folder - and logs
  `CLEAN removed=N refused=M of 17 known leftovers`. Exact names, never a
  pattern, so nothing outside the table can be deleted; the player's `app.log`,
  `app.old.log`, `config.ini` and `shots/` are deliberately not in it, because
  those are user data and the player already keeps just the current and one
  previous log.
- `files/GPU-Bloom-Slope-Probe-BREW00102.pkg`, MD5
  `811956619ea18987f0078ead0192ae6b` (rebuilt with the sweep). Not run on the
  console.

### 2026-11-24 — "Corrupted Data", CE-33481-9: an interrupted build shipped a half-written param.sfo

- First install of BREW00102: package installer reported
  `An error has occurred. (CE-33481-9)`, the home screen then showed the tile as
  **Corrupted Data** with a broken icon, and starting it gave
  `Cannot start the application. (CE-34544-0)`. No log, because the app never
  ran: the console refused the package, not the program.
- Cause, found by reading the package instead of guessing: the container was
  fine (`7f434e54` CNT header, 6,619,136 bytes, same as the working 101), but
  `sce_sys/param.sfo` had **7 entries instead of 11** - `TITLE`, `TITLE_ID`,
  `VERSION` and `SYSTEM_VER` were missing, so the console had nothing to name
  the app with. It was written at 05:02, the moment an earlier `make` was killed
  by a tool timeout (exit 137) partway through the twelve `sfo_setentry` calls.
  Every later build then found `param.sfo` newer than the Makefile, so `make`
  never regenerated it, and both 102 packages carried the truncated file. The
  build system was right; the file was newer than its recipe - the same
  timestamp trap that a `make clean` exists to break.
- Guard, so it cannot happen twice (rule R23): new `tools/check-pkg.py` parses
  the SFO, requires all eleven entries and the required keys, checks that
  `CONTENT_ID` and `TITLE_ID` match the Makefile, that the content id is 36
  characters, that the package starts with the `CNT` magic and is not truncated.
  The probe Makefile's `all` target now depends on a `check` target that runs it,
  so a bad package fails the build instead of reaching the console. Run against
  the rejected package it reports 3 failures; against the rebuild, 10 passes.
- `files/GPU-Bloom-Slope-Probe-BREW00102.pkg` rebuilt after `make clean`, MD5
  `3121e23fa1256a5e2b8c56d98e66f8a9`, `param.sfo` 11 entries, title
  `GPU Bloom Slope 4`. Not run on the console.

### 2026-11-24 — BREW00102 on the console: it installs, it runs, the panel is readable

- After the package fix (R23) the build installed and ran: the log is
  8,975 bytes at `/data/Alpha PS4 test kit/gpu-bloom102.log`, `mkdir` answered
  `0x80020011` (already there), one sink, nothing anywhere else on the console.
- **The uniform fix works and is proven:** all 23 `SET` lines read `accepted`
  (texmap 5/5, bright 5/5, blur1 6/6, blur2 6/6). A5 was right; A2's type
  reading was right; A4 was wrong. The picture now shows a real bloom field over
  blocky sprites, and the results panel is legible as glyph quads.
- **The timing is still quantised, and now we know exactly how.** With no swap
  inside the timed window, `draw_ms` came back 16.656 at N = 0, 16.654 at N = 1,
  16.658 at N = 4, 16.659 at N = 16 and 33.340 at N = 64 - one vblank and then
  exactly two. The CPU window can only bound the work to "< one frame"; the
  reported slope 0.278 ms/pass (r2 0.9509) is a single point crossing the vblank
  boundary, so it is a **bound of 0.26-0.52 ms per chain**, not a measurement.
  Read as a bound it is still the useful answer: even at the pessimistic end a
  6-pass chain costs ~3.1 ms of a 16.7 ms frame. Amendment A6, rule R24.
- The housekeeping sweep ran but said nothing, because the log line had been
  inserted into a branch that is skipped when a sink opens: **fixed** in
  BREW00103, which reports `CLEAN removed=N refused=M of 18 known leftovers`.

### 2026-11-24 — BREW00103: the GPU clock, and a slope that is a measurement

- New instrument per A6/R24: each measured frame is wrapped in
  `GL_TIME_ELAPSED_EXT` through `EXT_disjoint_timer_query` (entry points from
  `eglGetProcAddress`), the result is read after `glFinish`, and `gpu_ms` is
  fitted against N = 1, 4, 16, 64. The CPU window is still logged beside it as a
  cross-check, and if the entry points are missing the log says so instead of
  quietly reporting a quantised number as a cost.
- `CLEAN` is reported; the sweep table now includes `gpu-bloom102.log` in both
  `/data` and the test kit folder, so a 103 run removes the 102 log itself.
- `files/GPU-Bloom-Slope-Probe-BREW00103.pkg`, MD5
  `3bde518b33bf17627d5ab559b84f0084`, `param.sfo` 11 entries, title
  `GPU Bloom Slope 5`, package check 10/10. Not run on the console.

### 2026-11-24 — "Is this already solved elsewhere?" Some of it was, and that changed the method

- The owner asked whether this work is re-deriving answers other people already
  published. It was, in two places, and both are now recorded as rules rather
  than as embarrassment.
- **`alechurri/shipofharkinian-ps4`, `docs/TECHNICAL.md`** is a commercial-scale
  Piglet port with a "things that only showed up on hardware" section. It
  settles the `eglSwapInterval` trap (interval 1 after `eglMakeCurrent` blocks
  `eglSwapBuffers` forever on frame one; interval 0 plus your own timer is what
  they run), the real memory budget for a `gde` app (~768 MiB direct, ~255 MiB
  flexible, not the 4.6 GB the player's log quotes), shader compilation at
  110-260 ms per program with no disk cache, no `glBlitFramebuffer`, 16-bit
  offscreen depth, the OpenOrbis `struct stat` layout bug and `EINVAL` on
  relative paths. All of that is now in `PIGLET-NOTES.md`.
- **The ES2 spec** answers the question that cost us BREW00097 through
  BREW00101: `glUniform*` sets the uniform of the **currently bound** program.
  A4 was a detour through the driver's logs for something the spec states in a
  sentence.
- What the search did *not* answer, and therefore what a console run on it is
  worth spending: the cost of one 1080p bloom chain here, why a frame with the
  present removed still costs 16.66 ms, and whether a Piglet surface can share a
  screen with SDL's chrome. Note the sample already calls
  `eglSwapInterval(s_display, 0)` before `eglChooseConfig`
  (`PigletApplication.cpp:182`), so vsync was never the explanation for that
  16.66 ms.
- Rules R25 (spec, then other homebrew, then a console run) and the new §8 list
  of who solved what; findings in `PIGLET-NOTES.md` (`researched only` until
  measured here). No code was copied; every item is a written finding.

### 2026-11-24 — Rule R26: reflect on the manifest before every step

- The owner's instruction: every step, whatever the build, must first be checked
  against the manifest, so no work is done that the project already answers -
  and they should never have to be the one who says "somebody already did that".
- **R26** is now in `MANIFEST.md` §9 next to R25, and restated as a working rule
  in `AGENTS.md` (in one line, because G9's 640-line ceiling has no room to
  spare). It names its own evidence: BREW00102's timing question was already
  published by `shipofharkinian-ps4` and the uniform question was answered by the
  ES2 spec, yet both went to the console as tests, and the owner had to point it
  out. The rule's test is mechanical - before a build, run, research task or
  report line, reread §2/§6/§8/§9, `PIGLET-NOTES.md` and the amendments, then
  say which **recorded fact** the step tests and what is **genuinely new**; if it
  is already recorded, cite it and spend nothing; if the step is already in the
  plan, drop it. It inherits to every future project through G10.
- To make that reflection visible rather than a promise, the where-we-are block
  (§2) now carries a **Reflection (R26)** row: what is already recorded for the
  next run, and what that run actually adds.
- Housekeeping to fit the hard 128 KB manifest ceiling (G9): the long
  "Round-state notes carried over from AGENTS.md" block moved, unchanged, to
  `ROUND-NOTES.md` with a pointer in its place. Nothing was deleted; the manifest
  is now 125 KB against the 128 KB limit and the rules stay readable.

### 2026-11-24 — Re-amending the manifest after reading it whole: the state block was describing last week

- The owner asked me to read the whole manifest and re-amend it, because being
  asked to test known hardware means something recorded is being missed. Reading
  it found three stale entries and one recorded rule I had contradicted.
- **Stale, now fixed.** The where-we-are block still listed "Built and not yet
  run: v118; BREW00102", still called BREW00098 the blocking question, still said
  the unblocker was BREW00099, and still dated the last console verification to
  the BREW00098 log - while BREW00101 and BREW00102 had both run and 103 was
  built. The console-run row now carries 101 and 102 with their numbers.
- **Three §6 rows described a world that ended with A5.** "Bloom 3 drove the blur
  with a vec4" and "the driver's own location is not accepted by its own setter"
  were still marked open; both are the same missing `glUseProgram`, and BREW00102
  proved it with 23/23 `SET accepted`. "Round 2's bloom chain cannot be written as
  GLSL" was still the gate for the whole proof of concept; the blobs exist, link
  and draw - the gate passed. Two cosmetic rows were closed as historical.
- **The contradiction is now A7.** The measurement rules in `AGENTS.md` have said
  since v112 that every frame waits on the display and lands on whole 16.7 ms
  multiples. BREW00102's 16.656 / 16.659 / 33.340 are one, one and two refreshes -
  that rule obeying itself. Treating "why 16.66 ms" as an open question worth a
  console run was asking for a fact already written down. A6's reading is marked
  [superseded A7], and A7 keeps only the part that is genuinely new: the chain's
  own cost, which the display cannot show.
- **Two rules.** R27: the hardware is known - no run to confirm the display
  contract, module load, memory class, packaging route, log path or uniform
  binding; cite the recorded line instead, and quote the one unknown a run closes.
  To make that usable, §7 gained a **"Settled hardware - do not spend a console
  run"** table: eleven facts, each with its source, from the 60 Hz/vblank contract
  to the blob set and the param.sfo rule.
- **What that changed about the plan, which is the point of doing it.** The bloom
  proof of concept no longer has a blocking question: one chain is under half a
  millisecond by the CPU bound, so a 6-pass chain is at most ~3 ms of a 16.7 ms
  frame, which is affordable whatever the decimal is. BREW00103 stays built and
  unrun **on purpose** - it would turn a bound into a prettier number and decide
  nothing. The next console run is v118, because its logging answers the video
  16 fps question and the `??????` filename question, and no source can.

### 2026-11-24 — Platform scope: PS4 software for every PS4, and Pro numbers are ceilings

- Owner-stated, recorded as rule R28 and as a block at the top of §7: the target
  is the **PS4 family — base, Slim and Pro — as one platform**, not this one
  machine. Every model runs the same Orbis OS and the same interfaces this
  project uses (Piglet/GLES2, `sceAudioOut`, `libSceAvPlayer`, `scePad`, the same
  package format and install path), so PS4 is the common denominator: **working
  PS4 software runs on all PS4s**, and a model that differs in clocks changes
  only how fast the same code gets there, never what it does.
- What that forbids, written down so it can be checked: no model-specific builds
  or branches (one package serves every model); no Pro-only assumption in a look
  or a feature; no budget that only fits a Pro. Every measurement now names the
  model it ran on, and **a Pro figure is an upper bound** - the base PS4 is the
  floor a budget must fit.
- Status is honest: `owner-stated 2026-11-24`, **not yet measured on a second
  model** - the only console this project has run on is the owner's Pro on
  12.52. Firmware, GoldHEN and FTP are recorded as that machine's setup, not as
  features the software requires.
- The manifest is now at 130,866 bytes of its 128 KB-plus-slack ceiling (206
  bytes spare). The next substantial addition has to move a section to an
  appendix first - same as the round notes did - rather than squeeze the rules.

### 2026-11-24 — Rule R29: reuse the method that exists instead of rebuilding it

- Owner-stated, recorded as rule R29: **do not reproduce a method that already
  exists.** Look in this order and use what is found - this repository's own
  proven code, the OpenOrbis samples, a licence-clean library, the published
  technique - and write new code only where nothing fits or the licence forbids
  it. When new code is written it comes from the documented method and is never
  pasted (G7 clean-room note). If two implementations of one job exist, the
  second must name why it exists or it goes.
- The rule names its own evidence rather than inventing it: every GPU probe from
  BREW00097 on carries a hand-written logger and a hand-rolled glyph-quad text
  renderer, while the player has shipped `src/applog.c` and its own UI text
  drawing since v111. The duplication was defensible for a thirty-line
  throwaway and is not defensible now, so §6 carries it as an open item with a
  decision: the next probe either links the player's logger and text renderer or
  writes one line explaining why it keeps its own.
- Housekeeping for the size ceiling (G9): the two logging sections - the three
  stranger logs read and ruled out, and the survey of where `/data` accepts
  writes - moved to `APPENDIX-LOGS.md` with a pointer. Nothing deleted; the
  manifest is back to 126 KB against 128 KB and the rules were not squeezed.

### 2026-11-24 — Rule R30: only looks are put to the owner; technical questions are answered from the manifest

- Owner-stated, recorded as rule R30 and as a rewrite of the AGENTS reply rule:
  **the only thing asked should be aesthetic.** If a question is about
  programming or coding in any way, the answer comes from the manifest and the
  work continues.
- What counts as a question: which look, which palette, sharper or smoother,
  this layout or that - a visual choice only the owner can make. What does not:
  an API, a driver refusal, a format, a build error, a timing method, a
  structure, a default value. Those are resolved in this order - this manifest,
  `AGENTS.md`, `PIGLET-NOTES.md`, `APPENDIX-LOGS.md`, `ROUND-NOTES.md`, the
  ES2/EGL spec, the OpenOrbis samples, other PS4 homebrew - and the answer is
  written back into the manifest in the same turn, so the next session finds it
  rather than asking again. If nothing holds the answer, it becomes a decision
  recorded with its reason, not a question.
- Asking the owner to pick between technical options is now a defect in the
  round, in the same class as spending a run on a known fact (R27) or rebuilding
  a method that exists (R29).
- A console run is explicitly **not** a question: it is a step needing their
  hands, and the reply hands it over complete - file, MD5, install route, what to
  do, which lines to send back. That is unchanged.
- `AGENTS.md` carried the older, softer version of this rule ("questions are
  allowed when the owner asks to be consulted"); it was rewritten in place to
  match R30 without adding a line, so the 640-line ceiling still holds.

### 2026-11-24 — Reading the manifest again: the roadmap still called a finished thing "next"

- The owner asked for another full read and an amendment if it needed one. It did
  - in the roadmap, in the round plan, and in the predictions register.
- **§3 roadmap, two rows out of date.** "Prove one GPU path: one effect (bloom)"
  still pointed at the sample's 120 fps textured quad as the evidence, when the
  bloom chain itself has since drawn on the console (BREW00102: `bright`/`blur1`/
  `blur2` linked, 23/23 uniforms accepted, a real pass at 480x270 between 1080p
  targets). "Proof of concept: one look on the GPU ..." was still "next, after the
  video-logging round", when the *effect* half is done and cost-bounded; the
  **look** port is the part still waiting on the owner. "Research PS4 GPU
  capability" said "not started" while `PIGLET-NOTES.md` and the graphics
  principles carry the research and its disagreements.
- **A malformed table row, fixed.** The MilkDrop warp-feedback row and the v118
  row had been glued together by a doubled `||`, so two items rendered as one.
  They are two rows again, and the warp-feedback row now reads "unblocked; the
  bloom chain is the first effect through it".
- **§11 Round 2 is no longer a future plan.** It opens with a status line: effect
  proven 2026-11-24, one chain under ~0.5 ms by the CPU bound and a six-pass chain
  at most ~3 ms, look port and SDL/Piglet sharing still unproven. Its logging
  instruction ("one sink at `/data` and one in the player's folder") contradicted
  R22 and is corrected to **one** sink in `/data/Alpha PS4 test kit/`.
- **A prediction came true late, and lost.** P3 said the four-pass bloom chain
  would cost 1-3 ms; BREW00102 bounds it under ~0.5 ms, so the estimate was two to
  six times too pessimistic. Recorded as a miss on the cheap side, which is the
  useful kind: it says the cost model was over-cautious, not that the measurement
  is wrong. The register keeps the frame it was written in.

### 2026-11-24 — The predictions register moves out, and the audio thread's unlocked flags become R31

- **The predictions register left `MANIFEST.md` §11 for `APPENDIX-LOGS.md`.** It
  is 2,359 bytes of evidence — six rows, P0 to P5, each with the number that would
  falsify it — not a rule, and the manifest is against its ceiling. Every row moved
  unchanged, the rule that governs the register moved with it ("no number from this
  run enters §9 until its row is filled in"), and §11 keeps a pointer so nobody
  looks for it where it used to be. The manifest went from 129,606 to 129,004
  bytes with the new rule included.
- **R31 records a defect that was in the source all along.** `s_playing`,
  `s_quit` and `s_track_finished` are `volatile int` in `src/main.c:157-159`; the
  SDL loop sets them and `audio_thread` sets them too when a track ends
  (`src/main.c:1883-1885`), and `src/main.c` contains no `pthread_mutex` at all.
  `volatile` does not make that atomic and does not order the flag against the
  buffer it guards, so a stale or reordered read can start a track the loop has
  stopped or leave the thread running one the loop thinks is over. The rule states
  the fix — one owner per flag, a mutex or a generation handoff — and that the host
  tests cannot see this race, so the proof has to be a console run that stalls and
  repeats a track.
- **`check-links.py` no longer judges `_scratch/`.** It was reporting two broken
  links, both inside a throwaway copy of an older published tree where the target
  files were never copied; those pages are not published and `rule-index.py`
  already skips the same folder. With the folder excluded the check reads 7 pages,
  0 problems. Nothing in the real documentation was broken before or after.
- **Checks after the pass:** `check-manifest.py` 28 passed, 0 failed;
  `rule-index.py` 41 rules, 0 problems; `check-links.py` 7 pages, 0 problems;
  `status.py` 4 steps, 4 passing, 41 rules.

### 2026-11-24 — Reading the manifest whole before an upload: a broken rules table and a board three rounds stale

- **The graphics rules table was broken, and had been published that way.** Two
  paragraphs — "Ruled out by the driver, not by preference…" and "One thing the
  principles forbid saying…" — had been spliced *between* the R19-R29 rows, with
  three blank lines splitting the table into four fragments. Markdown cannot
  render that: the rules and the paragraphs both came out as rubble, in the
  manifest and in the published `docs/manifest/09-…` page. The 14 rows R18-R31
  are now contiguous, and the two paragraphs sit below the table where they
  belong. Nothing was reworded; the text of both paragraphs is the same text,
  only rejoined.
- **§2 still said the source was not published.** It named tag
  `docs-2026-11-24-0114` and "(source not yet published - see below)", which
  stopped being true at `c55591b`. The row now states the real thing: source,
  docs, tools and notes on `main`, tag `docs-2026-11-24-0209`, verified by
  comparing 63 of 66 files against the master copy, with the three differences
  named (the publisher's pointer block, the regenerated `AGENTS.md` header, the
  generated `font_inter.h`), and that this tree is ahead of it by the R31 pass.
- **The status board was advertising a run three rounds old.** "Waiting on the
  console" was hardcoded to the BREW00098 bloom differential and
  `/data/gpu-bloom98.log`, and "Predictions for that run" had gone empty the
  moment the register moved to the appendix. Both now come from live sources:
  the console card reads §2's own next action, and the predictions are parsed out
  of `APPENDIX-LOGS.md`. A board that names a finished run as the thing being
  waited for is worse than no board — it is the first thing anyone reads.


## Build log - moved from `MANIFEST.md` §4 (2026-11-24)

Kept verbatim. The spine now carries only the present (§2) and the decisions
(§5); this is the dated record, oldest first, as it was written.

## 4. Build log

Moved to `files/HISTORY.md` on 2026-11-24: the log had grown to 827 lines and was
crowding out the rules and the current state. The full entries — quoted log lines,
MD5s, source hashes, decisions and corrections — live there, verbatim and in date
order. This section is only the ledger, so a reader sees what happened without
loading the history.

| Date | Entry |
|---|---|
| 2026-11-23 | v113 (Liquid: water body, depth, framing) |
| 2026-11-23 | v114 (Liquid: glass, floor, spray, glint) |
| 2026-11-23 | v115 (the 467 ms freeze on Square) |
| 2026-11-23 | manifest: environment closed, GoldHEN and FTP researched |
| 2026-11-23 | v116 (glint, spray, floor — the three look tweaks) |
| 2026-11-23 | on hardware: the freeze, and Liquid's step |
| 2026-11-23 | manifest merge: the graphics standard becomes part of the goal |
| 2026-11-23 | manifest: GPU paths, console facts, techniques, and other players |
| 2026-11-23 | GPU path test: the piglet sample, built and packaged |
| 2026-11-23 | v117 (header overlap) and the video/audit analysis |
| 2026-11-23 | GPU test v2: a log written with kernel calls |
| 2026-11-23 | GPU test v3: the stage log (the one to run) |
| 2026-11-23 | the Piglet v3 log read: the GPU path is confirmed |
| 2026-11-23 | the v117 log read: nothing regressed, the header needs the screen |
| 2026-11-23 | manifest sync: graphics principles and the three rounds |
| 2026-11-23 | v118 (logging only) built |
| 2026-11-23 | Round 2 started, and stopped at the shader gate |
| 2026-11-23 | the shader catalogue package (BREW00094) built |
| 2026-11-23 | GNM checked, and the 60 fps skeleton written |
| 2026-11-23 | manifest: owner statements are folded in automatically |
| 2026-11-24 | the catalogue log read: a bloom chain CAN be assembled |
| 2026-11-24 | GPU Bloom POC built (BREW00095, no player version) |
| 2026-11-24 | the bloom log: the chain compiles, then dies at the first frame |
| 2026-11-24 | BLOOM 2 built (BREW00096): per-call error tagging |
| 2026-11-24 | the BLOOM 2 log: the chain runs, 85–100 fps, and the timings are not GPU timings |
| 2026-11-24 | BLOOM 3 built (BREW00097): sample draw out, GPU timings, real workload |
| 2026-11-24 | the BLOOM 3 log: my measurement is the bottleneck, and a uniform bug broke the blur |
| 2026-11-24 | other Piglet projects consulted, then BLOOM 4 built (BREW00098) |
| 2026-11-24 | R32 adopted: owner authorised scoped build rounds; dated backups under _scratch/backups/ are the revert path (hashes recorded in RELAY.md) |

## Carried from other PS4 homebrew - evicted from `MANIFEST.md` §9 (20261124-1113)

```
### Carried from other PS4 homebrew (`researched only`; text in `PIGLET-NOTES.md`)

Interval 0 is right for vsync; a `gde` app gets ~768 MiB direct and ~255 MiB
flexible, not 4.6 GB; shader compiles 110-260 ms and uncacheable; no
`glBlitFramebuffer`, 16-bit offscreen depth. Vsync was never the probe's problem
- `eglSwapInterval(display, 0)` is already in the sample
(`PigletApplication.cpp:182`) - so BREW00102's 16.66 ms with no present is still
open and BREW00103 measures it with the GPU clock.
```


## Who has already solved what - evicted from `MANIFEST.md` §9 (20261124-1113)

```
### Who has already solved what (2026-11-24)

Look for the existing answer first (R25, R26); findings, not code, in
**`PIGLET-NOTES.md`**. `alechurri/shipofharkinian-ps4` `docs/TECHNICAL.md` is the
best public Piglet write-up (`eglSwapInterval` trap, ~768/255 MiB memory,
110-260 ms shader compiles, no `glBlitFramebuffer`, `struct stat` bug, `EINVAL`
paths); `Navinas3460/Stremio-PS4` is a video app on this stack; `shadPS4` has the
display contract; the ES2 spec has the uniform rule A5 cost two runs to learn.
```

## The conversation is part of the project - read-order addition (20261124-1301)

```
Owner, 2026-11-24: "just make sure that it's part of the project ... it's the
soul of the project essentially." `CONVERSATIONS.md` at the kit root (5311
bytes) holds the owner-and-agents record; this round puts it last in the read
order, in `MANIFEST.md` and in `AGENTS.md` together, so a session meets it
without being told: `HISTORY.md` (why it happened) -> `../CONVERSATIONS.md`
(why we work this way; not a rule). MANIFEST.md 130916 -> 130977 bytes,
AGENTS.md 40198 -> 40259. check-manifest 30 passed, 1 failed - the same stale
`STATUS.html` as before the edit; check-links 7 pages, 0 problems, unchanged.
The file is not in the public repo, so the published tree and CI never see it.
```
## The mutation harness is back, in test/ (20261124-1323)

The owner asked for the fault harness to be rebuilt. It had been cited since early
on and was simply gone: `ps4-media-player/_scratch/linttest/mutate.py` existed in no
copy of the kit, while `status.py` ran it on every build. That is why the board read
"a host check is failing" with nothing on the page to explain it.

It now lives at `ps4-media-player/test/mutate.py`, beside the player tests, because
`_scratch/` is exactly the directory a cleanup deletes. Twelve cases, matching the
manifest's "12 of 12":

   1 a section number has a gap        7 a rule bullet is duplicated
   2 one R id appears twice            8 RULES.index is not valid JSON
   3 a § reference points nowhere      9 the index drops a prose id (R33)
   4 a TODO placeholder is left       10 an indexed rule has no falsifier (R34)
   5 a superseded tag names no        11 a number is quoted with no status
     amendment                           word (rule-index)
   6 AGENTS grows past 40 KB          12 the build stamp reads the clock

Each case copies the document tree to a temporary directory, injects one fault, and
requires the real checker to exit non-zero *and* print the failure it is supposed to
print. A crash for an unrelated reason is not a catch. The copy is removed in a
finally block, so the working tree is never written, and a fault's timestamp is put
back after the write so a case cannot pass merely because the edit made the status
board look stale.

Case 12 is a property rather than a corrupted document: two runs a second apart must
agree, and SOURCE_DATE_EPOCH=0 must print 1970. To show the property has teeth, the
same test runs against a copy of buildstamp.py whose DEFAULT reads the clock, and
that copy must fail it.

The first run caught 10 of 12 and said which two were wrong: a plain "97" where the
fault needed a section reference, and a synthetic R row whose cell spacing did not
match rule-index.py's parser. Both were harness bugs, not checker bugs; fixing them
is what took the count to 12. This is the reason the harness matches the printed
failure, not only the exit status.

Wired in:
  test/mutate.py          new, 12 cases
  tools/status.py:79-80   the step path, and the count it advertises (8 -> 12)
  Makefile:127            `make check` now runs the harness
  MANIFEST.md:556         citation repointed to test/mutate.py; the
                          declared-absent entry for the old path is gone

Evidence, on the laptop, this round:
  python3 test/mutate.py          12 of 12 caught, exit 0
  make status                    wrote STATUS.html (4 steps, 4 passing, 48
                                 rules), pill "all host checks passing" - the
                                 first green board since the harness vanished
  make check                     audio 30 PASS; manifest 31 passed, 0 failed;
                                 rule index 48 rules, 0 problems
  MANIFEST.md                    130,932 bytes, 140 under its 128 KB budget

Backups, sha256:
  _scratch/backups/MANIFEST.md.20261124T132110Z
    b6ce01581b6d953711b2774d7f0a3ad6df6ec91b4857b5d0cda3c052831ec467
  _scratch/backups/status.py.20261124T132110Z
    eda0cc673d30831dffccb4812ee72481401057fda37ad48c2a6711389bcee40d
  _scratch/backups/Makefile.20261124T132110Z
    5ed0db336bf4be095fe913ac70d26b91c7fc543d0a515e5ae77c8fbf20f08e8b
  _scratch/backups/HISTORY.md.20261124T132306Z
    c7e9f1a704168e4b48ef3476c1124d14baa72feac61d649df0d582870dd03c80
## Checkpoint: CI diagnosed, resume file written (20261124-1331)

The CI red was checked rather than assumed. The audit branch carries the published
layout - no MANIFEST.md, docs/ instead - and its workflow runs only check-links
plus a docs/RULES.index status validation, never check-manifest. A shallow clone
of audit/relay-2026-11-24 at 6017bf5 gives check-links 80 pages, 0 problems, and
41 rules with valid statuses, so both CI steps pass at that commit. The recorded
"85 pages, 69 problems" is therefore historical: commit 6017bf5, which redacted
the relay for publication, is what changed the link set. gh is not authenticated
here, so the live run list still needs the Actions page.

The published tree is behind the working one: 41 rules there against 48 here, so
publishing has not run since those seven rules were added.

For a cold start, RESUME.md now sits in the kit root: state, open items in order,
the commands, and the mistakes not to repeat. check-links roots at the repository,
not the kit root, so that file is outside its scan either way.
