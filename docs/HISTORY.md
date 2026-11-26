# Build history

The dated log: what was built, what ran on the console,
what the log said. Read it when you need to know *why* a rule exists.

[Back to the index](README.md).

---

# PS4 media player — build history

Extracted verbatim from `MANIFEST.md` §4 on 2026-11-24, when that section had grown
to 827 lines and was crowding out the rules and the current state. This file is the
record: it grows in date order, oldest first. Nothing here was rewritten. The dated
index stays behind in `MANIFEST.md` §4.

Append-only, newest last. **2026-11-24 and earlier live in
[`APPENDIX-HISTORY-2026-11-24.md`](../APPENDIX-HISTORY-2026-11-24.md)**, which
itself hands off to `APPENDIX-HISTORY-2026-11-23.md` (one directory up, beside this file). Each day moved out when this file approached its
96 KB budget, which is what the budget is for. Older rounds are summarized in `files/round2/README.md` and
`files/round3/README.md`.

### 2026-11-25 — the v138 log answers the frame question (v139)

- Run: `app.log` 2026-11-25 23:09:05, build `v1.38-fd440873e8a6`, base budgets,
  home screen only, about 600 frames at 150 ms. Last line in the file:
  `joy: button 0 -> key 120` — no frame after it, so the run ends on that press.
- `probe: neo mode 0, cpu 2100000000 MHz`. The call returns Hz and the log
  printed it as MHz; `tier = neo ? 2 : 1` then gave a 2100 MHz machine the base
  tier. Fixed: the number is converted, and a Pro-class clock takes tier 2 with
  a line that says why (base is 1600 MHz); the ladder can still step down.
- `flush=133.6 ms/frame`, `paint=1.7`, `rest=0`, `over=100%`, and not one
  `quality step` line in 87 s. The ladder lived inside the look's draw call and
  only ran when `t0 > 0`, so a frame that drew the room as a backdrop was never
  judged. Fixed: `art_ladder_tick()` is called from `vis_art_frame_ms()`, which
  main feeds the whole frame to, every frame. Home should drop to step 2.
- Panels: `slab_fall()` laid its light down in four-pixel bands, so the top of
  every door read as a grey plate the glyph was sitting on. Now one row per
  step on a curve, light at the lip only, and the plate is gone on the host
  render. Host home frame 9.9 ms worst, unchanged.
- Package: v139, 6,619,136 bytes, MD5 `18b9bf21f980490125da3bfafa8fc850`,
  build `v1.39-e0f24d4c2cd3`, 17/17 package checks. **Not run.**

### 2026-11-25 — the ledger, and what the log did not say (v140)

- New rule **G13**: every attempt is written into `docs/LEDGER.md` the turn it
  happens, with a one-word verdict, its evidence and the documents it amended. The
  ledger now carries sixteen rows, GPU probes included. `AGENTS.md` paid for the
  four new lines by compressing two bullets that restated the manifest — same
  facts, fewer words, still 638 lines.
- Asked whether the v138 log says anything about the PS button's quick menu or
  about music playing in the background. It does not, and the reason is now
  written down in `docs/SYSTEM-INTEGRATION.md`: the state is polled every six
  frames for a GNM keep-alive and never logged, and there is no background audio
  path in the code at all — one audio output, opened at boot, suspended with the
  process.
- v140 adds the missing record: `system: ui overlaid N, in background N` once per
  change, polled whether or not a GNM session exists, plus
  `system: status unavailable rc=…` when the console will not answer. Package
  6,619,136 bytes, MD5 `a46d07a233dafa0b347e6258f8c86925`, build
  `v1.40-21d49ce667c4`.
- v139 was built, never run, and removed from `files/` when v140 replaced it
  instead of being kept beside it. Packages are not reproducible, so those bytes
  are gone; the row says so. The rule that was broken is "keep every version".

### 2026-11-25 — does the PS5 help the PS4 build?

- Asked and answered in `docs/PS5-AND-FORWARD.md`, sources dated: the PS5 keeps a
  hypervisor above the kernel, so homebrew enabling is weaker there; a PS4 fpkg can
  run on a PS5 in backward compatibility; the class that runs is CPU-drawn, because
  a PS5 compatibility layer has no `libSceGnmDriver`; and the SHARE menu cannot be
  displayed during PS4 gameplay on a PS5. The owner's console-side counterpart is
  the Create button.
- What it changed: nothing in the build. PS5-only APIs (AGC, Tempest, ray tracing,
  the SSD decompressor) cannot be called from a PS4 eboot and a PS5 build would be
  its own project under G10. The machine is already measured at run time, which is
  what makes a "PS5 tier" unnecessary and wrong.
