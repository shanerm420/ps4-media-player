# Appendix — state notes (N1-N5)

Moved out of `MANIFEST.md` on 2026-11-26 so the spine stays readable, and kept here
verbatim under stable labels. The manifest cites these by label; nothing here is a rule,
and no rule here overrides `AGENTS.md`.

| Label | Section | What it records |
|---|---|---|
| N1 | Latest player build | the artifact currently on the console |
| N2 | Console frame cost, from the v119 log | what one real session cost, screen by screen |
| N3 | Amendments — papers vs code | the 2026-11-25 claim-by-claim audit |
| N4 | Method | how that audit was run |
| N5 | Graphics build run | the build that came out of it |

---

## N1 · Latest player build

One answer, in §2 above - this section used to repeat it, and still said v126
three builds after v126 was superseded. A duplicate of the state block is how a
manifest starts lying.

## N2 · Console frame cost, measured from the v119 log from the v119 app.log (not archived here)

Every style, step 3 (canvas 640x360), 1920x1080 out. Median of the log's own
per-second `stats` lines. `paint` is the software canvas, `flush` the texture
upload and blit, `present` the swap, `avg` the whole frame.

| style | name | n | avg | p95 | paint | flush | present |
|---|---|---|---|---|---|---|---|
| 0 | SPECTRUM | 3 | 66.7 | 100.1 | 23.2 | 30.5 | 9.7 |
| 1 | WAVEFORM | 4 | 80.8 | 116.7 | 23.8 | 37.9 | 16.6 |
| 2 | RADIAL | 2 | 67.8 | 100.1 | 16.6 | 42.0 | 8.9 |
| 3 | PULSE | 2 | 62.3 | 91.8 | 20.5 | 29.5 | 11.9 |
| 4 | MIRROR | 3 | 78.3 | 116.8 | 26.4 | 35.9 | 15.5 |
| 5 | BLOCKS | 2 | 70.5 | 108.4 | 21.0 | 37.3 | 11.8 |
| 6 | SCOPE | 2 | 71.2 | 108.4 | 23.3 | 35.2 | 12.3 |
| 7 | ORBIT | 3 | 77.0 | 116.8 | 24.7 | 42.0 | 13.9 |
| 8 | LIQUID | 3 | 62.5 | 100.1 | 18.7 | 39.1 | 6.3 |
| 9 | NEON | 3 | 70.1 | 100.1 | 24.5 | 39.1 | 16.3 |
| 10 | FLAG | 2 | 79.2 | 116.8 | 29.6 | 38.8 | 10.4 |
| 11 | DANCE | 3 | 70.1 | 66.8 | 29.8 | 29.9 | 10.1 |
| 12 | WORM | 100 | 66.7 | 66.8 | 0.7 | 36.6 | 14.3 |
| 13 | WINDMILL | 10 | 73.7 | 116.7 | 28.6 | 31.6 | 11.7 |
| 14 | DANCE 2 | 4 | 73.7 | 116.7 | 32.9 | 30.6 | 8.1 |
| 15 | DANCE 3 | 7 | 78.3 | 116.8 | 29.2 | 38.2 | 10.4 |
| 16 | RAVE DJ | 5 | 82.1 | 116.8 | 33.4 | 39.2 | 7.2 |
| 17 | TESSERACT | 32 | 33.4 | 33.4 | 0.9 | 24.3 | 8.0 |

**What this answers, without another trip.** The Stage-3 effects are a small part
of the frame: paint is 0.7–0.9 ms on the two styles whose looks skip the canvas
(WORM, TESSERACT) and 16–34 ms on the canvas styles, while `flush` alone is
24–42 ms. No effect added by Stage 3 can move a frame from 66 ms into 16.6 ms, so
promotion is not blocked on measuring them; it is blocked on the canvas and the
upload. Between them, `paint + flush + present` is essentially `avg`, so both
terms have to come down, not one.

**`over=` is not the frame target — settled from the code, not guessed.** Every
line reports `over=0(0%)` while `avg` is 62–82 ms, which reads like a
contradiction until you follow it: `stats_frame()` compares the frame against
`cfg.quality.budget_ms`, and that setting is **88.0 ms by default** (56.0 at step
1) in `src/appcfg.c`. It is the quality ladder's trigger, not 16.6 ms. So `over=0`
means "the ladder had no reason to step down", and at 66 ms the ladder is already
at its floor (step 3) — which is worse news than a counter bug would have been:
there is no step left to take.

