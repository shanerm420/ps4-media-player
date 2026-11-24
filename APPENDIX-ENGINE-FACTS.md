# Engine facts - appendix to AGENTS.md

**This is not a rules file.** The rules are in [AGENTS.md](AGENTS.md); this
appendix holds the measured facts those rules rest on, so the rules file stays
short enough to be read before work starts (G9). Nothing here overrides AGENTS.md,
and no figure here is a target: it is a reading, with its evidence beside it.

**Read order:** [AGENTS.md](AGENTS.md) (the rules) -> the manifest section named
beside the figure you need -> this appendix -> [docs/HISTORY.md](docs/HISTORY.md).

Moved out of `AGENTS.md` on 2026-11-24, text unchanged: the OpenOrbis/SDL facts,
the log and ladder facts, the console log facts, and the GPU path findings. A rule
that used one of these facts still cites its manifest section; the fact lives here.

## OpenOrbis / SDL facts already established

- `sceKernelGetProcessTime()` returns `uint64_t` microseconds from launch; it
  works before SDL_Init. The console's libc has **no working
  `CLOCK_MONOTONIC`** — `clock_gettime` fails, which is why every log line
  before v111 read `0.000`. v111 feeds the log the kernel clock.
- The SDL-PS4 software renderer **queues** draw commands; `SDL_RenderFlush` runs them (`SDL_FillRects`, `SDL_BlendFillRects`, `SDL_BlitSurface`, `SDL_BlitScaled`). Source: OpenOrbis/SDL-PS4 `src/render/SDL_render.c`, `src/render/software/SDL_render_sw.c`.
- **Measured on hardware (v112), and this corrects the line above:** the
  look's own rasterization lands in `paint` (16-40 ms at step 3), and `flush`
  has a floor of **21.6-24.5 ms on every screen**, including a music list that ([§9](docs/manifest/09-ps4-knowledge-base.md))
  draws almost nothing (`paint=0.9`, `flush=24.3`). That floor is the
  full-frame canvas copy into the 24-bit window surface, so the queue is
  already drained inside `paint` (the texture upload forces an early flush)
  and `flush` is mostly the copy plus chrome. A video frame under the canvas
  raises it to ~29-37 ms; video drawn on its own (no canvas) costs 1.4 ms. ([§9](docs/manifest/09-ps4-knowledge-base.md))
- The window surface the renderer draws into is created as 24-bit
  **`SDL_PIXELFORMAT_BGR888`**; presenting (`SDL_UpdateWindowSurface` →
  `PS4_UpdateWindowFramebuffer`) memsets the whole 8.3 MB video-out buffer, ([§9](docs/manifest/09-ps4-knowledge-base.md))
  copies rows at 4 bytes per pixel, then submits a flip and **waits for it**
  (`sceVideoOutSubmitFlip` + `sceVideoOutWaitVblank`/equeue). Source:
  OpenOrbis/SDL-PS4 `src/video/ps4/SDL_ps4video.c`.
- The net control device code **0 means wired** on this user's console
  (observed, not documented — no public source pins the enum). Other values
  stay unlabeled numbers.
- The pad opens as a **plain joystick** (6 axes, 20 buttons). Button numbers
  follow the pad's own report order: 0 Cross, 1 Circle, 2 Square, 3 Triangle,
  4 L1, 5 R1, 6 L2, 7 R2, 8 SHARE (never mapped), 9 OPTIONS, 10/11 L3 (mute),
  12 R3 (EQ), 13 touchpad click (screenshot, v111+). Triggers are polled as
  axes 4 and 5 in `pad_triggers()`. The pad still needs a proper
  game-controller mapping — open item.

## Log and ladder facts

- Log line: `[uptime] LEVEL tag message`, schema 2. Per-second summary carries
  `mode style step canvas file n avg min p95 p99 max over` plus notes
  (`paint`, `flush`, `pres`, `surface`, `dec_mb`, `bpm`, `level`, `bass`…).
  `paint` is the UI draw, `flush` is `SDL_RenderFlush` (the real drawing),
  `pres` the flip wait.
- Ladder (`src/vis_art.c`): `[quality] max_step=3`, `budget_step1_ms=56`,
  `budget_ms=88`, `settle_seconds=5`. **v112 fixed its timing bug:** main hands
  it the whole frame (`vis_art_frame_ms()`), flush and present included.
  Confirmed on hardware: `quality step 1 -> 2 (reason: over budget, 84.0 ms >
  56 ms at 1640x560, style 8 LIQUID)` and `2 -> 3 (89.2 ms > 88 ms, style 9
  NEON)`, after which the session stayed at step 3. It climbs back only on a
  remembered cheaper-step cost for the *same* look (`s_q_cost` is cleared on
  every style change), so in practice a look that reaches step 3 keeps it.