- What it named: the video path is the one thing that would break on a compatibility
  layer without the GNM driver - the UI and the visualizers would live and video
  would not - and the boot line `gnm driver rc=0x…` already reports it. SHARE is
  unmapped and no prompt row uses it, checked in `src/ui.c`.
- One wording change queued, not built: the probe prints `tier 2 (PS4 Pro)`, which
  would be false on anything that is not a PS4. It waits for the next build rather
  than adding a third unrun package to the archive.

### 2026-11-25 — a re-evaluation of the whole build (v141)

- Read it end to end against what the v138 log proved, looking for anything that
  would make the *next* run more informative. Found three things, changed all
  three, and left four alone with reasons.
- Changed: the tier line printed `(PS4 Pro)` from a clock reading, which is a
  model it cannot know — it says `Pro-class clock` now. Every screen change writes
  `screen: <name>` after the pad is read and before the new screen is drawn, which
  is the stage marker G2 asks for and the thing the v138 file could not tell us.
  And the overlay state is sampled every frame instead of every sixth, because at
  150 ms a frame a quick menu opened and closed inside a second would never have
  appeared; the GNM keep-alive still only fires every sixth.
- Left alone, with reasons: the GNM handle `0x2B` is treated as loaded and cannot
  be confirmed from this side — the boot line reports it, and a guess is worse than
  the log; the dead `s_drew` flag and the now-unused `t0` parameter are cosmetic
  and not worth invalidating a tested build; the ladder's budgets are untouched
  because no measurement exists to justify moving them; a CPU-only video path is a
  research question, not an improvement, and G12 says name it before building it.
- Package: v141, 6,619,136 bytes, MD5 `166dde14d6e5fdeaf7f6b661859c909b`, build
  `v1.41-1dc30a76b016`, 17/17 package checks. v140 stays in the archive unrun;
  v138 stays as the rollback. **Not run.**

### 2026-11-25 — the sweep, and the visualizer nobody could see (v142)

- Ran the whole app internally: every screen at 1080p and eight at 720p, all
  eighteen looks at all three steps, the audio and subtitle host tests, and the
  gates. 79/79 screens and 54/54 look renders passed once the harness itself was
  fixed (`one` writes its file into its own directory; looks only draw in
  `ui_draw_visualizer_full`, mode 13 - mode 12 is the music screen).
- Then the result that matters: the look box was **empty**, 0 bright pixels of
  918,400, plate color showing through. `s_tex` in `src/vis_art.c` is declared,
  used in `art_present()` and created nowhere, so `SDL_RenderCopy` was handed
  NULL every frame and the return was ignored. Every look has been painted into
  a canvas and thrown away since this present path replaced the old one. Every
  host render of a look was really the music screen, which is why nothing caught
  it: the box is dark, the background is dark, and the timings measured drawing,
  which does happen.
- Fixed in v142 by creating the texture at the canvas size, uploading with
  `SDL_UpdateTexture` and only then blitting. Proved on the host: 53,926 bright
  pixels at step 1, 62,741 at step 3, all eighteen looks distinct across steps.
  The bench was re-measured with the upload inside the frame (28.8-49.5 ms; SCOPE
  is closest to the 50 ms line at 49.47).
- Withdrawn: `AGENTS.md` open problem 10 claimed the step-3 look was known from
  host renders. It was not — those renders held no look. The claim is replaced
  with the finding and the fix.
- Package: v142, 6,619,136 bytes, MD5 `c9a4cee1029c0549585cc48cecc17225`, build
  `v1.42-b7a032d6fcd6`, 17/17 package checks. **Not run.**

### 2026-11-25 — the toolchain end to end (v143)

- Ran the whole chain on a cleaned tree: compiler, linker, `create-fself`, SFO,
  gp4, `pkg_build`, the package gate, the host preview and the host tests. A
  clean build is 17.1 seconds and passes 17/17; the modules in the package are
  byte-identical to the toolchain's; the SFO's `APP_VER` and `CONTENT_ID` match
  the Makefile and the gp4; the icon is 512x512.
- Made the gates fail on purpose: a touched source (16/17), a truncated eboot
  (15/17), a missing icon (16/17). Each caught, each named, none silent.
- **Found a build-chain bug:** `make VIS_PLATE_FX=0` built **with** the effects,
  because make treats the string `0` as true, and the flag signature stored the
  derived flag so neither value forced a rebuild. Only `1` turns it on now and
  the signature stores the value; verified both directions from clean (OUT,
  3,058,720 bytes; IN, 3,058,736).
- Recorded two limits instead of papering over them: packages are not
  reproducible (clock stamps), and the build id names the source, not the flags.
- Recorded one dead dependency: the eboot imports Piglet and PrecompiledShaders
  for GPU code that never runs without the takeover flag.
