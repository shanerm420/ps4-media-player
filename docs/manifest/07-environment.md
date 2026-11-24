# 7. Environment

*Part 7 of 12 of the manifest — previous: [Problems and fixes](06-problems-and-fixes.md) · next: [Resources](08-resources.md)*

Recorded **2026-11-23** from the owner's own answers. These are console facts as
reported, not documentation.

**Re-confirmed by the owner 2026-11-23, no differences:** PS4 Pro, firmware
12.52, GoldHEN v2.4b18.14, payload and loader present, **all files including
PKGs moved by FTP on port 2121**, packages installed from `/data/pkg` through
Package Installer with source **Hdd**. Every field below matches that answer.

**Platform scope: this is PS4 software, and it is for every PS4** (owner-stated
2026-11-24). The target is the **PS4 family — base, Slim and Pro — as one
platform**, not one machine. Every model runs the same Orbis OS and the same
interfaces this project uses (Piglet/GLES2, `sceAudioOut`, `libSceAvPlayer`,
`scePad`, the same package format and install path), so **software that works on
PS4 runs on all PS4s**, and nothing may be designed around a Pro-only behaviour.
A model that differs in clocks or GPU capability changes only how fast the same
code gets there, never what it does. Consequences, kept as rules:

- **One package for every model.** No model-specific builds, switches or paths.
- **A Pro number is an upper bound.** Every measurement names the model it ran on
  (this owner's console is a Pro, [§7](07-environment.md)); the base PS4 is the floor a budget must
  fit, so a look or feature that only holds on a Pro does not hold.
- **Per-console prerequisites are not software requirements.** Firmware 12.52,
  GoldHEN and FTP are this machine's setup, recorded in [§7](07-environment.md); the software claims
  nothing beyond PS4 + the APIs above. Rule R28.

**Settled hardware — do not spend a console run confirming any of this** (R27).
Each line is a fact with its source; a run is only earned for behaviour that is
absent from this list.

| Fact | Source |
|---|---|
| 1080p60, vblank 16.67 ms, flips per vblank, `sceVideoOutSetFlipRate` divides it; **frames land on whole 16.7 ms multiples** | `shadPS4` videoout; `AGENTS.md` measurement rules; BREW00102 |
| Piglet = OpenGL ES 2.0 / GLSL ES 1.00, EGL 1.4; `eglSwapInterval(display, 0)` already set by the sample | BREW00093 log; `PigletApplication.cpp:182` |
| `libScePigletv2VSH` + `libScePrecompiledShaders` load from `/<sandbox>/common/lib/`, `module_start() = 0`; **no `libGLESv2.sprx`** (`0x80020002`) | BREW00093, BREW00095-103 |
| Blobs `texmap`, `bright`, `blur1`, `blur2` exist and link; bloom needs no GNM path | BREW00102 log `PROG … link=1` |
| A uniform is set on the **bound** program; `glUseProgram` first | ES2 spec; A5; BREW00102 23/23 accepted |
| No 32-bit float targets or float blending; half-float targets present; `EXT_disjoint_timer_query` advertised | [§9](09-ps4-knowledge-base.md) GPU facts; BREW00103 |
| Shader compilation 110-260 ms each, no disk cache (`GL_PROGRAM_BINARY_LENGTH` rejected) | `shipofharkinian-ps4` (`researched only`) |
| `gde` class: ~768 MiB direct / ~255 MiB flexible | same source, `researched only` (measure only if a port needs the number) |
| Packages: FTP 2121 → `/data/pkg/` → Package Installer `Hdd`; `param.sfo` needs 11 entries or CE-33481-9 | owner; R23 |
| Logs: `sceKernelOpen`/`Write`/`Fsync`, one path in `/data/Alpha PS4 test kit/`, sweep old ones | [§6](06-problems-and-fixes.md); R22; BREW00102 |
| Pad opens as a plain joystick, button numbers as [§9](09-ps4-knowledge-base.md) records | v111+ logs |

**GPU test artifact (2026-11-23):** built from OpenOrbis toolchain **0.5.2**
(CHANGELOG head; not a git checkout), clang 18.1.8, sample
`samples/piglet` unmodified. Package **`PIGLETGPUR000000`**, title id
**BREW00093**, title "OpenOrbis Piglet Sample", 6,619,136 bytes, MD5
`0204bfbc2c718512f3a101b1fc623caf`; first logging variant MD5
`a635023753c6be4c0b57829b39c5c7a2` (log came back empty); **second logging
variant, kernel-call writes, MD5 `3f7555a468962dd9e97150f4b3bcb95a`, title
"GPU Test v2 (piglet log)", version 2.00, never run); **third logging variant,
stage log, MD5 `4d556822b249fcf1c3f55a044d806fa2`, title "GPU Test v3 (stage
log)", version 3.00 — this is the one to run**. All four in `files/`. The player is unaffected: different title id, no player version number used.