- Beat/tempo (`src/spectrum.c`): median of beat intervals, then folded into
  60–160 BPM. The 160 ceiling replaced 200 after an 88 BPM track read 175.8.
  If double-time still appears, fold each interval before taking the median.

## Console log facts worth remembering

- Device 0 wired; position/format lines belong to the boot block.
- Empty audio pulls log `rc=0` and are not decode errors.
- `long frame` warnings are rate-limited; the same line inside 3 s is counted.
- A synchronous bake shows as a high `paint` with an unchanged `flush`.
- v109 evidence: the 467 ms Square-on-Home freeze was `bg_build()` on a theme
  change, not subtitles; the video had been stopped 7 s earlier.
- v115 hosts the fix for that: the bake is sliced (`BG_JOB_MS`, 12 ms a frame)
  and the outgoing skin keeps drawing until the new one is ready. On the host
  the worst frame of a skin change went 110.9 -> 19.1 ms with identical output. ([§9](docs/manifest/09-ps4-knowledge-base.md))
- The console runs **GoldHEN v2.4b18.14** (PS4 Pro, 12.52): FTP 2121, payload
  server 9090, klog 3232, packages installed from `/data/pkg`. The GitHub
  release page still lists 2.4b18 and firmware only to 11.00 and is therefore
  wrong for this machine; the details and the source disagreements are in
  `MANIFEST.md` [§9](docs/manifest/09-ps4-knowledge-base.md).
- **Package builds are not byte-reproducible.** `eboot.bin` embeds `__DATE__`
  and `__TIME__`, so identical source builds two different pkgs and `cmp`
  between them always differs. An MD5 identifies a build, not a source state;
  prove the source with a hash of `src/` and with the version in the log's
  build line.

## GPU path — confirmed on this console 2026-11-23

The toolchain's Piglet sample was built as its own package (BREW00093), run on
the console, and it works. It is the first GPU path tried and the only one
tested; OpenGNM, PS4GL_GNM and ps4-kit stay untested and are not needed.

- **Modules: system copies only, no devkit files, no patch step.**
  `Piglet module_start() = 0 (0x00000000), handle 21` and
  `PrecompiledShaders module_start() = 0 (0x00000000), handle 22`, loaded from
  `/<sandbox>/common/lib/`. `libSceShaccVSH.sprx` was never requested. This is
  true for this sample on 12.52 with GoldHEN v2.4b18.14, nothing wider.
- **Driver strings:** `EGL_VERSION: 1.4`, `EGL_VENDOR: Sony Interactive
  Entertainment Inc.`, `GL_VERSION: OpenGL ES 2.0 Piglet`, `GL_VENDOR: Sony
  Interactive Entertainment Inc.`, `GL_RENDERER: Piglet`,
  `GL_SHADING_LANGUAGE_VERSION: OpenGL ES GLSL ES 2.0`.
- **Limits:** max texture 4096, max renderbuffer 4096, viewport 4096x4096,
  config count 30 (the sample's own `eglChooseConfig` returns 1).
- **Present:** 3,300 frames in 27.5 s = 119.9 fps, about 8.3 ms, on a textured ([§9](docs/manifest/09-ps4-knowledge-base.md))
  quad. The sample does not wait for vsync; do not read that as a budget for a
  real look.
- **Capabilities that matter here:** float textures and float/half-float linear
  filtering PRESENT; `EXT_color_buffer_half_float` PRESENT, so half-float
  render targets work; `EXT_color_buffer_float` absent, so no 32-bit float
  targets; `EXT_float_blend` absent; framebuffer fetch absent under both names,
  so a warp-feedback effect is ping-pong FBOs; `EXT_sRGB` absent;
  `OES_standard_derivatives`, `EXT_shader_texture_lod` and
  `EXT_disjoint_timer_query` present.
- **There is no `libGLESv2.sprx` on this console.** The player's boot line reads
  `libGLESv2 rc=0x80020002`, the same code the sample gets for an unmounted USB
  stick, and SDL then reports no dynamic GL support. GL work here links Piglet
  by name and swaps its own surface; SDL keeps the window, chrome and flip until
  a proof of concept says otherwise.
- **Logging rule learned the hard way:** write the log with
  `sceKernelOpen`/`sceKernelWrite`/`sceKernelFsync` and fsync every line. A
  `freopen` on stdout can fail silently and produce an empty file even though the
  path is writable — `/data`, `/data/pkg` and `/data/ultimate-ps4-media-player`
  all are, proven by that log. One sink is enough: extra sinks get every logger
  line but not the process's own `printf` output.
- **Unmeasured and blocking a port:** what a real visualizer costs on this GPU at
  1080p, and whether a Piglet surface can share the screen with SDL's chrome.
  Both are questions for the proof of concept, not for the audit.
