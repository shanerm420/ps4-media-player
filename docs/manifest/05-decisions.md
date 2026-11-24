# 5. Decisions

*Part 5 of 12 of the manifest — previous: [Build log](04-build-log.md) · next: [Problems and fixes](06-problems-and-fixes.md)*

Each entry: what was chosen, why, and what was rejected.

- **Native PS4 homebrew with OpenOrbis, C11, no Sony SDK.** Rejected: porting an
  existing player, or a scripting layer — both cost more than they save and
  neither teaches the platform.
- **SDL2 (the SDL-PS4 port in the toolchain) for UI and drawing.** Rejected:
  raw GNM — a month of work before a first picture.
- **Our own rasterizer for the visualizers** (`src/vis_art.c`) rather than
  pre-rendered video or shaders. Rejected: per-style shaders (the JS/GL era of
  the project) because the PS4 path is a software canvas. **Under review since
  2026-11-23**: the graphics standard in [§1](01-main-goal.md) requires the best the console can do,
  and the audit decides whether the software canvas is still the right home for
  any of the eighteen looks. Not overturned yet — nothing has been measured
  against a GPU path — but no longer settled either.
- **The graphics standard outranks the software canvas (2026-11-23).** Chosen:
  measure every look against what the console's GPU can do and move rendering
  onto it where that wins, rather than keeping on tuning the rasterizer. The
  requirement is the owner's; the plan is the audit's. Rejected: declaring the
  current look finished because the software path is what exists — that would
  answer the wrong question.
- **Quality trade-offs are logged, dated and revisited (2026-11-23).** The
  quiet defaults are decisions, not backgrounds: the quality ladder's step-2
  budget of 88 ms, `budget_ms = 62` as a manual way to prefer 20 fps over
  sharpness, and the stepped canvas at steps 2 and 3 all sit in [§5](05-decisions.md) and [§6](06-problems-and-fixes.md) with
  their reasons. Rejected: leaving them implicit in `appcfg.c` defaults, where
  they read as design rather than as choices that expired.
- **GPU paths are tested in a fixed order, smallest sample first (2026-11-23).**
  piglet/GLES2 from the toolchain first, because the headers and a sample are
  already on this machine; then OpenGNM and its shader compiler; then the
  experimental stacks. Chosen so that a failure at each level is informative and
  cheap. Rejected: starting with the most capable stack — an unproven Vulkan
  layer on an unproven firmware is the worst place to discover a problem, and
  nothing about it would be measurable on the console first.
- **No MilkDrop-style effect until a GPU path is proven (2026-11-23).** The
  warp-feedback idea is recorded as a candidate ([§9](09-ps4-knowledge-base.md)), not a plan. Rejected for
  now: porting projectM wholesale — its presets are shader-and-equation driven
  and its licence needs checking, and it assumes a GPU path that does not exist
  here yet.
- **The GPU test is its own package, not a player build (2026-11-23).** The
  sample ships under `BREW00093` with its own content id, so it cannot overwrite
  the player and it does not consume a version number: the player is v116 now
  and the next player build is v117. The player's source was not touched.
  Rejected: folding the test into the player — it would put an unproven driver
  path into the app that has to work, and a crash would be ambiguous between the
  sample and the player.
- **No visualizer is rewritten for the GPU until a path is confirmed on this
  console (2026-11-23).** A failure of this sample is a fact about the sample,
  not a verdict on the console: log it, then test the next alternative one at a
  time, in the [§9](09-ps4-knowledge-base.md) order. Rejected: starting the port against piglet now — if the
  module or symbol situation is wrong at 12.52, the port is wasted work on the
  wrong API.
- **Header: fit the text to the emblem's edge (2026-11-23).** `text_fit()` trims
  and adds `...`, using the same geometry the emblem is drawn from so they cannot
  drift. Rejected: scrolling the line — motion in an otherwise still header, and
  it needs its own timer; hard clipping with no mark — it looks like a broken
  filename, which is what was reported.
- **`[quality] budget_ms = 62` stays an option, not a change (2026-11-23).** It
  would move Liquid to step 3: 20 fps at a third of the canvas instead of 15 at
  half. Recorded because it is the only interim lever that exists; rejected for
  now because it trades sharpness for smoothness and still misses the standard.
- **A 960×540 room baked once per skin, upscaled once, then blitted unscaled.**
  Rejected: drawing the room live (325 ms a frame on console) and blitting a
  scaled texture every frame (45 ms a frame, measured).
- **Built in slices, not all at once.** Rejected: pre-baking all four skins at
  startup (+1.3 s of launch) and making the bake cheaper by changing its maths
  (would change the look, and float rounding is not free to prove).
