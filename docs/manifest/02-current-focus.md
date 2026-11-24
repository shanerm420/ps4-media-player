# 2. Current focus

*Part 2 of 12 of the manifest — previous: [Main goal](01-main-goal.md) · next: [Roadmap](03-roadmap.md)*

### Where we are — the one block to read first (2026-11-24)

The single answer to "where is this project now". Updated on every build, package
and log read, never left to the end of a round. Prose history lives in [§4](04-build-log.md) and the
knowledge in [§9](09-ps4-knowledge-base.md); this block is only ever the present.

| Field | State |
|---|---|
| Latest player build | **v118** — `files/Ultimate-Media-Player-v118.pkg`, MD5 `349943cbd65ddb1580a9e47aec7de1b0` — logging only, **built, not yet run on the console**; v117 `290e7edab55e864e35d497b369828193` is the rollback |
| Latest GPU package | **BREW00098** — `files/GPU-Bloom-Cost-Probe-BREW00098.pkg`, MD5 `a5607cc90de5a17491566a4e949ef4d1` — **ran 2026-11-24: two of five configs, then a clean exit on a uniform type**, see [§6](06-problems-and-fixes.md) |
| Built and not yet run | v118 |
| Already run on the console | Piglet sample v3 (GPU confirmed), BREW00094 catalogue (82 blobs), BREW00095 (502 at first frame), BREW00096 (85–100 fps, submission timings only), BREW00097 (15 fps; `glFinish` and vec4 bugs, see [§9](09-ps4-knowledge-base.md) A1/A2), BREW00098 (A empty 8.19 ms, B scene 8.32 ms, then C died on a uniform type — and both numbers are the present floor, not the work) |
| Blocking question | What does a 1080p bloom chain cost? BREW00098 could not answer: two of five configs ran and both sat on an 8.3 ms present floor, so the draw cost is hidden behind the flip |
| Unblocked by | a build that measures by repetition slope instead of by differencing (BREW00099, [§6](06-problems-and-fixes.md)) — no console run until then is worth its two minutes |
| Next action | build the repetition-slope probe (BREW00099): N passes per frame at N = 1, 4, 16, fit the line, slope is the cost |
| Last console-verified | 2026-11-24, the BREW00098 log |

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
