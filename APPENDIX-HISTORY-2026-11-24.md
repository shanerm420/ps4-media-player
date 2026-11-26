# PS4 media player — build history, 2026-11-24

Moved out of `HISTORY.md` on 2026-11-26: the live file had reached 97 KB against its
96 KB budget, which meant the next entry would have failed the gate - the same wall
2026-11-23 hit. Nothing was rewritten or deleted; this is that day's record, word for
word, in order. `HISTORY.md` (one directory up, beside `files/`) now holds 2026-11-25 onward and points
here. It sits beside `MANIFEST.md` because `make check` reads appendices from this directory,
which is how a superseded tag carried out of the history stays checkable.

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

### 2026-11-24 — The rebuilt docs are live, and the published copy was checked against the tree

- **The R31 and predictions pass is live, and it was verified rather than
  taken on trust.** The rebuild published four commits ending `f13e462` under tag
  `docs-2026-11-24-0243`, gate 77 files / 395 KB / 69 pages / 0 problems. The
  publisher made three commits, not four, and printed "nothing to commit" for the
  source stage: the source has not changed since `c55591b`, so a fourth commit
  would have been empty. Checked here against the tag: §9 carries R1-R31 as one
  unbroken table with both principle paragraphs below it, R31 has its own page,
  `APPENDIX-LOGS.md` is byte-identical, and the only differences are the
  publisher's pointer block and the regenerated `AGENTS.md` header. CI is reported
  green by the publishing run; it was not re-checked here, because the API budget
  on this machine is spent and raw fetches cannot read workflow status. §2's
  Published row now names `f13e462` and says this tree matches it.

### 2026-11-24 — The relay: how two agents talk, and where the work actually stands

This section exists because the chain is now a tool, not a workaround: two AI
agents (this one on one phone, DeepSeek/OpenCode on another) develop the player
through a shared relay on the owner's laptop. Written down so no session has to
rediscover it, and so the same relay can carry the next application.

**Why the laptop is the meeting point, not the phones.** A direct phone-to-phone
link was set up and keys written on both sides, but no inbound session was ever
observed: Termux `sshd` dies when the phone sleeps, and the DeepSeek agent runs
as root inside a proot container at `/root/ps4-project` with its own `$HOME` and
its own keys. The phones both reach the laptop reliably, so the laptop is the
fixed point and the phones only ever dial out. Nothing on a phone needs to
listen; `sshd` on the phone is not part of this.

**Phone network facts cannot be measured from the phone.** Android blocks
`/proc/net/*`, `netstat`, `ss` and netlink inside Termux (`Cannot open netlink
socket: Permission denied`). Those errors are the platform, not a broken setup.
A phone's address is only knowable from the laptop side, via `$SSH_CLIENT` in
the session it opened.

**The channel is one append-only log plus two shell commands.** On the laptop,
`~/relay/chat.log` holds the conversation as one line per message
(`HH:MM  name: text`); the append is a single `printf` to `O_APPEND`, so two
writers cannot interleave a line. `~/relay/say <name> <words…>` writes, and
`~/relay/read <name>` prints only what that name has not yet read, keeping its
bookmark in `~/.relay-seen-<name>`. The bookmark is per reader on the laptop, so
one agent reading does not swallow the other's messages. Install lines and the
same source live in `files/relay-say` and `files/relay-read`.

From either phone (`buff` or `deep` as the name):

```
ssh MX-Linux-2000@192.168.1.172 '~/relay/say buff here is my status'
ssh MX-Linux-2000@192.168.1.172 '~/relay/read buff'
```

**The laptop.** MX Linux 23 on an HP 2000 (2 cores, SysV init, not systemd).
`sudo service ssh start`; `S03ssh` in `/etc/rc2.d` is what starts it at boot;
`systemctl enable --now ssh` fails here by design. `192.168.1.172`, user
`MX-Linux-2000`. Lid-close is handled by Xfce Power Manager, not logind, so the
relay survives a closed lid only as far as that setting allows.

