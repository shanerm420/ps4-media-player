# Rule index

Every rule in the project, with its status and the evidence
behind it. Generated from `MANIFEST.md` §9 and `AGENTS.md` by
`tools/rule-index.py`; the machine-readable form is `RULES.index`.

Status vocabulary: **measured** (a console log proves it), **researched only**
(a source says so), **assumed** (a starting point, not a finding),
**contested** (two readings, a test settles it), **procedural** (a method).

## Rules that shape the work (G)

- [G1](G1.md) — Memory budget per subsystem, stated in every plan. `measured`
- [G2](G2.md) — Crash and hang runbook. `procedural`
- [G3](G3.md) — A fixed benchmark, compared rather than re-derived. `procedural`
- [G4](G4.md) — Input latency, target two frames, not yet measured here. `assumed`
- [G5](G5.md) — Thermals and sustained clocks. `assumed`
- [G6](G6.md) — `config.ini` migration, never a silent reset. `procedural`
- [G7](G7.md) — A clean-room note per borrowed idea. `procedural`
- [G8](G8.md) — "Professional" defined so it can be failed, from the v112 and v115 console runs. `measured`
- [G9](G9.md) — The rules core has a size ceiling. `procedural`
- [G10](G10.md) — A new project starts its own file, and links back. `measured`

## Graphics and hardware facts (R)