- Doc: `docs/TOOLCHAIN-E2E-2026-11-25.md`. Package v143, MD5
  `908cfa963bc252d7865ba96d933c1d2a`, build `v1.43-4958356e737f`. **Not run.**

### 2026-11-25 — compatibility layers, researched then built (v144)

- Research: the model axis does not exist (one firmware image for launch/Slim/
  Pro; ps4libdoc is firmware-indexed only), AvPlayer's 26 exports carry
  byte-identical NIDs on 5.05, 6.72 and 9.00, and the kernel's "Call to
  unpatched function is detected" is an unresolved import with three causes -
  wrong module/library pair in the stub, an export absent on that firmware, or a
  module that was never loaded.
- Built `src/compat.c`: at boot, load AvPlayer by sandbox path keeping the
  handle, resolve `sceAvPlayerGetVideoDataEx`/`GetVideoData` by name through
  `sceKernelDlsym`, ask for audio out / pad / user service / net ctl by internal
  id, and log every answer as a `compat:` line. The video path uses a resolved
  pointer when there is one and the linked symbol when there is not.
- Recorded what no layer can do: a statically imported module that the loader
  cannot resolve kills the process before `main`, and a stub with the wrong
  module/library pair is toolchain data, not something runtime code repairs.
- Doc: `docs/COMPATIBILITY-LAYERS.md`. Package v144, 6,619,136 bytes, MD5
  `2c7a3a6f070c53d9ce47555ac2670795`, build `v1.44-8c6bccde0f11`. **Not run.**

### 2026-11-25 — six research passes, and the manifest corrected (no build)

- Set six independent research briefs against the manifest's load-bearing claims:
  the SDL-PS4 present path, the console's clocks, frame pacing and memory
  bandwidth, GnmDriver and H.264 decoding, GoldHEN and firmware support, and PS5
  backward compatibility. Ten claims stood; seven were wrong, and two of those
  were wrong *explanations* of measurements that are still true.
- **A6:** `CLOCK_MONOTONIC` works on PS4 (libkernel, id 4; Sony's own shell uses
  it). Our pre-v111 `0.000` timestamps were an unchecked return over a zeroed
  `timespec`. The PSP/Vita `sceKernelGetSystemTimeWide` had been recorded as a
  PS4 fallback and is not a PS4 export.
- **A7:** the 21.6-24.5 ms `flush` floor is a **vblank wait** in the present path,
  not the 8.3 MB canvas copy - that copy is ~1 ms by arithmetic, and costing 22 ms
  would mean 377 MB/s. Consequently "no screen can exceed 30 fps" was wrong as a
  cause: the ceiling is 60 fps and the floor about 1.4 vsyncs.
- Also corrected: the "4x slower than the host" figure was never measured (5-13x
  per core is what other ports report); "the kernel and GoldHEN" report unresolved
  imports, when the message is the loader's; GnmDriver is not "never loaded for
  free", it is loaded for an eboot that imports it and only needs an explicit load
  for a module loaded later, which is our AvPlayer case; and the AnyPS5 #3094
  evidence had been read as a statement about PS5 backward compatibility when it
  is a PC reimplementation with no PS4 hardware.
- New: the SDL-PS4 row copy moves 4 bytes a pixel from a 3-byte surface, the flip
  wait is an equeue wait (`sceVideoOutWaitVblank` is unused), flip rate cannot
  exceed 60 Hz, `libSceVdecSavc` really does import GnmDriver, 12.50/12.52 are
  reported unstable by the community, and bypassing SDL video for own VideoOut
  buffers is the documented way out of the present floor if it is ever needed.
- No package this round: nothing in the binary changed. `AGENTS.md` 638 lines,
  `make check` green (28 manifest checks, 47 rules, 27 pages). v144 stays latest.

### 2026-11-25 — the fact-check batch becomes a rule (no build)

- The six-brief research batch that corrected seven manifest claims is now part of
  the project rather than a chat message: **G14** in `AGENTS.md`, its full text in
  `docs/OPERATING-RULES.md`, and the method, brief template, output contract and
  the 2026-11-25 result in `docs/AGENT-PRACTICE.md`.
- To stay under the 640-line ceiling, A4's body (superseded by A5 since the GPU
  probe) moved to `docs/AMENDMENTS-ARCHIVE.md` with a pointer left in its place -
  the first use of G9's own instruction to move rather than delete.
- No package: documentation only. v144 remains latest.

### 2026-11-25 — round two: the GPU question closed

- Six more briefs on what round one left open. The headline is a "no": **a
  Piglet/GL surface cannot share the screen with SDL's software window.** There is
  one scan-out; SDL already owns a VideoOut handle, mesa-ps4 says a second owner
  is not a thing, and no project anywhere mixes the two. That kills the 2026-11-23
  renderer decision ("Piglet for visuals, SDL keeps the chrome and the flip") as
  written: text and chrome would have to move into GL with the visuals, or neither
  moves. `MANIFEST.md` §5 and the two open questions in §10 are amended.
