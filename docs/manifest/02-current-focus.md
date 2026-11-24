# 2. Current focus

*Part 2 of 12 of the manifest — previous: [Main goal](01-main-goal.md) · next: [Roadmap](03-roadmap.md)*

### Where we are — the one block to read first (2026-11-24)

The single answer to "where is this project now". Updated on every build, package
and log read, never left to the end of a round. Prose history lives in [§4](04-build-log.md) and the
knowledge in [§9](09-ps4-knowledge-base.md); this block is only ever the present.

| Field | State |
|---|---|
| Latest player build | **v118** — `files/Ultimate-Media-Player-v118.pkg`, MD5 `349943cbd65ddb1580a9e47aec7de1b0` — logging only, **built, not yet run on the console**; v117 `290e7edab55e864e35d497b369828193` is the rollback |
| Latest GPU package | **BREW00103** — `files/GPU-Bloom-Slope-Probe-BREW00103.pkg`, MD5 `3bde518b33bf17627d5ab559b84f0084` — GPU-clock slope per A6/R24, `CLEAN` line now reported, one log path with automatic cleanup, package gated by `tools/check-pkg.py`, **built 2026-11-24, not yet run** |
| Built and not yet run | v118; BREW00103 |
| Already run on the console | Piglet sample v3 (GPU confirmed), BREW00094 catalogue (82 blobs), BREW00095 (502 at first frame), BREW00096 (85–100 fps, submission timings only), BREW00097 (15 fps; `glFinish` and vec4 bugs, see [§9](09-ps4-knowledge-base.md) A1/A2), BREW00098 (A empty 8.19 ms, B scene 8.32 ms, then C died on a uniform type — and both numbers are the present floor, not the work), BREW00099 (empty 16.68 ms = one vsync; n1/n4/n16 all VOID on `0x0502`), BREW00101 (one log folder works; all five configs `ok`; every `SET` refused with `Invalid program!` — the program was never bound), BREW00102 (installed after the package fix; **all 23 `SET` accepted**; panel legible; bloom visible; 16.656 / 16.654 / 16.658 / 16.659 / 33.340 ms for N = 0/1/4/16/64 = 1/1/1/1/2 refreshes) |
| Blocking question | **None left for the bloom proof of concept.** It is answered to decision grade: one chain is under half a millisecond by the CPU bound, so a 6-pass chain is at most ~3 ms of a 16.7 ms frame - affordable, whatever the exact figure. What is still open is the *player* side: the video path and the filename logging (v118) |
| Unblocked by | the owner's next console run of **v118**, which is logging no source has supplied yet. BREW00103 stays built and unrun on purpose: it would tighten the bloom figure from "under half a millisecond" to a decimal, and no decision needs that (R27) |
| Next action | run **v118** (`files/Ultimate-Media-Player-v118.pkg`, MD5 `349943cbd65ddb1580a9e47aec7de1b0`) and pull `app.log`: the `video: SRC` / `video: RATE` / `pull_rounds` lines are the only way to settle the 16 fps question |
| Reflection (R26/R27) | Before that run: **already recorded** - the 16.7 ms display step, the uniform binding, vsync, packaging, logs, the module load, the memory class, the blob set ([§7](07-environment.md) settled list). **Genuinely new** - the file's codec, rate, duration and bitrate, and whether the `??????` names arrive mangled or the font lacks glyphs. Both need this console and no source has them |
| Last console-verified | 2026-11-24, the BREW00102 log (GPU) and v117 (player) |
| Published | `shanerm420/ps4-media-player` — source, docs, `AGENTS.md`, tools and the notes on `main` at `c55591b`, tag `docs-2026-11-24-0209`, CI `docs` green; **verified 2026-11-24** by comparing every file with the master copy (63 of 66 identical; the three differences are the publisher's pointer block, the regenerated `AGENTS.md` header, and the generated `font_inter.h` that is correctly ignored). The tree here is ahead of it by the R31 and predictions pass |

**Focus: the GPU is reachable. Piglet ES2 runs on this console.** The v3 stage
log answered everything it was built for on the first run, and went past the
module question: the firmware's own `libScePigletv2VSH.sprx` and
`libScePrecompiledShaders.sprx` loaded with `module_start() = 0` (handles 21 and
22), `EGL 1.4` came up with a config, and the driver introduced itself as
`GL_VERSION: OpenGL ES 2.0 Piglet` / `GL_VENDOR: Sony Interactive Entertainment
Inc.` / `GL_RENDERER: Piglet`, with 935 bytes of extensions that include
`OES_texture_float`, `OES_texture_half_float` and
`EXT_color_buffer_half_float`. It presented 3,300 frames at about **120 fps**
before the owner closed it with the PS button. Three of the six log paths
opened, `/data` root included, so the empty v1 log was the `freopen`, not the
disk. The path is **CONFIRMED**; what is still unmeasured is what a real
visualizer costs on it. No player source was touched, and no visualizer is
rewritten before the proof of concept is built and approved.

**v117** is the player build on the console (header fix), rollback v116, then
v115. The `??????` filenames, the settings screen, the search index and the
visualizer overhaul were queued in the same message and are untouched.

**Video: the 16 fps is not decoding.** This player does not use FFmpeg — video
goes through Sony's `libSceAvPlayer`. During playback the log reads `paint=0.9`,
`flush=35–39`, `present=6–19`, `conv_avg=5.91` against `avg=50–67 ms`, so the
frame is spent in the presentation path (video blit plus the full-frame copy),
not in the decoder. The `15,397 pull failures` are **empty polls, not failures**:
the drain loop adds one on the first call that returns no picture and breaks, so
every productive drain also adds one. The counter needs `pull_rounds` (already
counted, never printed) before it can be read at all. What the log cannot say is
the file's codec, nominal frame rate, duration and bitrate — that logging is the
next step, and no player source has been touched.

**Audit: nothing fits.** All eighteen looks were timed on the host: the fastest
(LIQUID, 27.8 ms at step 1) is 1.7× over a 60 Hz frame and WORM is 4.5× over, so
native resolution is unreachable by tuning — which is why the console lives at
step 2/3, 15–20 fps at half or a third of the canvas. No look has a shader, a
real bloom pass or a lighting pass. Ranked gaps and the two-target plan are in
`files/round7/`.

**Header: fixed in v117, and v117 ran.** The title and status lines measure
themselves against the emblem's edge and end in `...` rather than running
underneath it. The v117 log (83.9 s, all eighteen looks toured with Triangle)
has no `long frame` after 7.2 s and no press over 66.8 ms, so nothing regressed;
whether the `...` appears is still only answerable on the screen.

**Pending from the owner — recorded as not reported, nothing assumed:**

- What the Piglet sample showed on the television: **not reported.** The log
  proves frames were presented (`STAGE 9`, 3,300 of them) and says nothing about
  what was visible.
- The v117 long-title header, whether the line ends in `...` before the emblem:
  **not reported.**
- The v116 look verdict — glint, spray, floor: **not given yet.**

The console steps in [§11](11-next-steps.md) carry the exact things to look at, so these arrive with
run reports rather than as questions.

**v118 is built and waiting on the console.** Logging only — no player logic
changed: `video: SRC` (brand, codec fourcc, bytes, resolution, duration,
bitrate), `video: RATE` (the file's own rate from 30 frame timestamps),
`pull_rounds` and `rounds_per_frame` in the `FILE` line and in the periodic
summary, `Stop`/`Close`/teardown/open timings, and one `library: name` line per
non-ASCII filename with its bytes, its UTF-8 validity and the atlas verdict.
MD5 `349943cbd65ddb1580a9e47aec7de1b0`. Then the GPU proof of concept, then the
two plans. No visualizer work before that proof is approved.

Why this matters to the main goal: the video answer shows the same wall as the
visualizers did — the present path, not the codec — and the audit turns the
graphics standard into a ranked list for the first time.

[Manifest index](README.md) · [Back to the index](../README.md)
