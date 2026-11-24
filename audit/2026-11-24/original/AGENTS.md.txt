# Working rules — PS4 media player

**Read order:** [docs/README.md](docs/README.md) → this file (the rules) → [manifest §2](docs/manifest/02-current-focus.md) → [rule index](docs/rules/README.md) → [manifest §9](docs/manifest/09-ps4-knowledge-base.md) → [history](docs/HISTORY.md).

This is the published copy of the rules file. It is regenerated from the
project's canonical `AGENTS.md` by `tools/publish-docs.py`; §-references
point into the published manifest.

---

# Ultimate PS4 Media Player — working rules and measured facts

Read this before touching anything. It holds the rules, the conventions, the
measured facts and the long-form plan, so they do not have to be repeated in
chat. If something here conflicts with a new instruction, the new instruction
wins and this file is updated to match.

**Read order:** `files/STATUS.html` (state in one screen) → `MANIFEST.md` [§2](docs/manifest/02-current-focus.md)
(where we are) → this file (the rules) → `MANIFEST.md` [§9](docs/manifest/09-ps4-knowledge-base.md) (the knowledge) →
`HISTORY.md` (why it happened). Nothing later in that order overrides anything
earlier.

**`MANIFEST.md` is the spine of the project** and is read first: the main goal
verbatim, current focus, roadmap with statuses, the dated build log, decisions
with what was rejected, problems and fixes, environment, resources, the PS4
knowledge base summary, open questions and the next three steps. This file keeps
the engineering half — hard rules, workspace and measurement rules, established
SDL/PS4 facts, build commands, and the full plan text. The two never contradict
each other; both are updated together.

**[§12](docs/manifest/12-ps4-development-standard-merged-2026-11-24.md) is the PS4 development standard, merged 2026-11-24** and applying to every
job under this project, not only the player: merge rules and priority order,
the startup checklist, the plan template, the reference-source order, the extra
protected files, the working method for graphics work, and its conflicts with
our measured rules (ours kept, listed there). Read it with this file and never
weaken an existing rule because of it.

## Graphics standard (standing requirement, added 2026-11-23)

From the owner, and part of the main goal — see `MANIFEST.md` [§1](docs/manifest/01-main-goal.md) for the verbatim
text and the five ways it must be carried out. In short: every visualizer must
look as good as this console can make it (native resolution, steady frame rate,
smooth motion, bloom, glow, depth, particles, lighting, shaders where the
hardware carries them), not just Liquid; software, low-resolution or
low-frame-rate results are not acceptable where a better method exists; the
renderer audit and every quality trade-off are logged in the manifest with
reasons and rejected alternatives.

Engineering consequences for this file:

- The "our own rasterizer, no shaders" decision in `MANIFEST.md` [§5](docs/manifest/05-decisions.md) is **under
  review**, not settled. Do not cite it as a reason to avoid a GPU path.
- Eighteen styles exist (`vis_style_name()` in `src/spectrum.c`). Any claim about
  "the visualizers" means all eighteen.
- A GNM driver session already exists: `src/main.c` starts
  `libSceGnmDriver.sprx` and calls `sceGnmSubmitDone()` under the system UI. The
  toolchain links `libSceGnmDriver.so` and ships `EGL/`/`GLES2/` headers with no
  GLES/EGL/Vulkan stub library found in `lib/`. Facts checked 2026-11-23; the
  audit goes further.
- Measure before claiming: a quality change is a measurement in milliseconds and
  vsyncs, at all three steps, three runs, with the drift control.
- **GPU paths, in the order they get tried:** the toolchain's own piglet/GLES2
  sample first (headers and sample are already installed here), then
  `PS4-OpenGNM/opengnm`, then the experimental stacks. Smallest working sample
  first, nothing integrated into the app until one runs on the console. The
  index of candidates, and which lines are unverified search snippets, is
  `MANIFEST.md` [§9](docs/manifest/09-ps4-knowledge-base.md).
- Do not add module-loading guesses (`libScePigletv2VSH.sprx`,
  `libSceShaccVSH.sprx`) to the app: run the sample and read what the console
  says.
