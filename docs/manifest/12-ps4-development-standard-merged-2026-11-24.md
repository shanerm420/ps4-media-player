# 12. PS4 development standard (merged 2026-11-24)

*Part 12 of 12 of the manifest — previous: [Next steps](11-next-steps.md) · next: —*

**Changelog:** 2026-11-24 — the owner's "PS4 Development Manifest: build any
program, to professional standard" was merged into this file as [§12](12-ps4-development-standard-merged-2026-11-24.md), following
its own Part 1 rules: nothing existing was deleted, rewritten or weakened, an
existing rule was left alone where it already says the same thing, and every
conflict is listed below with the existing rule kept. `AGENTS.md` carries the
same pointer and the rules that change how code is written. Parts 1–13 of the
owner's document are summarised here; the document itself is the source, and
where this summary is shorter it is the summary that is wrong.

**What it is for:** the project is no longer only the media player. Any PS4
program may be built — media player, visualizer, game, emulator port, utility,
file or network tool — to a professional standard, using this hardware and the
open homebrew tools. The player stays the main goal ([§1](01-main-goal.md)); this section is what
applies to every job under it.

**Merge rules, adopted as project rules.** Existing rules stay; a rule that
already exists keeps its wording; a conflict keeps the existing rule and is
reported to the owner for a decision, except where the new rule protects the
owner's files and data, the legal rules, or the stability of the build. Priority
order, highest first: (a) never delete or damage the owner's files, config, logs
or data; (b) legal and clean-room rules; (c) keep the build and the console app
stable; (d) hardware limits and the frame budget; (e) visual and feature
quality; (f) everything else. New rules go at the end of their section; sections
stay labelled and numbered.

**Startup checklist, before any job.** Read this manifest and `AGENTS.md`; state
the current version, what it contains and which parts have actually run on the
console; list the protected files and everything the job could touch; list the
reference sources relevant to it; write or update the plan; list what is unknown
or cannot be verified and ask for missing definitions. Never guess a plan, rule
or hardware fact.

**Plan before building.** Every job gets a short plan in this file before code:
Goal (plain words, and what done looks like), Platform facts (firmware,
libraries, memory, controller, display, storage paths), Architecture (modules,
data flow, which thread does what), key algorithms and data structures, the
functions and system APIs with the source that documents each, the file map,
the budget in real console numbers, the risks and how each is tested on the
console, and the test plan for the owner. Small changes get a short plan; a new
program gets the full plan and the owner's approval before it is built.

**Reference sources, in this order, never guessed.** OpenOrbis toolchain (docs,
samples, `include/orbis` headers) → ps4libdoc → PS4 Developer Wiki → LibOrbisPkg
(package and param inspection) → the ConsoleMods OpenOrbis SDK page → open-source
PS4 homebrew using the same library. Graphics technique reference, for ideas and
formulas only, never pasted code: tinyrenderer, GPU Gems 1–3 (glow, water,
volumetric light, shadows, bloom, depth of field), Inigo Quilez's articles,
GDC talks and postmortems, Shadertoy for inspiration only. General algorithms
and formats: standard textbooks and the format's own specification. Where a
licence is unclear, learn the idea and write the code yourself. One unverified
lead is recorded as such: `uomar8/ps4-dev-knowledge-base` — it could not be
opened when checked, so it is a lead to verify, not a source.

**Protected files — additions to the [§7](07-environment.md) list in `AGENTS.md`:** `library.idx`,
`THIRD_PARTY.md`, `LICENSE`, the packaging inputs `sce_sys/` (including
`param.sfo` and `icon0.png`) and `sce_module/`, and anything referenced by the
Makefile, the source, the README, the scripts or the tests. Keep a copy of
`param.sfo` before regenerating packaging files. Never delete a file this
session did not create; never delete anything without showing the exact list
first; prefer moving to `files/archive/`; when unsure, keep it.

**Working like a console developer, for every graphics or performance task:**
name the look in one sentence and which shipped game or app does it well; find
the technique in the sources above and name where it was found; budget it in
real console numbers and say which quality step it belongs to; fake it cheaply
(baked layers, lookup tables, sprites, low-resolution buffers for soft effects,
slower updates for slow things, screen-space post effects); write original code;
prove it with before-and-after renders and frame times at the step the console
actually runs; document it here and in `config.ini`. Techniques expected at this
standard: low-resolution bloom and glow, layered fog and light shafts, soft and
contact shadows, water (depth gradient, Fresnel, refraction offset, caustics,
foam, glints), glossy-floor reflections, lookup tables, sprites and impostors,
budgeted particles, color grading, dithering and grain, level of detail,
temporal reuse, procedural noise, eased curves. Results are judged on a clear
focal point, depth in layers, a cohesive palette with one or two strong lights,
motion tied to the audio, and no banding, shimmer or hard stripes where a soft
gradient belongs.