- **A7 corrected again, this time against a measurement.** An 8.3 MB frame copy on
  PS4 is 2.3 ms (~3.5 GB/s), not the ~1 ms I had computed; so the 22 ms present is
  about 5 ms of copy plus memset and 17 ms of vblank wait. My arithmetic was ~2x
  optimistic and is gone from the rule.
- The PacBrew SDL fork was priced and rejected: it is EGL/Piglet only, has no
  framebuffer path, and needs the runtime shader compiler module retail firmware
  does not ship. Under the no-Sony-SDK rule it is not adoptable.
- AvPlayer's silent "audio but no frames" now has a mechanism: the pipeline is
  audio-clocked and frames flow only while audio is pulled at real-time pace; plus
  `AddSource(url)` needs `fileReplacement` callbacks and `autoStart=1` never sends
  READY. Software H.264 is also shown viable (720p60 measured at 62 fps on a Pro).
- `APP_VER` padding: the convention is `01.44`, our package says `1.44` and
  installs. Changing it now risks the installer reading the new string as older,
  so it stays until a console test can decide it. Recorded as a decision, not an
  oversight.
- No package: documentation only. v144 remains latest.

### 2026-11-25 — round three, and the research set closes

- Present without waiting: **possible and bounded**. The console's flip queue is
  about sixteen flips deep; mesa-ps4 waits only when the producer runs ahead, and
  pays for skipping it with tearing when a cheap frame laps the display and up to
  sixteen frames of latency. Nothing presents above 60 fps on screen - the flip
  rate is 60/30/20 only - and OrbisDesk measured 115-140 presents a second
  delivering about 57. Our 17 ms wait is therefore a choice, not a law, but the
  trade is latency and tearing, not free frames.
- **A PS4 Pro runs at base clocks unless Boost Mode is on** (1.6 GHz, cut memory
  bandwidth), so a Pro without it measures like a Slim. The trip sheet now asks
  which way it is set before the first run.
- Our video path's real matrix: **MP4/H.264/AAC through AvPlayer**; MKV, TS, VP9
  and AV1 are not supported and HEVC is unverified on hardware. Sony's Media
  Player app supports far more, but through a different pipeline - the two must
  not be conflated in the documentation.
- Firmware 11.00 removed `_Unwind_RaiseException`; audited our own binary and it
  imports **zero** unwinder, `__cxa_*` or mangled C++ symbols, so the hazard
  cannot reach this app. Recorded with the command that proves it.
- With that, the publicly answerable set is empty. What remains needs the console:
  frame times at the real ceiling, thermals over five minutes, input latency, the
  first sight of a look on a television, and the `compat:` block.

### 2026-11-26 — the capability map, and what the toolchain cannot do

- Six briefs plus a read of the installed toolchain, organised as known knowns, known
  unknowns, unknown knowns and unknown unknowns. The answer to "can we build anything" is
  no, but the envelope is wide and now written down: 2D software and flips, GLES2 with
  **precompiled shader binaries** (two projects prove it with Sony's shader modules
  deleted), raw GNM draws, PCM audio on several ports, full pad input, `/data` I/O,
  sockets and HTTP, AVC 1080p decode. Not available: PSN, trophies, system music, VR, a
  second graphical process, compute on silicon, 4K from homebrew.
- Three findings touch our own build. **`CATEGORY gde`**, the only category Piglet gives a
  compositor to, caps direct memory at **768 MiB** against our 4,608 MB - so the GL route
  would cost six times the memory as well as the exclusivity we already knew about.
  **`ATTRIBUTE = 2`** never claimed bit 16, "CPU mode (7 CPU)", so we may be leaving a core
  on the table. And toolchain issue #290 says `thread_local` reads zero in a `-pie`
  eboot - our source has none, so the bug cannot reach us, but it rules out TLS for future
  per-thread scratch.
- The inventory itself is a fact now: 164 headers, 422 link stubs, and the typed/untyped
  split per module - `MsgDialog` 8/0, `AvPlayer` 0/24, `VideoOut` 10/117. Every module we
  adopt costs a hand-written header and one hardware run.
- Then a checklist of seventeen items and seven instruments: four to make the toolchain
  trustworthy, six to prove untouched surfaces, seven that only the console can answer,
  and the probes that surface the questions we have not thought to ask. G15 makes that the
  standing method for any new kind of job.

### 2026-11-26 — round two: nine capability questions answered, four claims corrected