- Visualizer technique targets, from the same standing requirement: log-spaced
  spectrum bands, per-band beats with a ~150 ms refractory and geometric decay,
  interpolation between audio packets, automatic gain, and a MilkDrop-style
  warp-feedback effect once a GPU path exists.
- **Check `MANIFEST.md` [§9](docs/manifest/09-ps4-knowledge-base.md) *Graphics principles* before any graphics change.** It
  holds the loop (five-phrasing research, hypothesis, test in its own package,
  console measurement, written rule) and every rule established so far with its
  status: measured, researched only, or assumed. Host numbers rank approaches;
  the console's numbers decide them. Nothing already ruled out there — 32-bit
  float targets, float blending, framebuffer fetch, sRGB framebuffers, the
  software canvas as the final answer — may be proposed again without a new
  reason written next to the old rejection.

## Reply format and research rules

- Every reply opens with one line: current focus, plan step, what is next.
- Answer a detail, log it in `MANIFEST.md`, then restate that line.
- Say plainly what is verified, what is assumed and what is unknown. Never claim
  something works unless it was run.
- **Every reply ends with a "Logical next step".** One decision or action, named,
  together with everything the owner needs to make it: the exact file, package,
  version, number or choice, what happens on each branch, and what to look at or
  send back. It is a proposal, not a question — it never asks the owner to guess,
  to remind us of context, or to re-read an earlier message. If nothing is left
  to decide, say so and name the step after that. The step also carries its own
  instructions: the file and MD5 to FTP, the package source to install from, how
  long to run it, the log path and pull mode, the exact lines to send back — or,
  for a build step, the command, package name, MD5 and source hash. The owner
  should never need a follow-up question to act on it.
- **Fold owner statements in without being asked.** Anything the owner says that
  bears on the build, on this console, on the toolchain, or on a future project
  goes into the matching `MANIFEST.md` section on arrival: a fact into [§9](docs/manifest/09-ps4-knowledge-base.md), a
  console behaviour into [§7](docs/manifest/07-environment.md), a rejection into [§5](docs/manifest/05-decisions.md), a failure into [§6](docs/manifest/06-problems-and-fixes.md), a number into
  the Graphics principles table with measured / researched-only / assumed attached.
  Record the date and that the owner stated it. Contradictions keep both readings
  plus a note on how to test them; never overwrite one silently. Filter by
  usefulness, not by volume: passing preferences, things already recorded and
  conversational remarks stay out, because a file full of noise stops being read.
  If it is unclear whether a statement is durable, record it as
  `owner-stated, untested` rather than dropping it. The goal is a knowledge base a
  future app can be built from, not a diary.
- **Only visual choices are asked (R30).** A question may only be an aesthetic
  one; every technical question is answered from the manifest instead. When the
  owner asks to be consulted, state each option with its cost and what it unlocks.
- **Keep going without asking for a start.** A round that is already written into
  `MANIFEST.md` [§11](docs/manifest/11-next-steps.md) is authorized: build it, test it, package it, log it, back it
  up, and hand over the console steps in the same reply. Do not end a reply
  waiting for "shall I begin the next round". Stop and ask what the next move is
  only when the next step genuinely needs the owner — a console run or pulled
  log, a screen verdict, a choice between measured options, an approval the plan
  reserves to the owner, or a permission the hard rules gate (delete/move,
  publishing, an unnamed behavior change, porting a look before the proof of
  concept is approved). When stopping, name what is needed, why nothing else can
  proceed, and what each answer unlocks. While moving, every other rule still
  applies.
- **Look for the existing solution before asking.** If a problem has been met by
  someone else already — build error, missing symbol, driver refusal, libc
  oddity, console behaviour, package that will not install — find how they solved
  it before asking the owner for anything: the toolchain's own samples, headers
  and code, then ps4libdoc, the PS4 Developer Wiki, then open-source homebrew on
  the same library. Record the fix and its source in `MANIFEST.md` as any other
  researched fact. Ask only when the answer needs what no source holds — this
  console, this account, this owner's eyes, a run or a pulled log — or a choice
  between measured options. A failed build is a lead, not a question.
