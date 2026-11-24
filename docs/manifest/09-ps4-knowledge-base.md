# 9. PS4 knowledge base

*Part 9 of 12 of the manifest — previous: [Resources](08-resources.md) · next: [Open questions](10-open-questions.md)*

The facts live in `AGENTS.md` §"OpenOrbis / SDL facts already established", §"Log
and ladder facts" and §"Console log facts worth remembering", because that file
is what is read before every change. Short version, so this manifest stands
alone:

- The SDL-PS4 software renderer **queues** draw commands; `SDL_RenderFlush` runs
  them. A texture upload forces an early flush, so the look's own work lands in
  `paint`, not `flush`.
- `flush` has a hard floor of **21.6–24.5 ms** on every screen, including one
  that draws nothing: it is the full-frame copy into a 24-bit window surface.
  No screen can exceed 30 fps until that path changes.
- Presenting memsets the whole 8.3 MB video-out buffer, copies rows at 4 bytes a
  pixel, submits a flip and waits for it.
- A **scaled** full-screen blit costs ~45 ms; an unscaled one is a row copy.
  Bake small, upscale once, blit 1:1.
- `sceKernelGetProcessTime()` works before `SDL_Init`; the console libc has no
  working `CLOCK_MONOTONIC`.
- The pad opens as a plain joystick; button 0 Cross, 1 Circle, 2 Square,
  3 Triangle, 13 touchpad click. SHARE is never mapped.
- A synchronous bake shows in the log as a high `paint` with an unchanged
  `flush` — that pair is how the 467 ms freeze was identified.
- **GoldHEN, on this console.** v2.4b18.14 is SiSTRo's beta, published on his
  Ko-fi shop, **not** on the GitHub releases page: that page still stops at
  v2.4b18 (28 Oct 2024) and lists firmware only to 11.00, so it is wrong for a
  12.52 console. The chain of support: **v2.4b18.7** added 12.50 and 12.52
  ("thanks to 15432 to implement theflow0's NetControl exploit (poopsploit)"),
  .10 added the PayLoader LaunchPad and AutoRun, .12 added 13.02/13.04/13.50,
  .14 added 14.02. Three places where sources disagree, all of them traps:
  1. *Whether 12.52 is supported.* The repo's release page says no; a GitHub
     issue from Aug 2025 says "first, we need a new kernel exploit"; SiSTRo's own
     .7 notes say yes from Nov 2025. The .7-and-later betas are right, and the
     older statements were true when written.
  2. *The name of the 9090 server.* Official 2.4b18 text says BinLoader, later
     mirrors say PayLoader. Same port, same job, different label.
  3. *Which port FTP uses.* GoldHEN's is **2121** in every source; other
     payload loaders use 1337 or 21, so a client set up from a different guide
     fails to connect for no visible reason.
  Services: FTP 2121, payload 9090, klog 3232. Internal package installation
  from `/data/pkg` has existed since GoldHEN 2.3, and Package Installer's Hdd
  source is `hdd:/data/pkg/`.

### Graphics principles — the rules every graphics decision follows (2026-11-23)

This is the owner's core instruction, and it governs every graphics change from
now on. It is checked **before** any graphics decision, and it is kept current
as rules are added, corrected or retired.

**The loop, in order:**

1. **Research** the technique with five different phrasings. Record sources,
   dates, and where they disagree.
2. **State a hypothesis:** what it should look like, and what it should cost on
   this console.
3. **Test it** — in a separate package if it touches the GPU, never by folding it
   into the player untested.
4. **Measure on the console:** frame time, GPU time, resolution, and what appears
   on screen. **Host numbers are a ranking, not a console number.**
5. **Write the result as a rule with its evidence**, in the form "half-float
   bloom at quarter size costs X ms at 1080p, measured in build N", and mark it
   **measured**, **researched only** or **assumed**.
6. **Rank by visual gain per unit of cost** and say why. If a result surprises
   us, record the surprise. If a rule turns out wrong, correct it, keep the
   history, and say what changed.

**Rules established so far:**