- **The pool is identity, not category.** nx-on-orbis signed one ELF three ways and got the
  big-app pool, a 768 MiB pool, and a layout that would not start; `PAID` + authinfo +
  `sce_module` decides. Plain `gd` gives ~4.4 GiB direct but only **~384 MiB flexible**;
  the Piglet identity gives 768 MiB direct and ~255 MiB flexible. Every published number is
  from a Pro - no base-PS4 pool measurement exists. Amendment A8.
- **`gde` alone does not open a GL context**: Conker ships `ATTRIBUTE 0x20814016` and
  `ATTRIBUTE2 0x6` because the sample's attribute set never returns an EGL display.
- **Two port-derived claims lost to our own console.** The sample run here on 2026-11-23
  enumerated half-float render targets, float textures, derivatives, shader LOD and timer
  query as present, and loaded `libScePrecompiledShaders.sprx` with `libSceShaccVSH.sprx`
  never requested. So "HDR is dead on Piglet" and "a devkit shader module is required" are
  both wrong *for this machine*, and the second one means the shader checklist item is a
  decision, not a project.
- **The 7th-core bit is unsupported, and the evidence points the wrong way.** Conker logs
  seven cores on `gd` with `ATTRIBUTE 2`. The probe is now "count the cores".
- **NID drift is closed in our favour** (A9): a NID is a truncated SHA-1 of the export name
  plus a fixed salt, so names keep their addresses on every firmware.
- **TLS is not one flag away**: #290's reporter found the CRT never calls
  `__init_tp`/`__copy_tls`, and no shipping project passes `-ftls-model`.
- **Emulators are a fifth source class** for struct field order, with `static_assert` on
  size and offset as the price. The Sources rule now says so.
- **Pad init has a working ladder** (A10); the dialog result bug has no fix, so own dialog.
- **Compute, HEVC and 2160p**: no public on-console attempt as of today.

### 2026-11-26 — the lie audit: six claims that did not survive

- Six agents were told to **falsify** the claims this project leans on, and our own numbers
  were re-derived arithmetically. Six failed, and two were fabricated attributions.
- **The 2.3 ms frame-copy figure is not a PS4 measurement.** The only 2.3 ms / ~3 GB/s
  1080p upload in the public record is a 2010 Windows 7 desktop thread about
  `glTexSubImage2D`. Withdrawn from §9 and from this file. What replaces it is ours: the
  21.6-24.5 ms floor is 1.30-1.47 vsyncs, so copy plus memset costs 4.9-7.8 ms, implying
  2.1-3.4 GB/s - inference, labelled as inference.
- **The "whole multiples of 16.7 ms" rule is false**, by our own log: 21.6 ms is 1.30
  vsyncs. The model that fits all five measurements is **one vblank plus work rounded up to
  whole vblanks**. A11b.
- **opengnm's numbers describe Sony's firmware**, not the library: ~122 of 207 exports are
  stubs, ~85 real, 74 used by the backend, and the "600 frames at 60 fps" is a vsync-paced
  soak with no frame-time data. The project's only measured FPS are 3.15-5. A12.
- **"AvPlayer plays MP4 only" is Apple's AVPlayer story**, which shares the name; `FILE` and
  `HLS` are real, the container list has no source, and no project has reported a container
  rejection. A13.
- **PS4 Cast is retracted**: AI-authored, its own README says the author cannot test on a
  console, and its logs record ~40% drops. Moonlight survives as a documented default, not
  a benchmark; Ship of Harkinian survives cleanly. A14.
- **The Conker attribute recipe was read from a branch Conker does not ship** - its package
  is plain `gd` with `ATTRIBUTE 2`. What survives is nx-on-orbis's three-layout measurement
  and SoH's 768 MiB / ~255 MiB. A15.
- **ioQuake3's 124 blobs are real; the rest of that claim is prose**, and per-stage capture
  contradicts the export's own `program` naming. A16.

### 2026-11-26 — correction to the correction

- The lie audit's first round called the **2.3 ms frame-copy figure fabricated** because six
  searches found nothing. The second round, asked to verify that correction, found it in
  **mesa-ps4's `wsi_orbis.c`**: "the copy costs 2.3 ms of a 21.9 ms frame - over a tenth of
  the frame to move 8100 KiB". The citation stands; only our vague "a PS4 port" was wrong,
  and it is now named.
- The timing model also took two attempts. Round two proposed `ceil(work/vsync)` with no
  extra vblank, which cannot produce our 21.6 ms floor - and that round's own 21.9 ms quote
  refutes it too. The model that fits every data point on both sides is **one vblank plus
  work**, and that is what stands.
- **A14 gets stronger, not weaker.** PS4 Cast is still retracted as evidence, but the measured
  numbers exist elsewhere: 58.5-59.1 presented fps at 1080p60 on an original PS4, decode
  3.98-5.6 ms, conversion 1.9 ms, 0.5-1.3% drops. The decoder is not the bottleneck.