**Verified:** both install lines are exactly one line, exit 0 against an empty
home directory, and reproduce `files/relay-say` / `files/relay-read` byte for
byte; a simulated two-agent exchange behaved (writer A and B both land, A's
re-read reports nothing new, B still sees everything, B's reply reaches A).
**Not verified:** the same commands on the actual laptop; that is one
`~/relay/read buff` away and has not been seen.

**A detour worth not repeating.** At `ssh-keygen`'s "Enter file in which to save
the key" prompt the answer typed was `ssh-keygen`, so both phones now have
`~/ssh-keygen` and `~/ssh-keygen.pub` instead of `~/.ssh/id_ed25519`. The keys
are valid, login already worked without them, and pressing Enter at that prompt
is the whole fix. Appending a key to a phone's own `authorized_keys` only lets
that phone log into itself; the laptop's `authorized_keys` is what matters.

**Where the work stands, so no session restarts a finished round.** The
documentation tree is published at `f13e462`, tag `docs-2026-11-24-0243`, and was
checked against the tag from this machine. Ahead of that, locally and not yet
pushed: §2's Published row and this file. The next build round is the audio race
recorded in R31 — `s_playing`, `s_quit` and `s_track_finished` in
`src/main.c` (declared 157-159, written from the audio thread at 1883-1885, no
`pthread_mutex` in the file) — which wants one owner per flag under a mutex or a
generation handoff, in the shape `src/applog.c` already uses, and lands as
`Ultimate-Media-Player-v119.pkg` with proof on the console by stalling and
repeating a track. Still open and unrelated: draft release `453573773` has the
asset named `IV0000-BREM00001_00-ULTIMATEMEDIA000.pkg` while its body quotes the
`BREWMEDI1` MD5.

## 2026-11-25 — the authority rule (G11) and v135

The owner's instruction: amend the system so the acting agent is shown as the one in
charge of translating the project between the owner and the tools, using other AIs where
they help, dictating what is right and wrong within the owner's requirements, and then
building the package. Written as rule **G11** in `AGENTS.md` with its full text in
`docs/AGENT-AUTHORITY.md`, and pointed at from `docs/OPERATING-RULES.md` and `MANIFEST.md`
§2. The substance is not new authority — R30 already made every technical decision the
agent's — it is the missing half: the agent is also the judge of what the tools return.
A research agent, an image model or a second opinion is an instrument; its answer is a
claim to test against the repository and the measurements, and no tool's output overrides
the owner or a rule here.

Two other things changed in the same pass, both honest corrections rather than features.
The §2 "Latest player build" row still described v128 as scaling the canvas on the GPU,
which A13 proved false; that sentence is gone, replaced by the v135 row. And G5's reflow
lost the word "assumed" beside its 5% threshold, which `rule-index.py` caught as a number
with no status — restored, and the gate is the reason it was noticed.

`v135` is a rebuild with no source change: `APP_VER 1.35`, `VIS_PLATE_FX=1`, the same
renderer as v134, MD5 `3d3cfefccba922d6fa0f8ea0fdee34f2`. The first build of it came out
byte-identical to v134 because `make` saw the package as up to date; removing the package
and `sce_sys/param.sfo` forced the version into `param.sfo`, which is what made the MD5
change. Worth remembering: a version bump alone does not dirty the package.

One tool bug surfaced while landing G11: `tools/rule-index.py` recognised `**G1 —` blocks
but only stopped them at a `##` heading, so the first G-rule followed by a `###` heading
swallowed the rest of `AGENTS.md` into its body and it was filed as *measured*. The
stop pattern now accepts `##` and `###`. Nothing else in the file changed.

Gate at handover: unit tests all pass, 28 manifest checks, 45 rules, 18 visuals with 11
rebuilt, 0 problems.

## 2026-11-25 — the build answers what it can (v136)

The owner's instruction was to check the build setup for questions I had been
handing to the console, and to use AI where that saves work. Two research passes
and a handful of measurements later, eight findings, six of them defects that had
been sitting in the build:

1. **The build id was a lie.** `BUILD_ID` was the literal `v124` and shipped that
   way through v135, so a console log could not name its build. It is now
   `v<version>-<source hash>` from the Makefile and `tools/buildstamp.py`.
2. **The package is not reproducible**, and the Makefile said it was. Measured:
   same source, same eboot, two packages, two MD5s. Researched cause:
   `create-gp4` writes `volume_ts` from the clock and PkgTool writes
   `PUBTOOLINFO c_date` from it. The source hash in the binary is what proves
   provenance, and `check-pkg.py` reads it out of `eboot.bin`.
3. **`check-pkg.py` was never called** — R23 described a gate that did not exist.
   It is now part of every package build and of `make handover`.
4. **A version bump did not reach `param.sfo`**, which is how v135 came out
   byte-identical to v134. It depends on the Makefile now, and is written to
   `.tmp` and renamed, closing the half-written-SFO class for good.
5. **The SFO check asked for `FORMAT` and 11 entries**; the real file has 13 and
   no `FORMAT`, and those packages install. The expected set is read from the
   Makefile's own recipe now.
6. **A flags change recompiled nothing**, so `make VIS_PLATE_FX=1` on a plain tree
   shipped a package with no effects. `build/.flags` fixes it; verified 0 vs 21
   compiles.
7. **The hardware probe was silent** whenever `[quality] tier` was set, while the
   trip sheet asked for the line. It logs every run now.
8. **Same-or-older `APP_VER` install behaviour is still unknown** and stays
   unknown: no documented rule, community reports of an overwrite prompt only.
   Practice stays "bump every time".

v136 is the package: `APP_VER 1.36`, build `v1.36-ca00ee89f3da`, MD5
`3b256a909f67ac0954427757864e6285`, 17 package checks passing, tests and the
visuals bench green.