- **Existing work is a starting point, not a tutorial.** If a project already
  solved it, use their answer or their code's approach instead of re-deriving it,
  and name the project in `MANIFEST.md` [§8](docs/manifest/08-resources.md). Write it fresh only if nothing
  suitable or licence-clean exists, or if their solution demonstrably fails on
  this console — and record that reason before starting. This covers redundant
  *work*, not only redundant questions: a round and a console run spent
  rebuilding somebody's working answer is the expensive mistake.
- **New evidence amends the rule, and the amendment is recorded.** When a
  measurement, log line or source contradicts a rule, edit the rule itself in
  the same turn — do not leave the old wording standing and contradict it
  elsewhere. Each amendment records the date, the wording before, the wording
  now, and the evidence; the old text moves to a one-line "superseded" note in
  [§6](docs/manifest/06-problems-and-fixes.md) or the build log. **Stricter is automatic; weaker needs the owner's yes**
  for anything protecting files, data, legality or build stability. Ambiguous
  evidence keeps both readings, marked `contested`, with the test that settles
  it. The register of amendments is `MANIFEST.md` [§9](docs/manifest/09-ps4-knowledge-base.md).
- **Reflect on the manifest before every step and check the step against it
  (R26).** Before a build, run, research task or report line: reread [§2](docs/manifest/02-current-focus.md)/[§6](docs/manifest/06-problems-and-fixes.md)/[§8](docs/manifest/08-resources.md)/[§9](docs/manifest/09-ps4-knowledge-base.md),
  `PIGLET-NOTES.md` and the amendments, then name the recorded fact the step tests
  and what is new. Answer already there: cite it, spend nothing. Step already in
  the plan: drop it. Every build, every future project.
- Research a PS4 question with several phrasings before trusting an answer (a
  plain question, a technical term, a tool or repository name, an error message,
  a forum or tutorial style), note where sources disagree, prefer official and
  recent ones, and record each with its address in `MANIFEST.md` §Resources.
- If we appear to be repeating ourselves, stop and show the relevant manifest
  entries rather than re-arguing.

## What this is

A native PS4 homebrew media player built with the OpenOrbis toolchain: MP3
through minimp3 → resampler → 10-band EQ → volume → limiter → `sceAudioOut`,
video through `libSceAvPlayer`, and a set of music visualizers drawn by our own
rasterizer. UI and rendering are SDL2 (the znullptr SDL-PS4 port shipped with
the toolchain). No Sony SDK material of any kind.

- Language and target: C11, `x86_64-pc-freebsd12-elf`, freestanding PIE.
- UI: `src/ui.c` (chrome, screens, themes, background bake).
- Visualizers: `src/vis_art.c` (our rasterizer, quality ladder, styles).
- Audio: `src/audio_core.c`, `eq.c`, `resampler.c`, `mp3_source.c`,
  `audio_out_orbis.c`, `spectrum.c` (FFT + beat/tempo detector).
- Video: `src/video_orbis.c`, `src/subtitles.c`.
- Settings: `src/appcfg.c` → `config.ini`. Logging: `src/applog.c` → `app.log`.
- Build: `Makefile`; host preview in `preview/`; host tests in `test/`.

Console data path (all of it user data, never touched by the build):
`/data/ultimate-ps4-media-player/` — `app.log`, `app.old.log`, `config.ini`,
`shots/` (v111+ BMP captures).
Probe and test logs go to **`/data/Alpha PS4 test kit/`** from BREW00101 on: the
app creates the folder and writes `<name>.log` there, one place to pull from.

## Hard rules (never break, never "improve")

1. Never change the kernel version string.
2. No leaked Sony SDK material, keys or files. OpenOrbis headers and open
   sources only.
3. Circle never quits the app. It backs out of a folder, then a screen, then
   to Home. Quitting is the console's business.
4. US spelling in code, comments and docs ("color", not "colour").
5. The app icon, the UI chrome (header, progress bar, button prompts) and the
   pad scheme stay as they are unless a request names them.
6. SHARE is never mapped — it is the console's own button.
7. Labels sit under their emblems.
8. L2 / R2 page the music list.
9. Backing out of a folder returns to the artist/album/genre you were in.
10. Up / down is volume in the full-screen visualizer.
11. Do not change how any other visualizer looks unless that visualizer is
    named in the request.

## Delivery rules

- One package per build: `Ultimate-Media-Player-vNNN.pkg`, plus its MD5.
  No zips, ever.