| # | Rule | Status | Evidence |
|---|---|---|---|
| R1 | The Piglet ES2 path works on this console: firmware modules load, EGL 1.4 comes up, `GL_RENDERER: Piglet`, frames present | **measured** | GPU test v3, MD5 `4d556822b249fcf1c3f55a044d806fa2`, log [§4](04-build-log.md)/[§9](09-ps4-knowledge-base.md) |
| R2 | Half-float render targets work; 32-bit float render targets do not; no float blending, no framebuffer fetch, no sRGB framebuffers — so feedback is ping-pong targets | **measured** | `piglet-gpu-test-v3.log`, extension scan |
| R3 | Maximum texture and renderbuffer size 4096, viewport 4096x4096 | **measured** | `piglet-gpu-test-v3.log` |
| R4 | The sample's 119.9 fps is a textured quad with no shader load, no bloom and no SDL window beside it — it is **not** a budget for a look | **measured, explicitly not transferable** | `piglet-gpu-test-v3.log`, frame timestamps |
| R5 | The SDL present path costs 21.6–24.5 ms a frame on every screen, including one that draws nothing | **measured** | v112/v116 logs, `AGENTS.md` |
| R6 | No look fits a 60 Hz frame at step 1, even on the host: 27.8–75.4 ms | **measured (host ranking)**; the console multiplies it by 2.2–4 | `files/round7/README.md` [§2](02-current-focus.md) |
| R7 | What a real 1080p look costs on this GPU | **assumed: nothing.** No number exists | pending BREW00098 (the measurement that will produce it) |
| R8 | Audio reaction is one global beat list today; per-band beats with a refractory interval and geometric decay, log-spaced bands, packet interpolation and automatic gain are the researched model | global behaviour **measured** in source; the per-band set is **researched only** | `files/round7/README.md` [§2](02-current-focus.md), MilkDrop/projectM model |
| R9 | There is no `libGLESv2.sprx` on this console, so GL work links Piglet by name instead of going through SDL's GL lookup | **measured** | v117 `app.log`, `boot: libGLESv2 rc=0x80020002` |
| R10 | **Piglet ES2 can only use shaders that already exist as blobs in `libScePrecompiledShaders.sprx`** — OpenOrbis 0.5.2 ships no GLSL compiler, and the runtime compiler is a devkit module this console does not have | **measured** | toolchain `bin/linux` listing; `PigletApplication.cpp:29,36,356` |
| R11 | **A bloom chain is assemblable from existing blobs**: separable blur pair, brightness/contrast, opacity and texmap composite, on a complete half-float target | **measured** | `gpu-shader-cat.log` 2026-11-24: `82 entries`, `blur1`/`blur2`, `status=0x8CD5 COMPLETE` |
| R12 | There is **no per-pixel custom shader** on this path — no fluid solve, raymarch, refraction or true bright-pass threshold | **measured** (the table has no such program and none can be compiled) | `gpu-shader-cat.log`, 82 entries; R10 |
| R13 | **The blur programs are an 11-tap Gaussian with a vec4 radius, and every program in the chain links** | **measured** (console, 2026-11-24) | `gpu-bloom.log`: `link=1 glerr=0x0000` ×5, `u_gaussianKernel[0] size=11`, `u_blurRadius type=0x8B50`, `u_filterAmount type=0x1406` |
| R14 | **CPU timers around GL draws measure submission, not execution** — a 1080p composite reported at 0.03 ms and a blur at 0.00 ms are queue times, not GPU work | **measured** (console, 2026-11-24) | `gpu-bloom96.log`: `BLOOM … blur1=0.00 comp=0.03 total=8.78ms` with `fps=113.9`; timing must use `EXT_disjoint_timer_query` or `glFinish` |
| R15 | **`glFinish()` per pass costs ~13–14 ms on Piglet**, independent of the pass size — a 480x270 pass measured 14.6 ms against 18.3 ms for 1080p | **measured** (console, 2026-11-24) | `gpu-bloom97.log`: `bright=14.61` at 480x270 vs `scene=18.30` at 1920x1080; four finishes a frame turned 10.3 ms into 66.7 ms |
| R16 | **`u_blurRadius` is a `vec2` (`0x8B50`), not a vec4** — set it with `glUniform2f`; a wrong type is rejected with `illegal to set uniform data with different types` and silently leaves the kernel at defaults | **measured** (console, 2026-11-24) | `[PIG]E-glUniform4f:8900` + `CHK pass-blur glerr=0x0502` in `gpu-bloom97.log`; corrects the earlier vec4 note in [§4](04-build-log.md) |
| R17 | **`GL_RGBA16F` does not exist in ES2, but a half-float render target does** — `internalformat GL_RGBA` + `type GL_HALF_FLOAT_OES` (0x8D61) completes; a port that asks for `GL_RGBA16F` gets a failure and wrongly concludes half-float is unavailable | **measured here + corroborated** | our `gpu-shader-cat.log` `TARGET … 0x8CD5 COMPLETE`; ioQuake3-PS4 reports "no GL_RGBA16F in GLES 2" and forces RGBA8 |
| R18 | **`eglSwapInterval(1)` after `eglMakeCurrent` blocks `eglSwapBuffers` forever**; set it before the surface is created, or use 0 | **researched only** (two independent ports, not tested here) | shipofharkinian-ps4 TECHNICAL.md; Stremio-PS4 `platform_ps4.cpp` |
| R19 | **Piglet memory comes from a flexible limit set at `scePigletSetConfigurationVSH`** — a working split is 128 MiB system shared / 512 MiB video shared / 236 MiB flexible / 4 MiB command buffers, textures and render targets out of the flexible share, a 1080p framebuffer pair over 64 MiB | **researched only** | shipofharkinian-ps4 TECHNICAL.md; the numbers to start G1's budget from |
| R20 | **Frame timing here sits on a ~8.3 ms present floor, so differencing two configs cannot see the work** — `A empty` measured 8.19 ms and `B scene` with a 1080p ramp and 2,000 sprites measured 8.32 ms, a 0.13 ms difference for work that cannot plausibly be free; anything drawn under the floor disappears into the flip | **measured** (console, 2026-11-24) | `gpu-bloom98.log`: `CONFIG empty … ms=8.19` and `CONFIG scene … ms=8.32`. The method is repetition slope: run N passes per frame at N = 1, 4, 16, fit the line, read the slope. Differencing stays valid only for work larger than the floor |

