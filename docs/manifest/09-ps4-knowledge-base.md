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
| R20 | **Frame timing here sits on a ~8.3 ms present floor, so differencing two configs cannot see the work** — `A empty` measured 8.19 ms and `B scene` with a 1080p ramp and 2,000 sprites measured 8.32 ms, a 0.13 ms difference for work that cannot plausibly be free; anything drawn under the floor disappears into the flip | **measured** (console, 2026-11-24) | `gpu-bloom98.log`: `CONFIG empty … ms=8.19` and `CONFIG scene … ms=8.32`. The method is repetition slope: run N passes per frame at N = 1, 4, 16, fit the line, read the slope. Differencing stays valid only for work larger than the floor. The slope is a CPU-side window, so it cannot resolve work below a frame press: [superseded A6] |

| R21 | **A uniform's set must be probed before it is trusted; the location the driver returns is not always accepted by the driver's own setter** — `u_blurRadius` (reported `0x8B50 FLOAT_VEC2`, loc 49) refuses `glUniform2f` at that location with `illegal to set uniform data with different types`, and `u_gaussianKernel[0]` (loc 51) refuses `glUniform1fv` with `Failed to retrieve uniform info`, each leaving sticky `0x0502` | **measured** (console, 2026-11-24) | `gpu-bloom99.log`: the `UNI`/`SET` block, then `CONFIG n1/n4/n16 … state=VOID err=0x0502`. Rule: try each set once at link, drain the error, print the result, and skip a uniform that refuses (A4). A skipped uniform leaves the driver's defaults, so the picture is not the intended one but the pass cost is unchanged |
**Ruled out by the driver, not by preference — do not propose these without a new
| R22 | **One log, in `/data/Alpha PS4 test kit/`, and delete what earlier builds left** — old builds opened seven sinks at once, so every run scattered copies that had to be deleted by hand | **decided** (owner, 2026-11-24) | One path via `sceKernelMkdir` + retry, then `sceKernelUnlink` over an **exact-name** table (no patterns), logging `CLEAN removed=N refused=M`. The player's `app.log`, `app.old.log`, `config.ini` and `shots/` are never in it |
reason:** 32-bit float render targets (`EXT_color_buffer_float` absent), float
| R23 | **No package is handed over until `tools/check-pkg.py` passes it** — BREW00102 installed as Corrupted Data (`CE-33481-9`) then refused to start (`CE-34544-0`): its `param.sfo` had 7 entries of 11, because a killed build left it half-written and `make` kept it | **decided** (owner, 2026-11-24) | `all` depends on `check`: SFO parses, 11 entries, required keys, CONTENT_ID/TITLE_ID match the Makefile, 36-char id, `CNT` magic, not truncated. `make clean` after any interrupted build |
blending (`EXT_float_blend` absent), framebuffer-fetch feedback (both names
| R24 | **GPU cost comes from the GPU clock; a CPU window is only an upper bound** — draws + `glFinish` with no swap in the window still gave 16.656 ms at N = 0, 16.659 at N = 16, 33.340 at N = 64 | **measured** on this console, 2026-11-24 (A6) | `gpu-bloom102.log` BREW00102 `CONFIG empty draw_ms=16.656`, `n16 16.659`, `n64 33.340`. Wrap the frame in `GL_TIME_ELAPSED_EXT` via `EXT_disjoint_timer_query` and fit the slope on `gpu_ms`; report a CPU window as "< one frame" and name the instrument behind every number |
absent), sRGB framebuffers (`EXT_sRGB` absent), and the software canvas as *the*
| R25 | **Read what other PS4 homebrew and the ES2 spec already solved before a round** — two console runs (A4, A5) went into a uniform question the spec answers in one line | **decided** (owner, 2026-11-24) | Order: ES2/EGL spec → this project's [§9](09-ps4-knowledge-base.md) facts → `shipofharkinian-ps4`, `Stremio-PS4`, `shadPS4` → a console run. Findings in `PIGLET-NOTES.md`; borrowed ideas get a clean-room note ([§8](08-resources.md)) |
answer to the graphics standard (R5, R6).
| R26 | **Reflect on this manifest before every step, and verify that step against it** — the owner had to point out that BREW00102's timing question was already published (`shipofharkinian-ps4`) and the uniform question was answered by the ES2 spec, after both had been sent to the console as tests; work that repeats a recorded answer is waste, and it is the owner's time being spent | **decided** (owner, 2026-11-24) | Before any build, console run, research task or report step: reread [§2](02-current-focus.md), [§6](06-problems-and-fixes.md), [§8](08-resources.md), [§9](09-ps4-knowledge-base.md), `PIGLET-NOTES.md` and the amendments, and state in one line **which recorded fact the step tests** and **what is genuinely new**. If the answer is already recorded, do not spend the run - cite it instead. If a step duplicates one already in the plan, drop it. Applies to every build and every future project (G10 inherits it); a round that skips this reflection is a defect in the round, whatever the build |