- Never upload or publish anywhere without being asked first.
- Keep the previous package as the rollback.
- Reports are plain language, no tooling or path narration in chat. Every
  report ends with a **Workspace** section: files created, changed, moved,
  deleted (normally zero), build-check result, and anything to confirm.

## Workspace rules

- Never delete without first listing what would go and getting a yes.
- No `rm -rf`, no wildcards. Remove by exact name, after checking nothing
  refers to the file.
- One `_scratch/` directory for harnesses and safety copies. Tell the user
  before emptying it. No `_new` / `_backup` / `final_FINAL` copies anywhere.
- Protected — never delete, move, rename or overwrite: `src/`, `third_party/`,
  `tools/`, `test/`, `preview/`, the `Makefile`, build scripts, generated
  headers, `README.md` and this file, the shipped package and the rollback,
  and anything outside the project folder. Edit in place.
- Outputs go in `files/roundX/` (current naming; older rounds used
  `files/stageX/`) with the version and topic in the name. A re-render
  overwrites the same name.
- After any cleanup or risky change: full console build, preview build, and
  `make check`.
- Ask before deleting, moving or renaming anything, and before any behavior
  change that was not named.

## Measurement rules

- Measure before and after at all three quality steps, three runs each, and
  say plainly which numbers are PC preview only.
- PC numbers are a ranking, not a forecast: the console runs the same software
  path about **4× slower** than the host, and every frame waits on the display,
  so frame times come out as whole multiples of 16.7 ms. Report console results ([§9](docs/manifest/09-ps4-knowledge-base.md))
  in milliseconds **and** in vsyncs (100.1 ms = 6).
- No speedup claim without a measurement.
- Do not make anything more than ~20% slower at the default step without
  saying why.
- Every new tweakable goes in `config.ini` with a comment and a default.

## Rules G1–G10 (full text)

Moved here from `MANIFEST.md` [§12](docs/manifest/12-ps4-development-standard-merged-2026-11-24.md) on 2026-11-24: rules live in the rules file, and the
manifest cites their ids instead of restating them. This file does not exceed 640 lines (G9). These rules apply to every job under this project; each says how it is
satisfied and what evidence proves it, and where a number is needed and none has been
measured the rule says so instead of inventing one.

These are the eight items above turned into things that can be followed and
checked. They are **new rules, appended**, not a rewrite of anything in [§12](docs/manifest/12-ps4-development-standard-merged-2026-11-24.md), and
they apply to every job under this project from now on. Each says how it is
satisfied and what evidence proves it; where a number is needed and none has
been measured, the rule says so instead of inventing one.

### G1 — Memory budget per subsystem, stated in every plan

A plan is not
complete until it names a ceiling for each part that allocates: UI and canvas,
decoder buffers, effects and intermediate targets, and headroom. The ceilings
must sum to no more than **60% of the direct pool**, so a job cannot pass its
own test and still starve the next feature. The v118 line
`video: engine 1 holds 4 blocks / 19.1MB of the 4608MB direct pool` is the
sensor for the decoder part; effects add their own report. When a subsystem
reaches its ceiling, log it by name and degrade (drop a target size, skip a
pass) rather than failing to open the file. Starting split, **assumed, to be
corrected by measurement**: UI and canvas 25%, decoder 40%, effects 20%,
headroom 15%. Nothing may quote those four numbers as measured.

### G2 — Crash and hang runbook

In order, before any investigation: (1) is the
package still the one you meant — check the MD5; (2) pull the log and read the
**last** ten lines, not the first — a stage marker with no successor names the
step that stopped; (3) is it a hang or a crash — a hang keeps the last frame and
stops logging, a crash ends the log mid-line or leaves `kernel` fault text;
(4) remove one variable (another file, another look, another quality step) and
re-run once, not five times; (5) reinstall the previous package to confirm the
console is still healthy, which separates our fault from the console's; (6) only
then write the cause into [§6](docs/manifest/06-problems-and-fixes.md). Never debug on the console by repeatedly
reinstalling: one variable per run, and the log decides.

### G3 — A fixed benchmark, compared rather than re-derived