**Two candidate causes worth measuring next, both from the log itself:** the
canvas is painted at 640x360 (step 3) and still costs 16–34 ms, which points at
the per-pixel software path rather than at resolution; and `flush` is charged
while the GPU path is off, so the upload is a CPU-side copy of a full 1080p RGBA
frame — 8.29 MB per frame.


## N3 · Amendments — papers vs code (2026-11-25)

Each of these was a real disagreement between a written rule and the repository.
The rule is kept; the code or the text was changed to match it, and the change is
recorded here rather than left to be rediscovered.

1. **R23 was written down and never enforced.** The rule says "no package is
   handed over until `tools/check-pkg.py` passes it" and that "`all` depends on
   `check`". It did not: `all: $(CONTENT_ID).pkg` had no check dependency, and
   neither did `handover`. The exact failure this rule exists to prevent
   (BREW00102, `CE-33481-9`, half-written `param.sfo`) was therefore still
   possible on any build. **Fixed:** both targets now depend on `check`.
2. **The GPU path drew a frame and then let the canvas paint over it.** In
   `vis_art_draw()` the block called `gpu_vis_frame()` and fell through into the
   canvas path, which painted the same scene again and blitted that — so even
   with a working Piglet surface the picture would have been the canvas, at twice
   the cost, and the GPU work thrown away. **Fixed:** it is a takeover that
   returns, behind `VIS_GPU_TAKEOVER`, because no Piglet surface has ever been
   confirmed on this console; without the flag the GPU code is not called at all.
3. **`gpu_vis.h` described an environment gate that no longer exists.** The
   header said "only active when the environment asks for it", while the call
   site said the console has no shell so it is not env-gated. **Fixed:** the
   header now describes the compile-time flag and the fail-closed behaviour.
4. **The delivery step named a package the build never produces.**
   `ED1234-BREM00001_00-0000000000000000.pkg` in `AGENTS.md` vs the real
   `IV0000-BREM00001_00-ULTIMATEMEDIA000.pkg`. **Fixed** in place.
5. **`make handover` names files from `PKGVER`, not `VERSION`.** A version bump
   on one produced a v125-named file holding v126 code. Recorded, and both move
   together now.
6. **`docs/VISUALIZERS.md` promised bench lines for every rebuilt look; none
   existed.** **Fixed** by running `make bench-vis`: 11 rebuilt looks measured at
   1920x1080 on this host — SPECTRUM 42.4, WAVEFORM 45.2, RADIAL 37.0, PULSE 35.8,
   MIRROR 42.7, BLOCKS 35.5, SCOPE 31.6, NEON 36.3, FLAG 43.2, WORM 49.9,
   WINDMILL 29.5 ms. `files/bench/visualizers.txt` now exists.
7. **Two different budgets were being quoted as one.** `docs/visualizers.csv`
   gives every look a 50 ms budget, which is the host bench's pass line; the
   product target is 16.6 ms for 60 fps. A look can pass the first and be nowhere
   near the second, and the console log shows exactly that (33–82 ms frames on a
   machine whose ladder is already at its floor). Both numbers are now stated
   with what they mean.
8. **The approved icon was never in the build.** It lived in `design/` while
   `sce_sys/icon0.png` held the old one, so any package "with the new icon" was
   not. **Fixed** at 512 px in v127 onward.

9. **The console's cost was the upload, not the painting.** Frames split paint
   17-34 ms, flush 24-42 ms, present 6-17 ms; the old path upscaled the canvas to
   1080p on the CPU and uploaded 8.29 MB every frame regardless of step. v128 made
   the upload canvas-sized (0.92 MB at step 3) and the ladder should now climb.
   Full argument, with sources: `docs/PS4-PLATFORM-NOTES.md`.
10. **The build assumed one console.** Every step ceiling and budget was chosen
    as if the machine were this one PS4 Pro, and the ladder could never climb
    back once it dropped, because a step it had never measured was judged
    unmeasurable rather than tried. **Fixed in v132:** an unmeasured step is
    probed after the settle and kept only if it fits, so the build finds the
    machine it is on - base, Slim or Pro - instead of being told by a compile
    flag. `[quality] tier` (0 auto, 1 base, 2 pro) states it instead; tier 2
    tightens the budgets to 16.6/25.0 ms. The budget defaults moved from 56/88 ms
    (which knowingly passed 11-18 fps) to 20/33.4 ms, which is 60 fps and 30 fps.
    Rules R32 and R33.