**Ruled out by the driver, not by preference — do not propose these without a new
reason:** 32-bit float render targets (`EXT_color_buffer_float` absent), float
blending (`EXT_float_blend` absent), framebuffer-fetch feedback (both names
absent), sRGB framebuffers (`EXT_sRGB` absent), and the software canvas as *the*
answer to the graphics standard (R5, R6).

**One thing the principles forbid saying:** that the GPU is faster. Nothing has
been drawn through it yet except a quad, so R7 stands empty until Round 2.

### GPU: verified here, and what is still unknown (2026-11-23)

Checked by reading this project's source and listing the toolchain, not by
research, so it is stated as verified:

- There are **eighteen** visualizer styles (`src/spectrum.c`,
  `vis_style_name()`), all drawn by `src/vis_art.c` into an SDL software
  surface. **No GPU code exists in `src/`** — no shader, no `gnm_` call, no GL.
- The app **already loads the GPU driver**: `src/main.c` calls
  `sceKernelLoadStartModule` on `/<sandbox>/common/lib/libSceGnmDriver.sprx`
  with `/system/common/lib/libSceGnmDriver.sprx` as fallback, logs the result,
  and calls `sceGnmSubmitDone()` when the system UI is over it. So GNM is
  reachable on this console and a driver session already exists; what does not
  exist is any drawing through it.
- The toolchain ships `libSceGnmDriver.so` and the `libSceGnmDebug*` stubs, so a
  GNM path is linkable today. It also ships `EGL/`, `GLES2/` and `KHR/` headers
  but **no** GLES/EGL/Vulkan stub library was found in `lib/`, which makes GNM
  the likely route and GL the unlikely one.
- The present path is the frame's hard floor: a full-frame copy into a 24-bit
  window surface, 21.6–24.5 ms on every screen. Any GPU renderer still has to
  hand its result to the same video-out flip, so that floor is part of the
  audit's arithmetic.

Unknown until the audit answers it, and to be researched with five phrasings
per topic before anything is decided: what GNM exposes for compute and shader
stages on 12.52; whether a GNM renderer can coexist with SDL's window surface
and its flip; whether text and chrome can stay on SDL while visuals move; what
bloom, particles and lighting cost on this GPU at 1080p; and what the port costs
in weeks. **No source disagreements recorded yet** — none has been researched;
the audit fills this in and flags the disagreements where it finds them.

### Video decode, verified and disputed (2026-11-23)