One benchmark,
unchanged between rounds: the host preview `seq` over the named looks at the
three quality steps, three runs, plus `one-shot.bmp` MD5s for the pixels. Numbers
and MD5s go in `files/bench/<version>.txt` and a round states the delta against
the previous file, not against memory. A round that changes rendering without a
`files/bench` line is not finished. The console remains the decider; the
benchmark exists so the console run is the only expensive measurement.

### G4 — Input latency, target two frames, not yet measured here

Poll-to-photon for one
button is part of any round that touches input: press a button that changes
something visible, capture at high frame rate or read the log's own frame
indices, and state the delay in frames. Target: **two frames or fewer** at the
step the console runs. If a change buys frame time by adding a frame of delay,
that is a loss and it is logged as one.

### G5 — Thermals and sustained clocks

Any run longer than five minutes states
whether the last minute was slower than the first. Compare the first and last
sixty seconds of frame times from one log; a drop over 5% is recorded as a
sustained-clock question in [§10](docs/manifest/10-open-questions.md), not smoothed away. Until a console reading settles it,
long-run numbers are quoted with their position in the run. The 5% threshold is an assumed starting point, not a console
finding.

### G6 — `config.ini` migration, never a silent reset

Renaming or removing a key
keeps the old name readable as an alias for one version, logs
`config key renamed <old> -> <new>`, and leaves the owner's value in place. A key
that is removed is logged as ignored, not dropped without a word. The startup
line `config loaded … (N keys, M missing, B bad, U unknown)` is the check: `bad`
and `unknown` rising is a migration bug in our code, not a user error.

### G7 — A clean-room note per borrowed idea

Whenever a reference shapes
something, one line goes into [§8](docs/manifest/08-resources.md): the technique, the reference, and what was
written here independently. Original code is not negotiable ([§12](docs/manifest/12-ps4-development-standard-merged-2026-11-24.md) legal), and
this is the record that proves it was.

### G8 — "Professional" defined so it can be failed, from the v112 and v115 console runs

At 1080p: a visualizer
holds **≤ 50 ms (3 vsyncs)** at the step the console actually runs, and draws at
**no less than 1640x560** for the look box before any step-down — the box size
the console already uses. Any look that cannot hold both states which one it
trades and why, in [§6](docs/manifest/06-problems-and-fixes.md). Chrome and menus hold **≤ 33.4 ms (2 vsyncs)** — already
measured on the music list. These are ceilings, not goals: a look that meets them
is finished even if it could be prettier, and a look that misses them is not
finished even if it looks good.

### G9 — The rules core has a size ceiling