- [R1](R1.md) — The Piglet ES2 path works on this console: firmware modules load, EGL 1.4 comes up, `GL_RENDERER: Piglet`, frames present `measured`
- [R2](R2.md) — Half-float render targets work; 32-bit float render targets do not; no float blending, no framebuffer fetch, no sRGB framebuffers — so feedback is ping-pong targets `measured`
- [R3](R3.md) — Maximum texture and renderbuffer size 4096, viewport 4096x4096 `measured`
- [R4](R4.md) — The sample's 119.9 fps is a textured quad with no shader load, no bloom and no SDL window beside it — it is **not** a budget for a look `measured`
- [R5](R5.md) — The SDL present path costs 21.6–24.5 ms a frame on every screen, including one that draws nothing `measured`
- [R6](R6.md) — No look fits a 60 Hz frame at step 1, even on the host: 27.8–75.4 ms `measured`
- [R7](R7.md) — What a real 1080p look costs on this GPU `assumed`
- [R8](R8.md) — Audio reaction is one global beat list today; per-band beats with a refractory interval and geometric decay, log-spaced bands, packet interpolation and automatic gain are the researched model `measured`
- [R9](R9.md) — There is no `libGLESv2.sprx` on this console, so GL work links Piglet by name instead of going through SDL's GL lookup `measured`
- [R10](R10.md) — **Piglet ES2 can only use shaders that already exist as blobs in `libScePrecompiledShaders.sprx`** — OpenOrbis 0.5.2 ships no GLSL compiler, and the runtime compiler is a devkit module this console does not have `measured`
- [R11](R11.md) — **A bloom chain is assemblable from existing blobs**: separable blur pair, brightness/contrast, opacity and texmap composite, on a complete half-float target `measured`
- [R12](R12.md) — There is **no per-pixel custom shader** on this path — no fluid solve, raymarch, refraction or true bright-pass threshold `measured`
- [R13](R13.md) — **The blur programs are an 11-tap Gaussian with a vec4 radius, and every program in the chain links** `measured`
- [R14](R14.md) — **CPU timers around GL draws measure submission, not execution** — a 1080p composite reported at 0.03 ms and a blur at 0.00 ms are queue times, not GPU work `measured`
- [R15](R15.md) — **`glFinish()` per pass costs ~13–14 ms on Piglet**, independent of the pass size — a 480x270 pass measured 14.6 ms against 18.3 ms for 1080p `measured`
- [R16](R16.md) — **`u_blurRadius` is a `vec2` (`0x8B50`), not a vec4** — set it with `glUniform2f`; a wrong type is rejected with `illegal to set uniform data with different types` and silently leaves the kernel at defaults `measured`
- [R17](R17.md) — **`GL_RGBA16F` does not exist in ES2, but a half-float render target does** — `internalformat GL_RGBA` + `type GL_HALF_FLOAT_OES` (0x8D61) completes; a port that asks for `GL_RGBA16F` gets a failure and wrongly concludes half-float is unavailable `measured`
- [R18](R18.md) — **`eglSwapInterval(1)` after `eglMakeCurrent` blocks `eglSwapBuffers` forever**; set it before the surface is created, or use 0 `researched only`
- [R19](R19.md) — **Piglet memory comes from a flexible limit set at `scePigletSetConfigurationVSH`** — a working split is 128 MiB system shared / 512 MiB video shared / 236 MiB flexible / 4 MiB command buffers, textures and render targets out of the flexible share, a 1080p framebuffer pair over 64 MiB `researched only`
- [R20](R20.md) — **Frame timing here sits on a ~8.3 ms present floor, so differencing two configs cannot see the work** — `A empty` measured 8.19 ms and `B scene` with a 1080p ramp and 2,000 sprites measured 8.32 ms, a 0.13 ms difference for work that cannot plausibly be free; anything drawn under the floor disappears into the flip `measured`
- [R21](R21.md) — **A uniform's set must be probed before it is trusted; the location the driver returns is not always accepted by the driver's own setter** — `u_blurRadius` (reported `0x8B50 FLOAT_VEC2`, loc 49) refuses `glUniform2f` at that location with `illegal to set uniform data with different types`, and `u_gaussianKernel[0]` (loc 51) refuses `glUniform1fv` with `Failed to retrieve uniform info`, each leaving sticky `0x0502` `measured`
- [R22](R22.md) — **One log, in `/data/Alpha PS4 test kit/`, and delete what earlier builds left** — old builds opened seven sinks at once, so every run scattered copies that had to be deleted by hand `procedural`
- [R23](R23.md) — **No package is handed over until `tools/check-pkg.py` passes it** — BREW00102 installed as Corrupted Data (`CE-33481-9`) then refused to start (`CE-34544-0`): its `param.sfo` had 7 entries of 11, because a killed build left it half-written and `make` kept it `procedural`
- [R24](R24.md) — **GPU cost comes from the GPU clock; a CPU window is only an upper bound** — draws + `glFinish` with no swap in the window still gave 16.656 ms at N = 0, 16.659 at N = 16, 33.340 at N = 64 `measured`
- [R25](R25.md) — **Read what other PS4 homebrew and the ES2 spec already solved before a round** — two console runs (A4, A5) went into a uniform question the spec answers in one line `procedural`
- [R26](R26.md) — **Reflect on this manifest before every step, and verify that step against it** — the owner had to point out that BREW00102's timing question was already published (`shipofharkinian-ps4`) and the uniform question was answered by the ES2 spec, after both had been sent to the console as tests; work that repeats a recorded answer is waste, and it is the owner's time being spent `procedural`
- [R27](R27.md) — **The hardware is known: never spend a run on what §7, §9, `PIGLET-NOTES.md` or the measurement rules already settle** — BREW00103 was queued to ask "why 16.66 ms" when the measurement rules have said since v112 that every frame waits on the display and lands on whole 16.7 ms multiples; the owner had to say so `measured`
- [R28](R28.md) — **The target is the PS4 family, not this one console** — the work is PS4 software, and PS4 is the common denominator: same OS, same GLES2/Piglet, same audio, video, pad and package interfaces on every model, so working PS4 software runs on all PS4s `assumed`
- [R29](R29.md) — **Do not reproduce a method that already exists - reuse it** — this project has rebuilt things that were already there: a bespoke logger and an on-screen text renderer for every probe while the player already had `applog` and its own text drawing, and a uniform-binding answer the ES2 spec states `procedural`
- [R30](R30.md) — **Ask the owner only about how it should look; every technical question is answered here and work continues** — the owner's instruction, 2026-11-24: "the only thing that should be asked is aesthetic questions... if it has to do with programming or coding in any way... refer to the Manifest to find the solution instead of asking the user" `procedural`
- [R31](R31.md) — **Two threads write the playback flags with no lock** - `s_playing`, `s_quit` and `s_track_finished` are declared `volatile int` in `src/main.c:157-159`; the SDL loop sets them, and `audio_thread` also writes them at the end of a track (`s_playing = 0; s_track_finished = 1;`, `src/main.c:1883-1885`), while `src/main.c` holds no `pthread_mutex` anywhere, so `volatile` is being asked to do a synchronization primitive's job `procedural`
- [R32](R32.md) — **Owner-authorised rounds are the only way source is written** `procedural`
- [R33](R33.md) — **One namespace: a rule is in the table and in the index, or it does not exist** `measured`
- [R34](R34.md) — **A rule without a falsifier is a preference** `measured`
- [R35](R35.md) — **Every pointer resolves, and a skipped check is never a pass** `measured`
- [R36](R36.md) — **Every measured rule names the machine it was measured on** `assumed`
- [R37](R37.md) — **Each agent verifies independently, and no agent's report is evidence for the other** `procedural`
- [R38](R38.md) — **A pointer resolves in one of three declared namespaces** `measured`

## Amendments

See [amendments](../amendments/README.md).

[Back to the index](../README.md).