| R27 | **The hardware is known: never spend a run on what [§7](07-environment.md), [§9](09-ps4-knowledge-base.md), `PIGLET-NOTES.md` or the measurement rules already settle** — BREW00103 was queued to ask "why 16.66 ms" when the measurement rules have said since v112 that every frame waits on the display and lands on whole 16.7 ms multiples; the owner had to say so | **decided** (owner, 2026-11-24, A7); the 16.66 ms step itself is measured - `AGENTS.md` measurement rules and `gpu-bloom102.log` BREW00102 | Every console request names the one unknown it closes and quotes the recorded fact it does *not* contradict. A run to confirm the display contract, the module load, the memory class, the packaging route, the log path or uniform binding is refused, and the recorded line is cited instead. Hardware questions go to [§7](07-environment.md) "settled" list first; only behaviour absent from it can be tested |
**One thing the principles forbid saying:** that the GPU is faster. Nothing has
| R28 | **The target is the PS4 family, not this one console** — the work is PS4 software, and PS4 is the common denominator: same OS, same GLES2/Piglet, same audio, video, pad and package interfaces on every model, so working PS4 software runs on all PS4s | **owner-stated 2026-11-24** (not yet measured on a second model) | One package, no model branches. Every number names the model; a Pro result is an upper bound and the base PS4 is the budget floor. Firmware and GoldHEN are per-console setup, not features of the software |
been drawn through it yet except a quad, so R7 stands empty until Round 2.
| R29 | **Do not reproduce a method that already exists - reuse it** — this project has rebuilt things that were already there: a bespoke logger and an on-screen text renderer for every probe while the player already had `applog` and its own text drawing, and a uniform-binding answer the ES2 spec states | **decided** (owner, 2026-11-24) | Before writing anything, look in this order: this repository's own proven code (`src/applog.c`, the UI text renderer, the ladder), the OpenOrbis samples, a licence-clean library, then the published technique - and use it instead of re-deriving it. Write new code only where nothing fits or the licence forbids it, from the documented method and never pasted (G7 clean-room note). Two implementations of one job must name why the second exists, or the second one goes |

| R30 | **Ask the owner only about how it should look; every technical question is answered here and work continues** — the owner's instruction, 2026-11-24: "the only thing that should be asked is aesthetic questions... if it has to do with programming or coding in any way... refer to the Manifest to find the solution instead of asking the user" | **decided** (owner, 2026-11-24) | A question to the owner is allowed only when the answer is a **visual choice** - which look, which palette, sharper or smoother, this layout or that. Anything technical (an API, a driver refusal, a format, a build error, a timing method, a structure, a default value) is resolved from this manifest, `AGENTS.md`, `PIGLET-NOTES.md`, `APPENDIX-LOGS.md`, `ROUND-NOTES.md`, the ES2/EGL spec, the OpenOrbis samples, then other PS4 homebrew - and the answer is written back into the manifest in the same turn, so the next session finds it. If a technical answer genuinely is not in any of those, the fallback is a decision recorded with its reason, not a question. Asking the owner to choose a technical option is a defect in the round. A **console run is not a question**: it is a step that needs their hands, and the reply gives it with the file, MD5, install route, what to do and which lines to send back |
### Carried from other PS4 homebrew (`researched only`; text in `PIGLET-NOTES.md`)

Interval 0 is right for vsync; a `gde` app gets ~768 MiB direct and ~255 MiB
flexible, not 4.6 GB; shader compiles 110-260 ms and uncacheable; no
`glBlitFramebuffer`, 16-bit offscreen depth. Vsync was never the probe's problem
- `eglSwapInterval(display, 0)` is already in the sample
(`PigletApplication.cpp:182`) - so BREW00102's 16.66 ms with no present is still
open and BREW00103 measures it with the GPU clock.

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

**A7 — asking the console for a step the rules already fix.** *Before:* A6 treated
"why 16.66 ms with no present" as an open question and BREW00103 was built to
answer it. *After:* the measurement rules in `AGENTS.md` have said since v112 that
**every frame waits on the display, so frame times come out as whole multiples of
16.7 ms**; 16.656 / 16.659 / 33.340 are one, one and two refreshes, which is that
rule obeying itself, and no run was needed to learn it. What A6 legitimately adds
is only the *cost of the chain*, which the display cannot show and the GPU clock
can. *Evidence:* `AGENTS.md` measurement rules; `gpu-bloom102.log` BREW00102.
*Status:* recorded fact, restated; rule R27. **Owner-stated 2026-11-24:** "I am
sure you're missing a lot if you're having me do any testing on known hardware."

**A6 — a CPU-side timed window cannot measure GPU work below one frame.**
*Before:* A3/R20 said cost comes from a repetition slope, and BREW00100-102
closed the window at `glFinish` before the swap to get under the present floor.
*After:* with **no swap inside the window at all**, BREW00102 still measured
`draw_ms=16.656` at N = 0, `16.659` at N = 16 and `33.340` at N = 64, so the
driver quantises any CPU-side finish to one refresh: the window can only say
"less than a frame", and the slope it produced (0.278 ms/pass, r2 0.9509) is one
point crossing a vblank, not a measurement of the chain. The repetition slope
stays the method; the instrument becomes the GPU's own clock -
`EXT_disjoint_timer_query` is published here (section 9), so a frame is wrapped
in `GL_TIME_ELAPSED_EXT` and `gpu_ms` is read back. *Evidence:*
`gpu-bloom102.log` 2026-11-24, five `CONFIG` lines and
`RESULT slope ms_per_pass=0.278 intercept_ms=14.913 r2=0.9509`. *Status:*
measured on this console; rule R24; BREW00103 implements it. Its reading
of the 16.66 ms as an open question is [superseded A7].


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

### Logging notes (moved)

The three stranger logs that were read and ruled out, and the survey of where
`/data` accepts writes on 12.52, are in **`APPENDIX-LOGS.md`** - evidence behind
R22, not rules (G9).

[Manifest index](README.md) · [Back to the index](../README.md)