- **A16 resolved**: shader capture is per **stage**, confirmed by where the call sits in the
  code and by `glGetProgramBinaryOES` being NULL on this firmware. The system shader table
  ships only Sony's UI and 2D transform shaders, so it cannot light a 3D scene.
- Method note added to the register: a failed search is a null result, never a verdict.

### 2026-11-26 — what the shipped player already proves

- The owner's read: we built a media player that plays video better than the other
  homebrew, and that answers several of the questions this research kept circling. Taken
  as owner-stated and untested, with the boundary written down so it cannot be stretched
  later.
- **What only a shipped app proves, and ours has:** a package that installs and runs on
  12.52 with GoldHEN, `/data` I/O and the log sink, audio out at 48 kHz through the whole
  chain (decode, resample, EQ, limiter), AvPlayer playback, a library scanned at 2,921
  tracks, module loading with the dlsym fallbacks, 144 versioned builds and 17 gates that
  fail on purpose. No amount of research substitutes for that.
- **What it does not prove, and must not be claimed to:** the frame-time ceiling, thermal
  behaviour, poll-to-photon latency, how a look reads on a television, the memory pool a
  different package identity would get, and container breadth - where the FFmpeg-based
  players still do more formats, if not better playback.
- Consequence for the checklist: the platform-surface probes (packaging, audio, files,
  module loading) drop in priority because the product already exercises them; the
  frame-time, thermal and visual questions stay at the top, because nothing but the
  console can answer them.

### 2026-11-26 — how retail packages are built, and what the best titles shipped

- **A package is three layers:** the PKG wraps an outer PFS whose only file is
  `pfs_image.dat`, and inside that PFSC container sits the inner PFS that mounts at
  `/app0`. Header 0x5A0, entry table 0x2A80, image at 0x80000, 64 KiB blocks. A measured
  24 GB AAA base carries **25 entry rows and 14 files** - size lives in a handful of giant
  archives, not in file count.
- **"Fake" is four swaps, not a format change:** header signature, the FINALIZED flag, the
  image key's encryption key, and a generated debug licence. Structurally a retail package
  and ours are the same object with a different trust root. Our build already matches
  retail practice on content-id grammar, `param.sfo` placement, the icon and `right.sprx`.
- **Two gaps, both small:** we ship no `pic1.png` startup image or `pic0.png` background,
  which the builder would promote into plaintext entries for free. `snd0.at9` needs an
  ATRAC9 encoder that does not exist openly, so home-screen music stays off the table.
- **The scale trap:** the PlayGo hash table is 4 bytes per 64 KiB, 0.0061% of the package,
  and the builder's 16-byte reserve can be crossed by a 512 KiB alignment step. At 6.6 MB
  we are 101 chunks and 404 bytes; near 8 GiB it would fire every time. The real hazard is
  stale `playgo-*` files in the tree, which we do not have.
- **The hardware ceiling, measured by the best-looking games:** 1080p30 shipped on every
  ambitious title; the only 60 fps datapoint is Uncharted 4's multiplayer dropping to
  900p. Temporal accumulation, baked GI, reduced-resolution screen-space effects and
  compute-driven culling are what bought the image. 1080p60 needs either a 720-900p
  internal render with temporal upscale, or far lighter shading than AAA.
- **Streaming practice for this hardware:** no seeks (a stock HDD's 4 KiB latency is 17.2 ms
  against 0.145 ms on SSD), 64-512 KiB reads, and decompression on the CPU - zlib on a
  Jaguar core runs at 74.75 MB/s, below the drive itself, which is why silicon Kraken had
  to wait for PS5.

### 2026-11-26 — our own package, measured; and a line drawn

- Asked to dissect a Cyberpunk 2077 package from a public collection, and declined: a retail
  game distributed without a licence is a pirated copy, and reverse-engineering its contents
  is a second problem on top of that. The container knowledge does not need it.
- So the same anatomy was read out of **our own v144 package**, which is the only kind we
  are entitled to open: magic `CNT`, `FINALIZED` clear (a fake package, as expected), 14
  entries and 6 structural rows, table at 0x2A80, body at 0x2000, image at 0x80000 with the
  header's `package_size` equal to the file size to the byte, and a 404-byte hash table over
  101 chunks.
- **That measurement corrected a public table.** psdevwiki names the dword at 0x0C
  `pkg_file_count`; ours reads 15 where the entry count is 14, and LibOrbisPkg writes a
  constant `0xF` there. A count that disagrees with the count is a constant - so `0x0C` is
  fixed and `entry_count` at 0x10 is the only count.