**GPU result (2026-11-23, firmware 12.52, GoldHEN v2.4b18.14, package
`IV0000-BREW00093_00-PIGLETGPUR000000`, title id BREW00093, title "GPU Test v3
(stage log)", version 3.00, MD5 `4d556822b249fcf1c3f55a044d806fa2`):** the run
succeeded. `Piglet module_start() = 0 (0x00000000), handle 21` and
`PrecompiledShaders module_start() = 0 (0x00000000), handle 22`, loaded from
`/BgZIO2hcQg/common/lib/`. `EGL_VERSION: 1.4`, `EGL_VENDOR: Sony Interactive
Entertainment Inc.`, `eglChooseConfig: 1 config(s)`. `GL_VERSION: OpenGL ES 2.0
Piglet`, `GL_VENDOR: Sony Interactive Entertainment Inc.`, `GL_RENDERER:
Piglet`, `GL_SHADING_LANGUAGE_VERSION: OpenGL ES GLSL ES 2.0`,
`GL_MAX_TEXTURE_SIZE: 4096`, `GL_MAX_RENDERBUFFER_SIZE: 4096`,
`GL_MAX_VIEWPORT_DIMS: 4096 x 4096`. Extensions present: `GL_OES_texture_float`,
`GL_OES_texture_float_linear`, `GL_OES_texture_half_float`,
`GL_OES_texture_half_float_linear`, `GL_EXT_color_buffer_half_float`,
`GL_OES_standard_derivatives`, `GL_EXT_shader_texture_lod`,
`GL_EXT_texture_filter_anisotropic`, `GL_EXT_disjoint_timer_query`,
`GL_SCE_piglet_sync`, `GL_SCE_piglet_shader_binary`,
`GL_SCE_orbis_canvas_texture`. Absent: `EXT_color_buffer_float`,
`WEBGL_color_buffer_float`, `EXT_float_blend`, `EXT_shader_framebuffer_fetch`,
`NV_shader_framebuffer_fetch`, `EXT_sRGB`. Presented 3,300 frames in 27.5 s =
**119.9 fps**, closed with the PS button (no `STAGE 10`). Log inside the
package: `/data/piglet-gpu-test-v3.log`, 8,343 bytes.

**Not observed by the owner, and not to be inferred:** what the sample showed on
the television, whether the v117 header ends in `...` before the emblem, and the
v116 look verdict. All three are listed as pending in [§2](02-current-focus.md).

- Console: **PlayStation 4 Pro**, jailbroken.
- Firmware: **12.52**.
- Jailbreak: **GoldHEN v2.4b18.14** (SiSTRo's beta line, published on his Ko-fi
  shop — see [§9](09-ps4-knowledge-base.md), the GitHub release page is out of date for this console). A
  payload and a payload loader are both on the console.
- The app's own boot line reads `firmware 12.508.001 (0x12508001)`; that is the
  12.5x platform's version string and it is what our logs will show.
- Direct memory pool reported by the console: 4608 MB.
- **Transfer: FTP only, for everything, both directions.** GoldHEN's built-in
  FTP server (**port 2121**) carries packages to the console and logs,
  screenshots and `config.ini` back. No USB stick and no other method — do not
  write steps that assume one.
  - **Packages:** upload `Ultimate-Media-Player-vNNN.pkg` to `/data/pkg/` on the
    console, then install it from **Package Installer** with the package source
    set to **Hdd [`hdd:/data/pkg/`]**. FTP only puts the file there; the
    installer is what installs it.
  - **App data read back** over the same connection:
    `/data/ultimate-ps4-media-player/` — `app.log`, `app.old.log`, `config.ini`,
    `shots/` (BMP captures, v111+).
  - **`config.ini`** is edited in place over FTP; R3 in the app reloads it.
  - The FTP server must be switched on in **GoldHEN → Server Settings**; it
    needs a network connection of any kind. Payload/BinLoader on 9090 and klog
    on 3232 are separate services and neither is needed for packages.
- Build machine: Linux x86_64, bash. Toolchain Clang **18.1.8**
  (conda-forge clangdev build, per the console's own build line).
- Toolchain root `$HOME/openorbis/PS4Toolchain`; pkg signing library
  `$HOME/openorbis/ssl11/usr/lib/x86_64-linux-gnu`; host SDL and friends in
  `$HOME/hostdeps`; compiler cache `$HOME/.cache/ps4-toolchain` (can be wiped
  between sessions; `bash ~/bootstrap-toolchain.sh` restores it in 2–4 min).
- Build command, preview command and test command: `AGENTS.md` §Build and check.
- Project layout: `files/ps4-media-player/` (source), packages in `files/`,
  round notes in `files/roundN/`, source snapshots in `files/archive/`.

### Rules tooling (added 2026-11-24)

Four programs keep these files honest, and none of them writes prose:

- `tools/check-manifest.py` — 28 checks: section order, rule ids, cross
  references, superseded tags against amendments, MD5s against `files/`,
  read-order headers, byte budgets, AGENTS figures that repeat a measurement
  without citing the section that owns it, and a status board older than the
  files it summarises.
- `tools/rule-index.py` — writes `files/RULES.index`: every rule with its status
  and evidence. It fails a `measured` rule that cites no log, file, MD5 or build
  id, and a figure with no status word.
- `tools/status.py` — `make status` writes `files/STATUS.html`, the one screen
  that answers healthy / where we are / what waits on the console. Generated,
  never hand-edited; `make check` fails when it is older than the rules files.
- `_scratch/linttest/mutate.py` — breaks one thing at a time and proves each
  checker notices it: 12 of 12, including that the build stamp does not read the
  clock.

`tools/publish-docs.py` builds the linked publishing tree (rules per page,
manifest in twelve parts, §-references rewritten to real links) and
`tools/check-links.py` fails on a broken relative link or heading anchor;
`tools/push-docs.sh` publishes it bottom-up, three commits, leaves first.

`tools/buildstamp.py` fixes `__DATE__`/`__TIME__` at compile time, so two builds
of one source state produce the same package and an MD5 identifies source again.

Budgets, so a document that only grows stops being read: `MANIFEST.md` 128 KB,
`AGENTS.md` 40 KB (and G9's line ceiling), `HISTORY.md` 96 KB,
`STATUS.html` 32 KB.

[Manifest index](README.md) · [Back to the index](../README.md)
