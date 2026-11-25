# 2. Current focus

*Part 2 of 12 of the manifest — previous: [Main goal](01-main-goal.md) · next: [Roadmap](03-roadmap.md)*

### Where we are — the one block to read first (2026-11-25)

The single answer to "where is this project now". Updated on every build, package
and log read, never left to the end of a round. Prose history lives in [§4](04-build-log.md) and the
knowledge in [§9](09-ps4-knowledge-base.md); this block is only ever the present.

| Field | State |
|---|---|
| Latest player build | **v124** — `files/Ultimate-Media-Player-v124.pkg`, 6,619,136 bytes, MD5 `9609b3b3711cf6cf84cf94f3bb432b3f` — the second-era design system plus the first screen rebuilt on it: one type ramp (14/18/22/26/34/44/60/92 px), a 4-based spacing scale, four corner radii, translucent slabs with a lit top edge and soft shadow instead of 1 px borders, letter-spaced small caps, a five-door home (one hero stage, five keepsake drawers, minimal hint tokens) and a baked stage falloff behind the interface. **Built 2026-11-25, not yet run.** v123 was the previous language (rail + cards) and is the rollback |
| Latest GPU package | **BREW00103** — `files/GPU-Bloom-Slope-Probe-BREW00103.pkg`, MD5 `3bde518b33bf17627d5ab559b84f0084` — GPU-clock slope per A6/R24, `CLEAN` line now reported, one log path with automatic cleanup, package gated by `tools/check-pkg.py`, **built 2026-11-24, not yet run** |
| Built and not yet run | v124 (supersedes v123, which was never run); BREW00103 |
| Already run on the console | Piglet sample v3 (GPU confirmed), BREW00094 (82 blobs), BREW00095 (502 at first frame), BREW00096 (85–100 fps, submission timings only), BREW00097 (15 fps; `glFinish` and vec4 bugs, [§9](09-ps4-knowledge-base.md) A1/A2), BREW00098 (A empty 8.19 ms, B scene 8.32 ms, then C died on a uniform type), BREW00099 (empty 16.68 ms = one vsync; n1/n4/n16 all VOID on `0x0502`), BREW00101 (all five configs `ok`; every `SET` refused with `Invalid program!`), BREW00102 (installed after the package fix; **all 23 `SET` accepted**; panel legible; bloom visible; 16.656 / 16.654 / 16.658 / 16.659 / 33.340 ms for N = 0/1/4/16/64), **v119** (2026-11-25: 2,921 tracks, ladder 1→2→3 at 110.8 / 94.7 ms, floor 21.39–21.56 ms, no errors) |
| Blocking question | **None left for the bloom proof of concept.** It is answered to decision grade: one chain is under half a millisecond by the CPU bound, so a 6-pass chain is at most ~3 ms of a 16.7 ms frame — affordable, whatever the exact figure. What is still open is the *player* side: the `video: SRC` / `video: RATE` / `pull_rounds` lines never appeared in the v119 run (E10), and the audio handoff has one clean session behind it, which is not proof |
| Unblocked by | nothing: the **v119** run happened 2026-11-25 02:41:26. The redesign order is `REDESIGN-CHARTER-2026-11-25.md` §7. BREW00103 stays built and unrun on purpose: it would tighten the bloom figure from "under half a millisecond" to a decimal, and no decision needs that (R27) |
| Next action | **Install and run v124 on the console** — `files/Ultimate-Media-Player-v124.pkg`, MD5 `9609b3b3711cf6cf84cf94f3bb432b3f`. The home screen is the only screen rebuilt so far; it is the reference for the rest. Pull `app.log` and read the menu pass: `paint` / `flush` / `pres` on the menu must not have moved the present-path floor (E7/E8 in `REDESIGN-CHARTER-2026-11-25.md`), and the screen verdict decides whether this is finally the level before the other five screens are converted |
| Reflection (R26/R27) | Before that run: **already recorded** — the 16.7 ms display step, the uniform binding, vsync, packaging, logs, the module load, the memory class, the blob set ([§7](07-environment.md) settled list). **Genuinely new** — the file's codec, rate, duration and bitrate, and whether the `??????` names arrive mangled or the font lacks glyphs. Both need this console and no source has them |
| Last console-verified | **2026-11-25, v119** (player, run 02:41:26); 2026-11-24, the BREW00102 log (GPU) |
| Published | `shanerm420/ps4-media-player` — source, docs, `AGENTS.md`, tools and notes on `main` at `b445e68` (merges `295f9db`, then `fa4f556`, then PR #3), with this sync on top; release `v1.0.0` **published** 2026-11-24T23:48:23Z with asset `IV0000-BREM00001_00-ULTIMATEMEDIA000.pkg`, 6,619,136 bytes, MD5 `fb2189115fdb683cd52b3148d77562c2`, SHA-256 `482d214c92c66784107675eed09b759f88c7a97ef7e8f294766e4dcc80530f59`, both read from the asset and confirmed against the archived copy of the same bytes; CI `links` green on `b445e68`. **Provenance:** tag `V1.0.0` (`78ac0bb`, 21 Nov) is the build source of the shipped asset; tag `v1.0.0` points at the docs merge and is the release's tag, so its source zip is not the source of that binary. `main` also carries the unverified A3/A3b renderer change, which this tree does not — the two sources differ |

**Focus: the interface is being rebuilt on a design system, and v124 is the first
screen on it.** The design-system pass of 2026-11-25, written up as
`REDESIGN-CHARTER-2026-11-25.md`, replaced the per-screen refinements that
produced v121–v123: one type ramp, one spacing scale, four corner radii and one
slab material, with structure carried by type size, air, shadow and a single
accent instead of boxes. v124 contains the system plus the home screen rebuilt on
it, and is **built, not run** — the console verdict on that one screen decides
whether the same system converts the other five. The phase order is the charter's
§7, carried in [§3](03-roadmap.md).

**The GPU is reachable. Piglet ES2 runs on this console.** The firmware's own
`libScePigletv2VSH.sprx` and `libScePrecompiledShaders.sprx` load
(`module_start() = 0`), `EGL 1.4` comes up, the driver introduces itself as
`GL_VERSION: OpenGL ES 2.0 Piglet` / `GL_RENDERER: Piglet` with float and
half-float texturing available, and it presented 3,300 frames at about 120 fps.
The bloom chain then followed: BREW00102 set 23 of 23 uniforms and drew a real
pass, and the cost is bounded at under ~3 ms for six passes (R24/A6). The path
and the effect are therefore **proven and affordable**; what is not proven is a
look port and SDL/Piglet surface sharing, and the look port needs the owner's
go-ahead before it starts ([§3](03-roadmap.md)).

**Audit: nothing fits.** All eighteen looks were timed on the host: the fastest
(LIQUID, 27.8 ms at step 1) is 1.7× over a 60 Hz frame and WORM is 4.5× over, so
native resolution is unreachable by tuning — which is why the console lives at
step 2/3, 15–20 fps at half or a third of the canvas. No look has a shader, a
real bloom pass or a lighting pass. Ranked gaps and the two-target plan are in
`files/round7/`. Every visualizer must still meet the graphics standard in
[§1](01-main-goal.md), so tidying the software canvas is not a substitute for
moving the looks onto the GPU.

**Video: the 16 fps is not decoding.** This player does not use FFmpeg — video
goes through Sony's `libSceAvPlayer`. During playback the log reads `paint=0.9`,
`flush=35–39`, `present=6–19`, `conv_avg=5.91` against `avg=50–67 ms`, so the
frame is spent in the presentation path (video blit plus the full-frame copy),
not in the decoder. The `15,397 pull failures` are **empty polls, not failures**.
The v119 run was built to print `video: SRC` / `video: RATE` / `pull_rounds` and
none of them appeared (E10), so the file's codec, rate, duration and bitrate are
still unknown and no player source has been touched since.

**Working tree, not in any package:** an uncommitted visualizer design round on
five looks in `src/vis_art.c` (NEON, DANCE, DANCE 2, DANCE 3, TESSERACT), with
host renders in `files/ultimate-ps4-player/design/review2/`. It is software-
canvas art, it is not built into a package, it has not run on the console, and
it is waiting on the owner's verdict. It is **not** approval to port a look.

**Pending from the owner — recorded as not reported, nothing assumed:**

- The **v124** home screen: resolution, crispness, the type ramp at 10 feet,
  whether the five-door home reads without instructions. **Not reported.**
- What the Piglet sample showed on the television: **not reported.** The log
  proves frames were presented and says nothing about what was visible.
- The v117 long-title header, whether the line ends in `...` before the emblem:
  **not reported.**
- The v116 look verdict — glint, spray, floor: **not given yet.**

The console steps in [§11](11-next-steps.md) carry the exact things to look at, so these arrive with
run reports rather than as questions.

Why this matters to the main goal: the video answer shows the same wall as the
visualizers did — the present path, not the codec — and the audit turns the
graphics standard into a ranked list for the first time.

[Manifest index](README.md) · [Back to the index](../README.md)