`AGENTS.md` **must not grow past its
**640 lines** (635 when these rules were added on 2026-11-24, the plan text included, so the five lines of headroom are not an invitation) and this file's rules
sections ([§5](docs/manifest/05-decisions.md), [§11](docs/manifest/11-next-steps.md)'s reply rules, [§12](docs/manifest/12-ps4-development-standard-merged-2026-11-24.md)) must not grow past 900. The target is
smaller: 400 lines of rules, reached the first time the plan text is touched, by
moving that text to an appendix with a pointer. History, evidence and the knowledge base are not
counted and may grow without limit. When a rules section would pass its ceiling,
the older rule moves to an appendix with a one-line pointer — never deleted,
never weakened, just out of the way of the rules somebody has to read before
starting work. A rules file nobody finishes reading protects nothing.

### G10 — A new project starts its own file, and links back

Anything that is
not the media player — a game, a tool, a port — gets its own `AGENTS.md` and
manifest that inherit the ten rules above and this project's evidence standard,
and that do **not** inherit Liquid's water bands. The shared core stays here;
the new file holds only its own facts. Before the first build, the new project
reports the startup checklist ([§12](docs/manifest/12-ps4-development-standard-merged-2026-11-24.md)) against its own file.

### Amendments (A-numbered)

The register of rules that later evidence changed. The rule now reads as amended and
the old wording is kept here so a future reader can see what moved and why; the
manifest [§9](docs/manifest/09-ps4-knowledge-base.md) points here.

**A1 — per-pass `glFinish` as a GPU timing method.** *Before:* "per-pass timings
are GPU-side: `glFinish()` at every pass boundary" — implying a finish per pass
returns that pass's GPU cost. *After:* `glFinish()` per pass costs **~13–14 ms
regardless of pass size** (a 480x270 pass measured 14.6 ms against 18.3 ms for ([§9](docs/manifest/09-ps4-knowledge-base.md))
1920x1080), so it measures the round trip, not the work; only whole-frame timing
is trustworthy, and cost is established by **differencing configurations**
(BREW00098's A–E), with timer queries as the alternative when the entry points
exist. *Evidence:* `gpu-bloom97.log` 2026-11-24, 22 reports; corroborated by the
vendor-neutral rule that per-pass finishes stall both CPU and GPU. *Status:*
measured on this console; rule R15.

**A2 — `u_blurRadius` typed as a vec4.** *Before:* "`u_blurRadius type=0x8B50`
(vec4 — an x/y radius pair with room for two more components)". *After:*
`0x8B50` is **`GL_FLOAT_VEC2`**; it is set with `glUniform2f(p, 1/w, 0)` for the
horizontal pass and `(0, 1/h)` for the vertical, and a `glUniform4f` is rejected
with `illegal to set uniform data with different types`, leaving the uniform and
the kernel at their defaults. *Evidence:* `gpu-bloom97.log` 2026-11-24, the
`[PIG]E-glUniform4f:8900` line plus `CHK pass-blur glerr=0x0502`; the earlier
`blur1` and `blur2` figures in that run are void because of it. *Status:*
measured on this console; rule R16.

**A3 — treating a difference between configs as the cost.** *Before:* "cost is
established by **differencing configurations** (BREW00098's A–E), with timer
queries as the alternative when the entry points exist". *After:* differencing
only sees work larger than the present floor ([§9](docs/manifest/09-ps4-knowledge-base.md) R20) — `A empty` measured
8.19 ms and `B scene`, a 1080p ramp plus 2,000 sprites, measured 8.32 ms ([§9](docs/manifest/09-ps4-knowledge-base.md) R20), which
is the flip and not the draw ([§6](docs/manifest/06-problems-and-fixes.md)). Cost is measured by **repetition slope**: run N
passes per frame at N = 1, 4, 16, fit the line, and read the slope; the
intercept is the floor and is reported separately. Differencing stays valid only
where the work clearly exceeds the floor. *Evidence:* `gpu-bloom98.log`
2026-11-24: `CONFIG empty frames=120 ms=8.19 fps=122.1` ([§9](docs/manifest/09-ps4-knowledge-base.md) R20) and
`CONFIG scene … ms=8.32` ([§9](docs/manifest/09-ps4-knowledge-base.md) R20), then config C died. *Status:* measured on this console; rule R20.

**A4 — "the type was wrong" read from the console's log.** *Before:* A2 concluded
`u_blurRadius` is `FLOAT_VEC2` and "is set with `glUniform2f`". *After:* the driver
reports `u_blurRadius type=0x8B50 FLOAT_VEC2 size=1 **loc=49**`, and
`glUniform2f` **at that same location** is refused with the identical
`illegal to set uniform data with different types`, while `glUniform1fv` on
`u_gaussianKernel[0]` (loc=51) answers `Failed to retrieve uniform info`. So the
**location** the driver returns is not usable by its own setters, and no type
inference from the log can fix it. The rule is now: probe every set once, at link,
before any frame is timed; skip a uniform that refuses, and never spend a frame
discovering it. *Evidence:* `gpu-bloom99.log` 2026-11-24, the `SET`/`UNI` block
plus three `state=VOID err=0x0502` configurations. *Status:* measured on this console; rule R21. Superseded by A5.

**A5 — setting a uniform on a program that is not bound.** *Before:* A4 said the
location `glGetUniformLocation` returns "is not usable by its own setters".
*After:* setters act on the **currently bound** program (ES2 spec) and nothing was
bound, so every `SET` answered `Invalid program!`; BREW00097-101's "different
types" was the same mistake (loc 49 on texmap is `u_opacity`, a float). Bind with
`glUseProgram` first. *Evidence:* `gpu-bloom101.log`, `SET … REFUSED 0x0502` beside
`[PIG]E-glUniform2f:8372 - Invalid program!`. *Status:* measured; R22; replaces A4.

## Sources rule

When a PS4 function, structure or library is needed and its behavior is not
known, do not guess. Check in this order: OpenOrbis headers and samples →
ps4libdoc → PS4 Developer Wiki → open-source PS4 homebrew using the same
library. Say which source was used and flag anything unverified.

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

## Build and check

The compiler cache in `$HOME/.cache/ps4-toolchain` can be wiped between
sessions; `$HOME/openorbis` and `$HOME/hostdeps` survive.

```
# console (from the project root)
OO_PS4_TOOLCHAIN=$HOME/openorbis/PS4Toolchain \
PATH=$HOME/.cache/ps4-toolchain/cc/bin:$PATH \
make PKG_LD=$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu      # pkg+eboot, gated by check-pkg.py

# if the toolchain is gone: bash ~/bootstrap-toolchain.sh  (~2-4 min, micromamba, clang 18)

# preview + tests
make -C preview build/preview CFLAGS="-O2 -std=c11 -Wall -Wextra"
make check                                                     # must be ALL TESTS PASSED
```

Preview run needs
`LD_LIBRARY_PATH=$HOME/hostdeps/root/usr/lib/x86_64-linux-gnu:$HOME/hostdeps/root/usr/lib/x86_64-linux-gnu/pulseaudio`
and `SDL_VIDEODRIVER=dummy`. `./build/preview one <mode> [style [w h]]` writes
`one-shot.bmp`; comparing its MD5 between two builds is how rendering changes
are proven. `seq <mode> <style> <frames> <path>` runs a screen for a while and
saves the last frame (the visualizer's spray and glint do not exist on frame
one). `skin [frames] [switch_at] [path]` times every frame across a skin change.

## Delivery and test loop (FTP only)

The console is a PS4 Pro on firmware 12.52 with GoldHEN v2.4b18.14. Files move by
FTP and nothing else — confirmed by the owner on 2026-11-23.

1. Build as above → `ED1234-BREM00001_00-0000000000000000.pkg` + `eboot.bin`.
2. Copy it to `files/Ultimate-Media-Player-vNNN.pkg` and `md5sum` it; that copy
   is the archive and the next rollback.
3. Upload the same pkg over FTP to **`/data/pkg/`** on the console. GoldHEN's FTP
   server is port **2121** and must be on in GoldHEN → Server Settings.
4. On the console: **Package Installer**, package source
   **Hdd [`hdd:/data/pkg/`]**, pick the new version. FTP does not install
   anything by itself.
5. Run it, then pull the evidence back over the same FTP from
   `/data/ultimate-ps4-media-player/`: `app.log` (and `app.old.log`), `config.ini`,
   and any BMP in `shots/`.
6. Read the log here: `long frame` lines, the per-second `paint` / `flush` /
   `present` split, `quality step` changes, and `worst_paint` when a freeze is
   suspected. A freeze shows as a high `paint` with `flush` unchanged.

Do not write steps that use a USB stick, `usb:/`, or the 9090 payload server for
packages. None of them is part of this setup, and a loop nobody can run is
worse than none.

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

## Known open problems

1. Visualizer frames sit at **62-82 ms (4-5 vsyncs, 12-16 fps)** at step 3 on
   the console; the fast looks reach 50-60 ms (3-3.5 vsyncs). Rave improved to
   66.7 ms (4 vsyncs) from the ~100 ms (6 vsyncs) it ran at step 2 in v111. ([§9](docs/manifest/09-ps4-knowledge-base.md))
2. The canvas copy in the present path costs **21.6-24.5 ms on every screen**, ([§9](docs/manifest/09-ps4-knowledge-base.md))
   even one that draws nothing, so no screen can exceed 30 fps until the
   present path changes. At step 3 the frame splits roughly: rasterizer 16-40,
   copy 22, present 4-18.
3. NEON is the outlier: 116.8 ms (7 vsyncs) for a 2.5-minute stretch in the ([§9](docs/manifest/09-ps4-knowledge-base.md))
   first session with `flush=72.2` (three times the copy floor), but 70.1 ms in ([§9](docs/manifest/09-ps4-knowledge-base.md))
   the second session. Unexplained; needs one focused re-test.
4. The ladder does not climb back within a look (see the ladder fact above),
   so after the first drop everything runs at step 3 for the rest of the
   session — correct for the heavy looks, over-eager for the cheap ones.
5. The pad opens as joystick only.
6. Music stall: 200–217 ms frames when the full-screen visualizer switches on,
   66–100 ms when a track starts. This is what "the music stall" means until
   the user says otherwise. The **467 ms freeze on Square** was not the library
   list: it is the room being re-baked for the next skin. The log shows paint
   435 ms with an unchanged flush on a screen whose paint was 0.9 ms a frame,
   and the startup bake measures the same 441 ms. Fixed in v115 and **confirmed
   on hardware 2026-11-23**: forty-odd SQUARE presses, no frame over 50.1 ms, ([§9](docs/manifest/09-ps4-knowledge-base.md))
   `worst_paint` 18.5 ms, `paint` 9.2–11.8 ms while a bake was in progress on ([§9](docs/manifest/09-ps4-knowledge-base.md))
   every one of those frames.
7. Liquid: the flat cyan body, the vertical bands and the empty top third are
   addressed in v113 (see the Round 3 notes). Hardware numbers say the change is
   free (50-61 ms at step 3); what is *not* confirmed is how it looks on the
   television, because the console run was photographed on the Home and music
   screens only. v114 finishes the look: front glass, floor, spray and glint.
   Crest and foam line are good — they were not touched, keep them that way.
   Nothing named for this look is still open; what remains would be new work
   (a ripple on the glass, spray hitting the pane, reflected room detail).
   **On the console it settles at step 2, 15 fps** (66.7 ms a frame, paint ([§9](docs/manifest/09-ps4-knowledge-base.md))
   33.4): the ladder stops there because step 2 is judged against
   `quality.budget_ms` (88 ms) and 66.7 is under it — it only drops to step 3
   below ~11 fps. Step 3 would give 20 fps on a third of the canvas;
   `config.ini` `[quality] budget_ms = 62` is the dial if smooth beats sharp.
   A default, not a defect.
8. Skins: v110 cached repeat visits, but the *first* visit still baked the
   whole room in one frame (~0.44 s). v115 builds it in ~12 ms slices while the
   outgoing skin keeps drawing, so a change costs no frozen frame and the new
   room arrives about a second later. Same pixels: checked on the host against
   a build forced to do the whole bake in one go.
9. v111's log clock and touchpad capture are unproven on hardware.
10. **What step 3 looks like is now known without a console capture.** The
    preview build pins the step with `VIS_STEP=1|2|3`, renders the same code
    the console runs, and repeat runs are byte-identical. At 1920x1080 the
    visualizer box is 1640x560, so step 3 draws 546x186 and upscales 3x. Judged
    from those renders: LIQUID and WORM are close to step 1 (the water bands and
    the worm's body soften, the crest and the silhouette hold); RAVE DJ and NEON
    lose visible detail (dancer faces become blobs, the neon sphere and its ring
    highlight go chunky). Nothing new is broken by the drop - it is the same
    picture, softer - so the decision that follows is about *which* looks may be
    allowed to fall to step 3, not about repairing a defect. Evidence:
    `files/round2/step3-quality/`.

- **The state is on one screen, and it is generated.** `make status` writes
  `files/STATUS.html`: every host check with its result, the latest player and
  GPU packages with their MD5s, the blocking question, and what is still
  waiting on the console. It is never hand-edited; `make check` fails when it
  is older than the files it summarises. Read it first, then the block below.
- **Where we are is updated on every event, not every round.** The block at the
  top of `MANIFEST.md` [§2](docs/manifest/02-current-focus.md) is the single answer to "where is this project now":
  latest player build and MD5, latest GPU package and MD5, what is built and not
  run, what has run on the console, the blocking question, who unblocks it, the
  next action and the last console-verified date. Edit it in the same turn as any
  build, package, console run or log read; put history in [§4](docs/manifest/04-build-log.md) and knowledge in [§9](docs/manifest/09-ps4-knowledge-base.md),
  never in the block. `tools/check-manifest.py` fails when it names an older
  package than the newest in `files/`.

## Plan

The round plan is in `files/PLAN.md`: a work plan, not a rule, overriding nothing.

## State

State lives in `MANIFEST.md` [§2](docs/manifest/02-current-focus.md); round history in `ROUND-NOTES.md`.

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