**Honesty and testing, unchanged and restated:** say what ran only on the host
preview and what ran on the console; measure before and after at all three
quality steps, three runs each; nothing more than ~20% slower at the default
step without a stated reason; every tweakable in `config.ini` with a comment and
a default; one config file and one log file per program with levels, tags,
uptime timestamps, run separators, a rotation cap and a summary at exit; a
screenshot button saving a BMP into a capped folder so the television never has
to be photographed; long-run crash and memory-growth checks; problems reported
including what could not be verified.

**Legal, unchanged and absolute:** public documentation, open samples and
original code only; no leaked SDK material, no leaked source, no code copied
from a reference; third-party licences in `THIRD_PARTY.md` for anything actually
shipped; respect other people's data and accounts; no features that bypass
protections on content or services the owner does not own; no secrets on the
console or in the workspace.

**Future-proofing:** keep the resource index ([§8](08-resources.md) plus [§9](09-ps4-knowledge-base.md)'s references), the known
limits ([§7](07-environment.md) and [§9](09-ps4-knowledge-base.md)), the open questions ([§10](10-open-questions.md)) and the decisions log ([§5](05-decisions.md)) current,
with the version that used each source; ask for any definition that is missing
rather than inventing it; check a library's or tool's current documentation and
record the version relied on, because toolchains and firmware move.

### Conflicts found while merging, existing rule kept

| The merged document says | This project already has | Kept |
|---|---|---|
| Software rendering is the proven graphics path; the toolchain's GL renderer produced broken output elsewhere, so treat GPU as an experiment never assumed | The Piglet sample has since run on **this** console: modules load, EGL 1.4, `GL_RENDERER: Piglet`, 119.9 fps on a quad, half-float targets COMPLETE | Ours. GPU is an experiment that has already passed its first test here (AGENTS.md, [§9](09-ps4-knowledge-base.md) R1–R12). The other port's broken GL renderer is recorded as a different stack's result, not as evidence about Piglet. |
| Every visualizer pays a flush cost of roughly 22–27 ms while video pays under 1 ms for the same step | Measured: the floor is 21.6–24.5 ms with video under the canvas raising it to 29–37 ms; video drawn with no canvas costs 1.4 ms | Ours, by measurement; the document's range is inside our floor and its video figure matches only the no-canvas case. |
| The log's first-texture format reads ARGB8888 while the display reads ABGR8888 — a possible per-pixel conversion cost, unconfirmed | Measured: the window surface is created as 24-bit `BGR888` and presenting memsets 8.3 MB, copies at 4 B/px, then waits for the flip | Ours. The ARGB/ABGR swap is recorded as an **unconfirmed lead**; nothing is built on it until a log shows the swap. |
| No zips unless asked | One package per build, plus its MD5, no zips ever | Ours — stricter, so the stricter wording stands. |
| New programs get the full plan and are approved before being built | Rounds in [§11](11-next-steps.md) are authorized in advance; proof-of-concept work proceeds without a fresh start | Both stand: authority is granted per round in [§11](11-next-steps.md), and a program not named there still needs its plan approved first. |

### What a console developer would expect that this document does not yet cover

1. **A memory budget per job**, not just a pool size: how much of the 4.6 GB direct
   pool a look, a decoder and a UI may each hold, and what happens on exhaustion.
2. **A crash and hang runbook**: what to check first, which log tail matters,
   how to get back to a known-good package, and how to tell a driver fault from
   ours.
3. **A performance regression gate**: a fixed benchmark, run per round, whose
   numbers are compared rather than re-derived (the drift-control rule exists
   but there is no fixed suite).
4. **An input latency target** and how it is measured, since frame time alone
   does not say how the pad feels.
5. **Thermals and sustained clocks**: whether a long visualizer run drops clocks
   on this console, which would change every steady-state number.
6. **A save/config migration rule**: what happens to an owner's `config.ini` when
   keys are renamed or removed, so settings never silently reset.
7. **A clean-room note format**: for each borrowed idea, one line naming the
   reference and what was written independently — the legal rule asks for it but
   nothing records it.
8. **A definition of "professional"** measurable on this hardware: a frame-time
   ceiling per quality step, a resolution floor, and a list of looks that must
   hold it.

### The gaps, as rules G1–G10

The ten rules that came out of the gap list above are written **in full in
`AGENTS.md`** under "Rules G1–G10", because that is the file every job is run by and
rules belong where they are obeyed. One line each: G1 memory budget per subsystem,
G2 crash and hang runbook, G3 fixed benchmark, G4 input latency, G5 thermals, G6
`config.ini` migration, G7 clean-room note, G8 "professional" defined, G9 size
ceilings, G10 a new project gets its own file.

[Manifest index](README.md) · [Back to the index](../README.md)