- The practical value is the check itself: a package that installs has `body_off +
  body_size == pfs_off` and `package_size == file size`, and a package that fails usually
  has one of the three out of step. That is now written down where the next packaging
  failure will look for it.

### 2026-11-26 — six more agents, and the docs lost an argument

- A second adversarial round, aimed at the claims that had become load-bearing rather than at
  new ground. Five of six briefs came back with a hit, and the biggest one was ours: **the
  "1080p30 shipping ceiling" was false.** WipEout Omega is a locked native 1080p60; MGSV,
  Doom, Wolfenstein, DMC5 and GT Sport are all a real 60 Hz tier. The correct statement is that
  the console has two tiers and resolution is what gets traded to reach either, with CPU-side
  simulation cost - not resolution - as the thing dynamic scaling cannot buy back.
- Two performance numbers were wrong in a way worth remembering: **74.75 MB/s of zlib decode
  is the Switch's deflate**, not a Jaguar core's (PS4: miniz 85.65, zlib-ng 109.30, zstd
  133.50), and the 17.2 ms / 0.145 ms drive pair was arithmetic, not measurement - the measured
  figures are 21.1 ms and 0.06 ms. A table row is not a platform.
- The `0x0C = 0xF` finding from yesterday is now double-sourced: a retail THIEF dump shows
  `0F` at 0x0C with **21** entries in the same header, which settles it as a constant rather
  than a count.
- New gate, `tools/check-layout.py`, in `make check`: magic, `entry_count_2 == entry_count`,
  table inside the body, and - only for packages that carry a PFS image - `body_off +
  body_size == pfs_image_offset`, `package_size == file size`, image inside the file. It skips
  PS4AL stubs and delta patches, which legitimately leave those fields at zero.
- It also cost me one wrong table: the published struct puts body geometry at 0x400, but the
  bytes put it at 0x20/0x28 with the image block at 0x410/0x418/0x428. The gate was written
  against the wrong offsets first and failed on a package that is fine - which is the reason
  to require a gate to reproduce a known-good file before trusting it.

### 2026-11-26 — the blanks, closed by arithmetic instead of by a console run

- Twelve more research briefs, run as six questions phrased two different ways each, then
  checked against our own files. Four of the project's open blanks moved, and one of them
  moved because of a sweep of **all 86 packages we have ever built**.
- **`0x0C` is not a file count.** Every one of the 86 has `0x0F` there, including a build with
  13 entries against the usual 14, and a retail THIEF dump shows `0x0F` with 21 entries. A
  field that does not move when the count moves is not a count. What it is stays ranked: most
  economically it is PS3's metadata-count slot frozen after PS4 moved metadata into the entry
  table - though `0x0C` and the DRM dword at `0x70` are both `0x0F` here, which is what the
  duplicate-theory predicts. A package with DRM other than `0x0F` would decide it, and this
  toolchain cannot build one, so it stays open with the test named.
- **The 768 MiB figure was never ours.** It is the documented default for the mini-app family
  (`gde`, `DMEM_FLAG` 0); a `gd` app has been measured at 4,608 MiB direct, which is exactly
  what our app asks for. So the old comparison was between two identity classes. And a `gde`
  route is not stuck at 768: `DMEM_FLAG` 1-4 raises it to 1024-1792 MiB, so the price of a
  Piglet-class renderer is a 1.75 GiB ceiling rather than 768 MiB.
- **The copy is 2.3 ms; the frame is the rest.** 8,294,400 bytes in 2.3 ms is 3.61 GB/s,
  matching the port's logged 3,572 MB/s to 1%, and 21.9 ms is 1.31 vsyncs. The 3.4 ms figure
  from the same file is now **contested**: an L2-eviction story fits it, but double-counts
  write traffic and treats overlapped write-back as serial; a frame-level charge including
  submit and completion fits the same gap. 2.3 ms is a floor, 3.4 is not a budget line yet.
- **The 62-82 ms question is unmeasured, not unanswerable.** The 60 Hz tier's arithmetic makes
  the pixel-bound share the whole argument: 55 ms of pixels makes 720p halve the frame, 5 ms
  makes it invisible. One boot settles it - viewport *and* scissor at 1080p/720p/360p/8x8 into
  the same target, fit the line, read the intercept. Until that runs, every 60 fps plan is an
  opinion, and this entry exists so the next one is not.

### 2026-11-26 — the manifest gets a category above it

- The spine had five unlabeled sections bolted onto its end and 30 bytes of headroom, so it
  was redistributed rather than rewritten. **A new Tier 0 page, `docs/PROJECT-MAP.md`, now
  sits above it** and defines what each layer owns and what every label prefix means, which
  gives the manifest something to refer to itself by.
