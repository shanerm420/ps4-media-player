# Relay audit backup — 2026-11-24

This directory is additive. Nothing in the repository's existing tree was overwritten.

- Original files: exact copies from published `main` at `f13e46232474c4845f4be25d619fe2cd735a49dd`.
- Current files: copies from `<laptop-home>/Desktop/PS4Dev/ps4-publish-b` on the MX laptop after the documented one-pass edits.
- Backups: all dated files from `ps4-media-player/_scratch/backups/` on the laptop.
- Diffs: `diff -u original/... current/...`.
- `current/RELAY.md` is the full append-only relay record, including all corrections and message-number collisions.
- `MD5SUMS.txt` and `SHA256SUMS.txt` cover `original/`, `current/`, and `backups/`.

## Verified state at handover

| Item | Value |
|---|---|
| MANIFEST.md | `a357932d80435583f0c327349a98d4bd`, 130916 B, 156 B headroom |
| HISTORY.md | `e4f789afdcf455bb717ff4c12c1f6c17` |
| RULES.index | `db62edde6f4cd4efc1b585494232a935`, 48 rules, 48 falsifiers |
| AGENTS.md | `73db1911ed718e0f80b27876c8d5c852`, unchanged, 640-line ceiling |
| src/vis_art.c | `a5c2fb3939e39d404b9f68ed48835ffa` |
| src/main.c | `e66562c6a452019ca406c7950c7a1f8a` |

Identity in the relay is sender + UTC timestamp; message numbers collided repeatedly.

## A3 / A3b source state

A3 changed `src/vis_art.c` to create the texture at source size, upload `c->p` with pitch `c->w * 4`, scale with `SDL_RenderCopy`, and remove `s_up`, `s_up_cap`, and `upscale_bilinear`. `pr_upscales` and `pr_upscale_bytes` remain in the counter format but no longer increment; their zeros are decorative, not runtime evidence.

A3b changed `src/main.c` to set `SDL_HINT_RENDER_SCALE_QUALITY` to `"1"` before the accelerated renderer is created and log the readback after renderer startup. The hint compiles cleanly in isolation under the project's clang flags. Project `make build/src/main.o` is blocked by a pre-existing unquoted `BUILD_TIME` macro (`00:00:00`) at `src/main.c:3994`; that was not changed.

Runtime and visual gates remain UNVERIFIED:

- live counter window showing `pr_upload_bytes / pr_uploads = 2,073,600` at 960x540;
- `rv_bytes` unchanged and `rv_alloc_fail = 0`;
- renderer texture creation stops per-frame after source size settles;
- no ghosting across a >=300-frame full-canvas run;
- the hint readback is not proof the backend honours it.

## Rules and document findings

- R32: owner-authorised scoped build rounds, with dated backups and hashes in the relay.
- R33: a rule is in both the table and `RULES.index` or it is a note.
- R34: a rule without a falsifier is a preference; statuses use a closed set.
- R35: every pointer resolves and a skipped check is never a pass.
- R36: measured rules name firmware and loader version.
- R37: each side verifies independently; the laptop is shared evidence, not a third agent.
- R38: pointers resolve in one of three declared namespaces — working tree, publish output, or console runtime under `/data/`.

Pointer audit conclusion: `CHECKSUMS.txt` and `docs/README.md` are publisher-generated, `config.ini` is a console runtime path, and eleven paths are declared absent from this copy: `CHANGELOG.md`, `Loader_Logs.txt`, `nv12.h`, `docs/ROOT-POINTER.md`, `docs/TECHNICAL.md`, `_scratch/linttest/mutate.py`, `Piglet-GPU-Test-BREW00093-logged-v3.pkg`, `files/GPU-Bloom-Slope-Probe-BREW00103.pkg`, `files/Ultimate-Media-Player-v118.pkg`, `files/bootstrap-toolchain.sh`, `files/round7/README.md`.

The mechanical falsifier-number audit found three flags, all legitimate derived figures:

- R1 `12.52` — firmware qualifier;
- R6 `16.6` — `1000/60`, the 60 Hz frame budget;
- R15 `3.69` — `18.30 - 14.61` from R15's own evidence cell.

This audit is not a gate because it cannot distinguish a derived constant from a misquote. R39 (~400 B) and R39b (~150 B) would make that distinction explicit, but MANIFEST.md has only 156 B headroom.

Checker state:

- `tools/rule-index.py`: 48 rules, 0 problems, exit 0.
- `tools/check-manifest.py`: 30 passed, 1 failed — `STATUS.html` stale; owner must run `make status`. Section 16 adds three checks: RULES.index parses, ids agree both ways (38 R + 10 G = 48), every entry carries a falsifier. The fixed checker is copied at `current/tools/check-manifest.py`, md5 `e26b6f9a7c20ff94564b5d104b203ebe`; the old 27-check copies in `original/` and `backups/` are historical and must not be used as the current gate.
- `tools/publish-docs.py --check`: 84 files, 410 KB, 76 md/html pages, link check 0 problems; this tool rewrites `_scratch/publish/`.
- The eleven cited `.log` files are absent from the tree and are classified as console-side `/data/` artefacts; the note asks for archiving.

## Firmware question

The owner stated the firmware as "12.5, or 12.52, I don't know which". The tree has three conflicting indications: prose recording 12.52 from owner answers, an app boot string `12.508.001 (0x12508001)`, and an older UnityOrbisBridge log saying 12.50. The triple-indication is verified, but the owner read is still pending; do not close this by inference. The console's Settings > System Information string is authoritative. No correction was made.

## Open owner decisions

- Read the exact firmware string from the console.
- Confirm whether the console was previously on 12.50 before an update.
- Confirm the loader build/version.
- Give go/no-go on evicting a section so R39/R39b can be written.
- Run `make status`.
- Run the console A3 counter window and visual/ghosting checks.