- **Quality ladder judged on the whole frame, not the rasterizer alone**
  (`vis_art_frame_ms()`). Rejected: judging paint only — it kept looks at a step
  the frame could not afford.
- **Keep the visualizer's cheap elements at every step.** Rejected: gating by
  `s_paint_step`, because the console settles at step 3 and never climbs back, so
  gated elements simply never appear.
- **Judge looks with the host renderer at a pinned step (`VIS_STEP`) instead of
  waiting for console screenshots.** Rejected: asking for a capture of every
  change — the round trip is slow and the host runs the same code.
- **Bench an untouched look as a drift control.** Adopted after the host was
  measured 6–10% slower between sessions, which made raw before/after numbers
  meaningless in both directions.
- **FTP is the only delivery path, both directions.** Packages go to
  `/data/pkg/` and are installed from Package Installer's Hdd source; logs,
  screenshots and `config.ini` come back over the same connection. Rejected:
  USB (the common tutorial route) — the owner does not use it, and writing steps
  for a method nobody runs is how a build loop rots. Also rejected: pushing
  packages through PayLoader/BinLoader on 9090, which sends payloads, not
  packages.

- **GPU test logging is its own package and its own module (2026-11-23).**
  The second logged build writes with `sceKernelOpen`/`sceKernelWrite`/
  `sceKernelFsync` from the first statement of `main()`, tries six locations and
  records every return code, redirects fd 1 and 2 at the first file that opened,
  fsyncs each line, and heartbeats for 30 s. It keeps title id BREW00093 and the
  same content id so it replaces the earlier logged build, and it uses no player
  version number. Rejected: a second `freopen` variant — the first failed
  silently, and a second guess would fail the same way with the same silence;
  rejected: logging built into the player — the player is not under test and its
  source stays untouched; rejected: USB-only output — the owner's loop is FTP
  and no stick is part of it, so USB is a fallback tried third, not the plan;
  rejected: a thread that writes asynchronously — one more moving part in a
  build whose only job is to be readable.
- **The stage log is v3, and v2 is superseded without ever running
  (2026-11-23).** Numbered stages plus a frame line every 60 frames turn a
  truncated file into a location, which is the whole point of the build. v3
  keeps the kernel-call writer, the six-path table and the fsync per line.
  Rejected: sending v2 as well — two packages with the same title id means the
  second install silently replaces the first, so there would be one result and
  two candidates; rejected: drawing the stage number on screen — it needs a
  texture and a program the sample does not have, and a clear-color tint would
  mean editing `Render()`, the drawing path this build must not change;
  rejected: creating a test half-float render target to prove support — a
  fragile driver is exactly what this run is measuring, so the extension list
  and the limits answer it without allocating anything.
- **Piglet/GLES2 is where the visualizer work goes (2026-11-23).** Confirmed on
  this console rather than argued: EGL 1.4, a config chosen, `GL_RENDERER:
  Piglet`, ES2 shaders with derivatives and float/half-float texturing, and
  120 fps on the sample. The plan is one look on the GPU with half-float render
  targets and a real bloom pass, in its own package, measured before anything is
  ported. Rejected: starting with GNM or OpenGNM — the smallest sample already
  works and OpenGNM's shader compiler is validated on 9.00, not 12.52, so it
  would add risk before any picture; rejected: moving the UI onto GL as well —
  the chrome, the text and the flip work today, and the 21.6–24.5 ms present
  floor stays whatever draws above it; rejected: rewriting a visualizer now —
  the cost of a GPU look is unmeasured, and an unmeasured rewrite is how a
  renderer change becomes a rewrite of all eighteen looks.
- **The round order is logging, then the proof of concept, then the plans
  (2026-11-23).** v118 prints the video and filename facts with no logic change,
  because that round cannot break rendering and it answers two open questions at
  once; then one GPU look measured at 1080p; then the handover plan and the
  60 fps budget written from those numbers. Rejected: the proof of concept
  first — it would leave the video and `??????` questions waiting on a renderer
  change for no gain; rejected: writing the 60 fps budget before Round 2 — a
  budget with an unmeasured cost per pass is a wish list, and the principles
  section forbids it.
- **The proof of concept carries no player version number (2026-11-23).** It
  has its own title id and its own name; v118 stays the player's next number.
  Rejected: calling it v119 — it would read as a player build in the package
  list, and it cannot replace the player anyway.
- **Every graphics decision is a rule with its evidence (2026-11-23).** [§9](09-ps4-knowledge-base.md)
  *Graphics principles* records the loop and the status of each rule, and no
  later decision may repeat a rejected approach without a new reason. Rejected:
  deciding by taste or by what the host preview looks like — host numbers rank
  approaches, the console's numbers decide them.

[Manifest index](README.md) · [Back to the index](../README.md)
