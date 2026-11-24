# 3. Roadmap

*Part 3 of 12 of the manifest — previous: [Current focus](02-current-focus.md) · next: [Build log](04-build-log.md)*

Statuses: done, in progress, blocked, not started.

**To a finished app**

| # | Step | Status |
|---|------|--------|
| 1 | Confirm the environment (console, firmware, toolchain, data path) | done |
| 2 | Install and test the OpenOrbis toolchain | done |
| 3 | Hello-world PKG that installs and runs | done |
| 4 | Window, input, and a real UI on the console | done |
| 5 | File browsing, drives, media scan | done |
| 6 | Music playback (MP3, EQ, spectrum, visualizers) | done |
| 7 | Video playback (`libSceAvPlayer`) and subtitles | done |
| 8 | The player interface: home, music, videos, pictures, settings, EQ | done |
| 9 | Test with many file types and long sessions | in progress |
| 10 | Package each build and hand it over with an MD5 | done (v116) |
| 11 | Install and usage notes | done (`README.md`) |
| 12 | Keep the PS4 knowledge base current for future apps | in progress |

**Engine and performance work** (detail in `AGENTS.md` §Plan)

| Item | Status |
|------|--------|
| Round 2 — flush fix, ladder judges the whole frame, Stage B machinery | done (v112) |
| Round 3 — Liquid look: water body, depth, framing | done (v113) |
| Round 3 — Liquid look: glass, floor, spray, crest glint | done (v114) |
| Round 4 — the 467 ms Square freeze (sliced background bake) | **done, confirmed on hardware** (v116 run, 2026-11-23: worst paint 18.5 ms on 42 presses) |
| Round 5 — glint brightness, spray clustered on crests, floor lifted | built and running on the console (v116); **look not yet judged on the television** |
| A3 — stop copying the whole canvas and re-uploading it every frame | not started |
| A4 — dirty regions for layers that change in part of the frame | not started |
| Stage C — update slow layers less often | not started |
| Stage D — cheap tricks that look expensive | not started |
| Stage E — spend freed time at the step the console actually runs | not started |

**Graphics standard work** (added 2026-11-23, [§1](01-main-goal.md)). Eighteen looks exist:
SPECTRUM, WAVEFORM, RADIAL, PULSE, MIRROR, BLOCKS, SCOPE, ORBIT, LIQUID, NEON,
FLAG, DANCE, WORM, WINDMILL, DANCE 2, DANCE 3, RAVE DJ, TESSERACT — all drawn by
our own software rasterizer into an SDL surface.

| Step | Status |
|------|--------|
| Audit all eighteen looks against the standard; write down where each falls short | **done 2026-11-23** (host timings for all 18, ranked gaps, two-target plan — `files/round7/`) |
| Research PS4 GPU capability per topic, five phrasings each; findings and disagreements into [§9](09-ps4-knowledge-base.md) | not started |
| Decide the renderer: software canvas, GNM, or a hybrid — logged in [§5](05-decisions.md) | **decided 2026-11-23**: Piglet ES2 on the GPU for the visuals, SDL keeps the chrome and the flip ([§5](05-decisions.md)); GNM, OpenGNM and the experimental stacks stay untested alternatives |
| Prove one GPU path on the console: one look, one effect (bloom is the candidate) | **done 2026-11-23** — Piglet/GLES2 runs on 12.52: EGL 1.4, `GL_RENDERER: Piglet`, ~120 fps on the sample's textured quad |
| Proof of concept: one look on the GPU (LIQUID is the candidate), half-float render targets, a real bloom pass, in its own package | next, after the video-logging round and the owner's go-ahead ([§11](11-next-steps.md)) |
| Move the biggest-gap looks onto it, in the order the audit sets | not started |
| Reconcile Liquid's step 2 at 15 fps with the standard | not started (finding logged in [§6](06-problems-and-fixes.md)) |
| Header: now-playing line must not run under the emblem | **done in v117** (text fitted to the emblem's edge, ends in `...`); console look unconfirmed |
| Video playback: why 16 fps, and what a pull failure is | **investigated 2026-11-23, no source change** — cause points at the present path; metadata logging is the next step ([§11](11-next-steps.md)) |
| Re-read the trade-off register every round and revisit what it lists | in progress |
| **Verify one GPU path on the console, smallest working sample first** — piglet/GLES2 (shipped with the toolchain), then OpenGNM, then the experimental stacks; every result logged in [§6](06-problems-and-fixes.md) including failures | **done 2026-11-23, piglet/GLES2 confirmed on the first path tried** — both system modules `module_start() = 0`, EGL 1.4, `GL_RENDERER: Piglet`, 3,300 frames presented ([§4](04-build-log.md), [§9](09-ps4-knowledge-base.md)). OpenGNM, PS4GL_GNM and ps4-kit are not needed and stay untested |
| Port or reimplement one MilkDrop-style warp-feedback effect once a GPU path is proven | **unblocked 2026-11-23**, and it belongs in the proof of concept: framebuffer fetch is absent, so it is ping-pong FBOs. Licence check first for projectM presets; from-scratch stays the default || v118 — logging only: video codec/fourcc/rate/duration/bitrate/resolution at open, `pull_rounds` in the summary, the raw bytes and glyph check for a `??????` name, open/teardown stage timings | **built 2026-11-23** (`349943cbd65ddb1580a9e47aec7de1b0`), awaiting the console run ([§11](11-next-steps.md) Round 1) |
| GPU proof of concept — one look with the biggest visible gain, half-float bloom chain, per-band audio analysis, own package at 1080p | after v118 ([§11](11-next-steps.md) Round 2) |
| Round 3 — plans only: SDL/Piglet handover, and a 60 fps budget for all eighteen looks | after Round 2 is logged ([§11](11-next-steps.md) Round 3) |

**Optional feature candidates** (ideas from studying other PS4 players, [§9](09-ps4-knowledge-base.md); ranked
later, not committed): resume position / continue watching; favorites and last
played; playlists by media type; external subtitle files with proper font
coverage; multiple audio and subtitle tracks; network streaming over HTTP or
FTP; cover art; safe USB-removal handling; a full settings screen. They wait
behind the graphics work — the goal is quality first.

[Manifest index](README.md) · [Back to the index](../README.md)