**Verified in our own source:** there is no FFmpeg and no CPU decoder in this
player. Video is `libSceAvPlayer` (Sony's system player), our per-frame work is
`convert_nv12()` at 5.91 ms average / 26.60 ms worst, and the drain loop
(`src/video_orbis.c` ~1430–1485) pulls at most 8 frames per step, breaks on the
first empty return, and increments `pull_fail` there — so that number counts
empty polls, not lost frames. `pull_rounds` is counted but never printed.

**What four other projects say, and where they disagree:**

- PS4-IPTV-Player: "video is decoded by the PS4's hardware decoder via
  `libSceAvPlayer`", and the decoder is fixed-function for **AVC/HEVC only**.
- Stremio-PS4: uses AvPlayer for MP4/H.264 and converts NV12 with SSE2 at about
  **4 ms per 1080p frame** — our 5.91 ms is the same order, so conversion is not
  the bottleneck either.
- ps4-uk-stream: **pPlay's mpv decodes in software** on the Jaguar CPU; the
  hardware decoder is only reachable through `SceAvPlayer`, with no FFmpeg
  bridge.
- Moonlight-ps4's plan: **"hardware decode has no precedent in PS4 homebrew"**,
  pointing at `libSceVideodec2` stubs and at wiliwili's software-only decode.

The last one is the disagreement: whether AvPlayer counts as "hardware decode"
or whether that name belongs only to direct `libSceVideodec2` access. For our
purposes it does not change the measurement — **our frames arrive from AvPlayer
and our budget goes to the present path** — but it does mean the claim "another
player does 8-bit H.264/HEVC in hardware" is about AvPlayer's internal decoder
and remains unverified here. jocover's TrueHD PKG proves the console's codec
blocks are reachable from a homebrew PKG through system libraries, not that our
path can bypass AvPlayer.

### Visualizer audit headline (2026-11-23)

Eighteen looks, host, 1640×560, 40 frames: step 1 runs **27.8–75.4 ms**, step 3
**11.2–17.8 ms**. Nothing fits a 60 Hz frame at full resolution even on the host,
so the console's 15–20 fps at step 2/3 is arithmetic, not a bug. No look contains
a shader, a bright-pass bloom, or a lighting model; "glow" everywhere is a soft
radial blit. Audio reacts through one global beat list (median interval, 60–160
BPM fold) with no per-band running average, no ~150 ms refractory, no geometric
decay, no packet interpolation and no automatic gain. Full table, effects
inventory and the two-target plan: `files/round7/README.md`.

### Amendments — A1 and A2 (text lives in `AGENTS.md`)

The register of rules changed by later evidence is kept in `AGENTS.md` with the rules
it amends. **A1**: per-pass `glFinish` as a timing method. **A2**: `u_blurRadius`
typed as a vec4. Each carries its before wording, after wording and evidence.

### GPU rendering paths that exist for PS4 homebrew (index, 2026-11-23)

**Every line here came from search snippets unless it says otherwise, and none
has been run on this console.** Treat the whole index as leads to verify, in the
order [§5](05-decisions.md) sets.

1. **Piglet / GLES2 via OpenOrbis — the only path already on this machine.**
   v0.5.2 (21 Dec 2021) added "OpenGL/piglet GPU rendering sample + headers"; the
   sample describes Piglet as "an OpenGL ES 2.0 and EGL 1.4 specs
   implementation" (Title ID BREW00071, derived from flatz's `ps4_gl_test`), and
   this project's toolchain include root already carries `EGL/`, `GLES2/`,
   `KHR/` — though **no GLES/EGL stub library was found in `lib/`** when checked
   here, which is the first thing to resolve.
   *Disagreement:* older write-ups say a GL app must load the system modules
   `libScePigletv2VSH.sprx` and `libSceShaccVSH.sprx`; one developer reports an
   SDL port needing no external modules. Whether that holds on 12.52 is
   **unknown** — the sample is the test.
2. **OpenGNM (`PS4-OpenGNM/opengnm`)** — GNM library with `sceGnm*` drop-in
   compatibility, a SPIR-V shader compiler (`opengnm-psbc`) reported
   hardware-validated on **FW 9.00**, and a set of small examples. Same family:
   `opengnm-stack`, a Vulkan 1.0 layer; an independent audit reportedly finds
   unsupported or stub paths in it. **Experimental** — record that word next to
   every result.
3. **PS4GL_GNM** — GLES-style driver on GNM with its own GLSL → GCN compiler and
   a PKG maker; stated limitation: `glReadPixels` cannot read 16-bit or float
   render targets (relevant if a frame is ever read back for capture).
4. **ps4-kit (`shitastical`)** — OpenOrbis with a newer clang, a runtime with
   crash logs and a profiler, and Mesa RADV on GNM with a VideoOut presenter.
   Attractive because it would give a real Vulkan/GL stack and a debugger, and
   because **this project has no profiler**; expensive because it is a whole
   build environment.

Decision rule already in [§5](05-decisions.md): smallest working sample first, every result —
including "does not build" and "crashes on launch" — logged in [§6](06-problems-and-fixes.md).

### The module question — ANSWERED 2026-11-23 (the four positions, then the log)

This is the disagreement the GPU test exists to settle. **All four are search
snippets or local reads; none has been run on this console.**

1. **Our own sample (read locally, verified with our eyes).**
   `PigletApplication::loadModules()` builds `/<sandbox>/common/lib/`, then
   `sceKernelLoadStartModule` on `libScePigletv2VSH.sprx` and on
   `libScePrecompiledShaders.sprx` — **not** `libSceShaccVSH.sprx` — and prints
   `Piglet module_start() = <rc>` / `PrecompiledShaders module_start() = <rc>` or
   `Failed to load the Piglet module, err=0x…`. Its PKG's `sce_module/` holds
   only `libSceFios2.prx` and `libc.prx`. So: **the system copies are expected,
   and the sample brings none.**
2. **orbisdev/orbisGlPerf (2018 log).** Both SPRX load from the sandbox path as
   ids 63 and 64, then the module base is patched ("patching module") — implying
   the system copy needed a runtime patch on that firmware.
3. **LÖVE PS4 port.** Demands both SPRX from **4.74 devkit** firmware, either in
   the PKG or at `/data/love/modules` or `/data/self/system/common/lib`, with
   exact sizes 744,208 and 10,394,272 bytes; "Without the modules, graphics fail
   to start". Its diagnostic signature is worth memorizing:
   `[rtld] syscall load_prx failed due to 0x0000000d` followed by "No available
   video device" — that is a **permission refusal**, not a missing file.
4. **ioQuake3-PS4.** The opposite: Piglet loads automatically from the console's
   own firmware and the devkit `libSceShaccVSH.sprx` is never needed;
   hardware-verified with both absent.

**The answer, from the console, 2026-11-23:** **system modules only, no devkit
files, no patch step.** In the v3 log the prefix is the title's own sandbox
mount — `module prefix: /BgZIO2hcQg/common/lib/` — and both modules load from
the firmware:

```
[    0.075] Piglet module_start() = 0 (0x00000000), handle 21
[    0.078] PrecompiledShaders module_start() = 0 (0x00000000), handle 22
[    0.079] STAGE 2  module load: both modules started
```

`libSceShaccVSH.sprx` (the LÖVE port's devkit file) was never requested and the
GL context came up without it, so on this console the devkit position is wrong
for this sample. There is no "patching module" line either — the 2018
orbisGlPerf behaviour was not needed here. **This applies to this sample, on
this console, on 12.52 with GoldHEN v2.4b18.14** — nothing more, and the other
stacks in [§9](09-ps4-knowledge-base.md)'s index remain unverified. What to read in the
log, in order: `Module prefix = …` (is the sandbox path the real one),
`Piglet module_start() = …` or `Failed to load the Piglet module, err=0x…`,
`PrecompiledShaders module_start() = …`, `Modules loaded OK`,
`EGL version major=… minor=…`, `Piglet num_configs = …`, then
`GL_VERSION` / `GL_VENDOR` / `GL_RENDERER` — the last three are the GPU answer
in the driver's own words. A `load_prx … 0x0000000d` before "No available video
device" means refused, not missing. Note also: LÖVE names `libSceShaccVSH` while
our sample names `libScePrecompiledShaders`; do not treat them as the same file
without evidence.

### Console GPU facts (as recorded, not measured here)

- **Base PS4:** 18 compute units, 1,152 shader cores, 1.84 TFLOPS, GCN, shared
  system memory with no separate VRAM.
- **PS4 Pro:** 36 compute units, ~4.2 TFLOPS, Polaris-based, dual-rate 16-bit
  (two FP16 operations per cycle).
- Ours is a **Pro**. **Whether homebrew here runs in Pro or base mode is
  unknown** and is now an open question ([§10](10-open-questions.md)) — it decides whether the visual
  budget is the 18-CU or the 36-CU one, so nothing may be assumed about it.
  The log does not print it; the check would be a GPU query (a GNM/submission
  path) or a measurement, not a guess.
- Shared memory means no VRAM ceiling to plan against, but the same 4.6 GB pool
  the audio, video and library already live in.

### Visualizer techniques worth adopting (2026-11-23)

- **MilkDrop-style warp feedback** — the projectM model: each frame is warped
  and blended with its own previous frame through a pixel shader, modulated by
  the music. It is the single technique that most separates "modern real-time"
  from "shapes on a canvas", and it is inherently GPU work. Licences not yet
  checked (projectM is the reference; its preset corpus has its own terms).
  Candidate only, per [§5](05-decisions.md).
- **Log-spaced spectrum bands** so bass gets resolution where the ear has it —
  our current bands are not log-spaced (audit item).
- **Per-band beat detection:** each band compared against its own running
  average, a threshold, a **refractory interval of about 150 ms**, and a
  **geometric decay** so a beat is a value that fades rather than a binary that
  flickers. Current code: global beat + BPM fold, no per-band refractory, no
  decay.
- **Interpolate between audio analysis packets** so visuals do not step at the
  packet rate (43 ms blocks with the current 2048-frame buffer).
- **Automatic gain** so quiet tracks still move — we have `level` in the log but
  no AGC pass.

Each of these is an audit line against every look, not a wish list.

### Existing PS4 media players to study (licences noted where known)

| Project | What to take from it | Licence / caveat |
|---|---|---|
| **pPlay** (mpv + ffmpeg) | many codecs, subtitles, HTTP/FTP streaming, custom mpv config, **non-Latin text via a bundled font file** — directly relevant to our `??????` bug | GPL-3.0; author states software decoding, so 4K lags |
| **PS4 Media Player** (pPlay fork) | reworked UI, cover art fetching, movie/show sorting, search tab | fork of a GPL-3.0 project |
| **Media Center for PS4** | favorites, last played, playlists per media type, safe USB-removal handling | not checked |
| **PS4 Player / PS4 Player 3D** | MP4/MOV, ASS/SSA subtitles, 3D and 180/360 from filename tags | not checked |
| **PS4 IPTV** | M3U and Xtream sources, continue watching, multiple audio/subtitle tracks, playback geometry | not checked |
| **jocover Dolby TrueHD decoder PKG** | how a homebrew PKG reaches the console's own decoder | **search snippet only, not opened** |
| **PS-PLAY 3.0** | claims optional hardware decode of 8-bit H.264/HEVC via the console's decoder | **target platform unconfirmed — do not rely on it** |

### Research still to be done (five phrasings each, disagreements recorded)

1. Official sample code for `libSceAudioOut` and `libSceVideoOut` in OpenOrbis, and
   what low-latency audio looks like for a visualizer (current block is 43 ms).
2. Network streaming for PS4 homebrew — the owner's list broke off here; the
   rest of the item is to be supplied before it is treated as complete.
3. What the PS4 GPU can do per visualizer topic: bloom, particles, lighting,
   compute, and at what cost at 1080p.
4. Whether a GNM/GL renderer can share the screen with SDL's window surface and
   its flip, or has to own presentation itself.
5. Text and chrome on SDL while visuals move to the GPU: possible, and what it
   costs in a composited frame.

### The three other logs the owner found (read 2026-11-23, all ruled out)

| File | What produced it | Relation to the Piglet run |
|---|---|---|
| `Loader_Logs.txt` (9,870 bytes) | the console's own loader verifying an RSA/SHA-256 signature on `/mnt/sandbox/NPXS39041_000/app0/Media/rsa.pub`, repeated dozens of times | **none.** NPXS39041 is a system application (its sandbox, its `Media/` folder), the lines carry no date, and the words BREW00093, Piglet, EGL and GL do not appear. It is a system app's sign-in/verification loop, not our sample. |
| `memory.log` (7,200 bytes) | `sceLibcMspaceCreate()` / `sceLibcMspaceMallocStatsFast()` pairs, one pair per line, no timestamps, no title id | **none.** It is a libc heap/mspace diagnostic dump — the kind a plugin or a debug build writes — and has no event in it. |
| `UnityOrbisBridge.log` (305 bytes) | a Unity-Orbis bridge: `Currently running on PlayStation 4 (12.50)`, `App has broken from the sandbox in 0s!`, SCE libraries loaded, SCE message dialog | **none, and it is six days older.** Dated 11/17/2026 02:57:47, firmware 12.50, and the bridge's own log — the Piglet run was on 11/23 on 12.52. |

So none of the three can say anything about the empty Piglet log. They are worth
one fact each: the console does run apps that write their own logs to disk, and
`UnityOrbisBridge.log` shows a homebrew app escaping its sandbox and writing a
dated log successfully on 12.50 — which supports "writing a log from homebrew
works here", and therefore supports the reading that v1's redirect itself failed
rather than every write being impossible. Marked as an inference, not a result.

### Logging from homebrew on 12.52, and where /data is writable (2026-11-23)

**Verified in this project's own code and packages:**

- The player writes `/data/ultimate-ps4-media-player/app.log` with
  `fopen(..., "w")` (`src/applog.c:225`), and its earliest boot line opens the
  same path with POSIX `open(..., O_WRONLY|O_APPEND|O_CREAT, 0644)` and `write()`
  (`src/main.c:91`). The owner pulls that file over FTP after every run, so
  **libc file I/O into a `/data` subfolder is proven on this console** — the
  method is proven, the folder is not.
- `sceKernelOpen`, `sceKernelClose`, `sceKernelWrite`, `sceKernelWritev` and
  `sceKernelFsync` are declared in `orbis/libkernel.h` (lines 339, 81, 451, 453,
  143) and linked through `-lkernel`; `dup2` is at `unistd.h:40`;
  `sceKernelGetFsSandboxRandomWord` at `libkernel.h:185`. All used by the v2
  package.
- **The toolchain's `fcntl.h` defines only `O_RDONLY`/`O_WRONLY`/`O_RDWR`.** An
  open that means to create a file has to supply `O_CREAT`/`O_TRUNC`/`O_APPEND`
  itself. This is a trap worth remembering: `sceKernelOpen(path, O_WRONLY, 0666)`
  cannot create anything, and a silent failure follows.
- The first logged package's `eboot.bin` carries `/data/piglet-gpu-test.log`
  (one string), so its redirect compiled in; the file still came back empty.

**Answered by the v3 run, 2026-11-23:** the `/data` root, `/data/pkg` and
`/data/ultimate-ps4-media-player` all accepted `sceKernelOpen` with
`O_WRONLY|O_CREAT|O_TRUNC` from this title — `path OK … (fd 5)`, `(fd 6)`,
`(fd 7)` — so a title **can** create files at the `/data` root and in
`/data/pkg`, and `O_CREAT`/`O_TRUNC` with the FreeBSD 9 values work at all
three. The three USB mounts answered `rc 0x80020002`, which is "no such file or
mount", not "permission denied".

**Still unknown, and not to be repeated as fact:** whether `freopen` on stdout
works in this libc (v1's silence says it did not, but v1 could not report its
own return); whether stdio buffers survive a PS-button close (v3's kernel writes
did, which is why it fsynced each line).

**Where the sources disagree** (five phrasings, checked 2026-11-23):

- ConsoleMods' *PS4:Files and Directories* describes `/data` as where **homebrew
  apps keep their own folders** (Apollo's user folders are the example) and
  lists `/mnt/usb0`, `/mnt/usb1`, `/mnt/usb2`, `/mnt/ext0` for removable
  storage — which supports "a `/data` subfolder per title", the pattern this
  player already follows, and the USB paths v2 tries. It does not say the
  `/data` root itself is writable by a title.
  <https://consolemods.org/wiki/PS4:Files_and_Directories>
- Two install guides (MacMyths, TechBloat) call `/data/` or `/data/pkg/`
  "writable", but they mean **writable over FTP by the user**, not by a running
  app's sandbox — a different permission subject, and the reason v1's silent
  failure cannot be ruled out from documentation alone.
  <https://macmyths.com/how-to-install-pkg-files-on-ps4-over-usb-and-ftp>
- The PS4 Developer Wiki's *Homebrew Enabler* page says a HEN-loaded fSELF has
  "system permissions, for example RW access to all filesystem partitions" —
  the direct counter-claim, predicting `/data` root *is* writable.
  <https://www.psdevwiki.com/ps4/Enabling_Homebrew> Unresolved; v2's six-path
  table answers it in one run instead of an argument.
- OpenOrbis' *Building Homebrew* says only that `/app0/` is the package root and
  that bundled data must be inside the package; it is silent on writable paths,
  so it neither supports nor contradicts the two readings above.

### Round-state notes carried over from `AGENTS.md` (2026-11-24)

Moved out of the rules file to bring it under the G9 ceiling. These are
history and measurement; the present tense lives in the where-we-are block
at the top of [§2](02-current-focus.md). Nothing here was rewritten — the text is as it stood.

- Latest build: **v116** — `files/Ultimate-Media-Player-v116.pkg`,
  MD5 `7e215289a0f6ec58a55c50e4058e6b3c`. **On the console 2026-11-23** — the
  first run of this build: Square clean (worst paint 18.5 ms), Liquid at step 2,
  15 fps. The three look tweaks from the v114
  television photos: a glint with a bright core and a wide halo (+66 luma over
  567 pixels, all below the lip, against +52 over 262 in v114), spray that
  leaves the crests instead of lying across the tank, and a floor lifted 0.03 of
  the frame. Liquid only; NEON/WORM/RAVE DJ byte-identical to v114. Host bench
  26.74 / 15.08 / 11.36 ms at steps 1/2/3, and reverting any one tweak does not
  make it faster, so all three are free within noise. Notes in `files/round5/`.
- Previous: v115 — `files/Ultimate-Media-Player-v115.pkg`,
  MD5 `25f3af25c68a57fb8509de28fd83e40e`. The room background is built in
  slices, so a skin change (SQUARE on the main screen, the 467 ms freeze in
  three sessions' logs) no longer stops the screen. Not yet confirmed on
  hardware. Notes in `files/round4/`.
- Previous: v114 — `files/Ultimate-Media-Player-v114.pkg`,
  MD5 `b98a943f1c72f7a2582cb4e61475b985`.
  Round 3 finished: the tank's front glass and floor now have real presence,
  the spray is bigger and lasts longer, and five glints travel along the crests.
  The crest line and the foam code were not touched in v113 or v114. Most of
  the glass was missing on the console because it was gated behind a quality
  step the console never climbs back from; it now draws at every step.
  Notes and renders in `files/round3/`.
  Round 3 (first pass): Liquid's water body is lit by a smoothed copy of the
  surface instead of the per-column crest value (which saturated and made the
  flat cyan), the depth ramp runs green under a crest to deep blue with a
  longer-lived blue channel, and the water line rises 8% of the frame via the
  new `[liquid] surface_lift`, with two soft lamps in the headroom. Crest and
  foam untouched; NEON, WORM and RAVE DJ render byte-identical to v112. Notes
  and before/after sheets in `files/round3/`.
  **Hardware test (4.7 min, step 3):** Liquid runs 50.0-60.8 ms a frame
  (3-3.6 vsyncs) with paint 21.4-24.8 and flush 22.2, against 62.5-63.2 ms in
  v112 - so the new water costs no frames, and this session is slightly faster.
  The ladder dropped 1 -> 2 -> 3 on TESSERACT at 98 s (156.6 ms, 110.1 ms) and
  stayed at 3. Liquid was on screen full-screen at 152-155 s and 253-257 s, but
  no capture of it came back, so the *look* is still unconfirmed on the console.
  The 467 ms Square freeze on the music screen happened again (15.9 s).
- Previous: v113 — `files/Ultimate-Media-Player-v113.pkg`,
  MD5 `ce7c55f0749743620a86630792bc2449` (Liquid water body, depth, framing).
- Rollback before that: v112 — `files/Ultimate-Media-Player-v112.pkg`,
  MD5 `3714cd0a30921b72f226e877926fee4f` (ladder judges the whole frame,
  Stage B machinery behind `soft_quarter`, off; logs in `files/round2/`).
- Rollback before that: v111 — `73b67059b6ddfec44e9942520c472770`
  (log clock + screenshot capture), and v110 —
  `05fc21e97fea99b0d382ffe9be6762bd` (cached skins).
- Source snapshots in `files/archive/`: v104
  (`d69a49a195ffcd9808506a38fd47017e`), v110
  (`1a81ec3cef4ccfd0b653b74f3ee9db3c`, cited, not archived). Make a new one when asked.
- Measured facts from Round 3 (host, 1080p, 100 frames x3, Liquid): v113
  costs 25.94 / 13.93 / 10.88 ms at steps 1 / 2 / 3 against 22.19 / 12.05 /
  9.34 for v112, i.e. 10-17% more depending on the run (the host drifts 4-8%
  between sessions; the same v113 code measured 23.86 / 12.95 / 10.29 in an
  earlier session). That is inside the 20% ceiling; on the console it is about
  3 ms on a 62-66 ms frame. The cost is the two headroom lamps and the extra
  column pass.
- **The Liquid body must not be lit by the per-column crest value.** It is a
  height divided by 0.012 m, a swell here is about 4 mm, so it pins at 1 across
  the tank - that is exactly what made the water one flat cyan and drew the
  height field as vertical bands. Light the body from a smoothed height with a
  palette scaled to the swell (FL_BODY_M = 30% of crest_m) and keep the raw
  column value for the rim and the foam only.
- Widening the light shafts to soften them is expensive: a halo ellipse costs
  +2.0 ms at step 1 and +1.4 ms at step 3, a 25% wider core +1.8 ms at step 3,
  because a wide ellipse covers the whole canvas once the canvas is stepped
  down. Dimming the core is free.
- **A look element gated behind a quality step is a look element the console
  never shows.** The console settles on step 3 within seconds and does not climb
  back, so Liquid's side panes, the glow over the water and the pane highlights
  (all behind `s_paint_step <= 2`) were simply absent on hardware - that, and
  not the design, is why the glass read as a thin strip. Cheap elements belong
  at every step; at a stepped canvas they cost a few thousand pixels.
- **Bench an untouched look as a drift control.** The host ran 6-10% slower in
  the v114 session (Rave DJ, untouched, went 53.72 -> 57.36 ms at step 1), so
  raw before/after percentages from different sessions are not comparable.
  Liquid at step 1 reads +22% raw and +14% net of that drift.
- Round order still to come: A3, A4, Stage C, D, E.
- Measured facts from Round 2 (host, 1080p, 100 frames x3): the SDL flush costs
  a constant 3.6-4.1 ms at every step (it is one full-frame canvas copy into the
  24-bit window surface); the rasterizer's own draw is 5-50 ms depending on look
  and step; run-to-run spread is about +/-5%. Quarter-res soft layers and a
  separable canvas upscale both measured *slower* and are not in the build.
- **Step-3 appearance (host render of v112, `VIS_STEP=3`, 2026-11-23):**
  LIQUID and WORM stay close to full resolution; RAVE DJ and NEON visibly
  soften. Numbers are in `files/round2/step3-quality/metrics.txt`; the edge
  metric is dither-dominated on the smooth looks, so the images are the
  evidence. The console screenshot round trip is therefore optional, not
  blocking.
- **v112 on hardware (2026-11-23, 8 minutes of log, two sessions):** the
  ladder fix works — it dropped 1 -> 2 -> 3 with whole-frame reasons and the
  session then held step 3; Rave at step 3 is 66.7 ms (4 vsyncs) against the
  ~100 ms it used to take; the flush floor is 21.6-24.5 ms with a music list
  that draws nothing; every look is 62-82 ms. All 18 styles were visited. The
  user's `config.ini` predates v112 (`quality.soft_quarter` reported missing,
  default 0 in effect).

[Manifest index](README.md) · [Back to the index](../README.md)