- Labels, all additive: the twelve sections are **M1-M12**, the knowledge topics inside M9
  are **K1-K13**, and the five state notes that used to trail the file are **N1-N5** in
  `docs/APPENDIX-STATE.md`. They were first called A1-A5 and that was wrong - A is the
  amendment register, already at A16, so a reader would have had two different A1s.
- Every section number is unchanged, which matters: dozens of citations across `AGENTS.md`
  and the docs say "§9" or "§5", and a restructure that renumbered would have silently
  broken all of them. The labels sit after the numbers, not instead of them.
- Spine: 131,042 -> 123,0xx bytes, with 7.9 KB free for the first time in a while.

### 2026-11-26 — the new findings reach every page, and three of them get smaller

- Six more briefs, this time aimed at what had *already* been written rather than at new
  ground, then fifteen edits across five documents. Every brief found something, and three
  of the corrections made claims weaker rather than stronger - which is the point.
- **The 60 Hz paragraph had two wrong examples.** The 1,728-1,760 x 1080 dynamic window is
  Wolfenstein: *The New Order*, whose sequel runs full 1080p and does not hold 60; and GT
  Sport's "mid-40s" is beta code, with the retail game dipping to 56. And the sentence's own
  explanation was wrong: a Pro has the same CPU and 2.3x the GPU and turns GT Sport's dips
  into a lock, so what separates the tiers is per-pixel GPU budget, not CPU simulation. CPU
  cost is the separate reason a frame cannot be rescued by lowering resolution.
- **The memory figure lost its "measured" standing.** 4608 MiB is one author's probe on a
  *base* PS4 - a Pro reports ~4.4 GiB, or 5,056 MiB in NEO mode - and `DMEM_FLAG`'s 768-1792
  tiers are documented for the key with nothing showing `gde` may set it. Recorded as a class
  distinction that still holds, with its scope attached.
- **The copy cost is now a range with a caveat.** The 2.3 ms / 3,572 MB/s agreement is close
  to circular (same timer), the file disagrees with itself by 32%, and 3.6 GB/s is only
  plausible for a multi-threaded write-combined stream - a single Jaguar core should expect
  1-1.5 GB/s. Since our own present path is a single-threaded row copy, 2.3 ms is a lower
  bound for us, not a forecast; a GPU copy of the same surface is about 0.1 ms.
- **Package anatomy: three numbers were being quoted as constants.** 0x5A0 is the CNT struct
  inside a 0x1000 header; 0x2A80 is 0x2000 + 0x800 + 0x100 + 0x180; the image offset must be
  read at 0x410 because a large title's PlayGo hashes push it far past 0x80000. A no-data
  PS4AL package has no PFS layer at all, so "three layers" is not universal.
- **The map gained the three rules an outside reviewer said it lacked:** two ratings per
  statement (provenance and confidence), scope travelling with every number, and a
  two-way edge so correcting a fact forces a look at the decisions that cited it.

### 2026-11-26 - the present path gets a mechanism, and a guess gets thrown out

- `AGENTS.md` hit its wall (13 bytes, 639 of 640 lines), so its open-problems list moved to
  `docs/PLAN-AND-OPEN-PROBLEMS.md` as **P1**, keeping the original item numbers. The rules
  file is now 37.5 KB and 593 lines, with room for the first time in months.
- **A correction made an hour earlier was itself wrong.** I had written that 3.6 GB/s of
  copy could only be multi-threaded, because a single Jaguar core "should expect 1-1.5 GB/s".
  That number was a guess. Measured single-core STREAM on a 1.5 GHz Kabini part is ~3.25 GB/s
  of copied bytes, a PS4 Pro measures 4.47 GB/s, and the eight-thread ceiling is only ~6.4.
  So 3.6 GB/s is ordinary single-threaded behaviour - which removes my "lower bound for us"
  reasoning in both directions.
- **The 21.6 ms floor finally has a mechanism**, read out of the driver source: two scan-out
  buffers with a wait on every flip (so the CPU never runs during scanout), a memset of the
  whole 8.29 MB buffer that the row copy then overwrites anyway, a 3->4 byte row expansion
  from a `BGR888` surface meeting `ABGR8888` scan-out, and buffers registered at panel size
  when `SubmitFlip` takes no source rectangle - so registering at 720p would be free scaling
  and 2.25x fewer pixels. This is now M9 K14 and P1 item 2.
- **K14 also separates Piglet's limits from the console's**: 16-bit depth, one colour target,
  no `glBlitFramebuffer` and no fine-grained timers are GLES2-surface limits, while GNM has
  32-bit float depth, eight render targets and 100 MHz clock-counter events. The shader
  pipeline is the one real constraint left, and its 110-260 ms figure stays contested because
  one port reports the compiler modules do load.
- Wolfenstein now carries its year and its console, and GT Sport's 56 fps carries its build:
  a launch-week demo, not retail.