11. **The host benchmark measured a moving target.** `bench-vis` let the ladder
    run, so a heavy look stepped down mid-run and the number was a blend: the
    same look read 35.0 ms pinned and 17.9 ms unpinned. **Fixed:** the bench pins
    `VIS_STEP=1`, takes the median of three runs (this VM drifts 15%, enough to
    flip SCOPE over the line at 48.1 then 51.7 on one build), and says so in the
    header. Pinned medians, 1080p: SPECTRUM 40.2, WAVEFORM 43.2, RADIAL 34.3,
    PULSE 33.5, MIRROR 40.9, BLOCKS 42.8, SCOPE 47.1, NEON 34.1, FLAG 42.0,
    WORM 44.0, WINDMILL 27.3 ms - every look inside the 50 ms line, after three
    real trims: BLOCKS halo only on its near half (63.7 -> 42.8), SCOPE four
    phosphor bands instead of eight (61.9 -> 47.1), WORM halo 5.2r -> 3.8r
    (50.6 -> 44.0).
12. **Outside research found four missing pieces** (2026-11-25; sources §Resources):
    (a) **no hardware probe** - `sceKernelIsNeoMode()` and
    `sceKernelGetCpuFrequency()` were declared and never called; v133 logs
    `probe: neo mode N, cpu N MHz`, and `tier = 0` means the probe decides (Neo
    is a *mode*, so a Pro in base mode correctly reports 0);
    (b) **canvas rows were not 16-byte aligned** - 546 px is 2184 bytes a row; a
    PS4 port measured ~50 ms for a 1436-wide upload against ~1.7 ms at 1440, so
    v133 pads the width to a multiple of 4 px and crops on present (free here,
    unproven on the console);
    (c) **SDL and Piglet do not composite** - a window-surface flip and an EGL
    surface are separate paths, so `VIS_GPU_TAKEOVER` stays off: it would draw a
    look with no UI over it;
    (d) **A7 supersedes "our own copy"**: present blocks on a vblank. A PS4 port
    a PS4 port measures that copy at 2.3 ms inside a 21.9 ms frame (§9), so copy plus memset is
    ~5 ms and the wait is the rest.
    Verified: `icon0.png` is 512x512, 8-bit, PNG type 2 (no alpha); `param.sfo`
    has its 11 entries.




## N4 · Method

`docs/OPERATING-RULES.md` sets the order of resort for every question: manifest
and existing logs first, then the code, then the web, then a host build, then a
batched console trip, and only then the owner. A question the repository can
answer is never put to him.

## N5 · Graphics build run

The whole graphics build is now a single ordered chain in
`docs/BUILD-CHECKLIST.md` §I, with each step's dependency named. Six stages:
shell materials → effects on the host → C renderer behind a flag → one batched
console measurement trip → promote only what held 16.6 ms → identity and ship.
Only two waits exist in the chain: the console trip and the owner's verdict on
which styles get promoted. Everything else proceeds without a decision.

Approved so far: the icon (`design/icon-v7-1024.png`, five image-to-image passes),
six section doors, the room plate, four content sheets, and all eighteen
visualizer world plates (`files/assets/vis-*.jpg`).
13. **"The GPU scales the canvas" was false, and it was mine.** The SDL-PS4 port
   has no accelerated renderer - its PS4 render driver is compiled out and the
   software one reports `SDL_RENDERER_SOFTWARE`. So `RenderCopy` scaling is
   `SDL_BlitScaled` on the CPU and `SDL_HINT_RENDER_SCALE_QUALITY` does nothing
   there. v134 corrects the comment, replaces the claim, and records the
   measurement that settled it (an integer expansion + 1:1 copy cost 11.3 ms
   against 10.4 ms at step 3, so the blit stayed). Same research added the pieces
   v134 ships: pad removal/reconnect handling, and an atomic `config.ini.tmp` +
   `rename()` save. See `docs/PS4-PLATFORM-NOTES.md`.
