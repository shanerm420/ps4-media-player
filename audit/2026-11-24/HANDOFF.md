# Takeover handoff — relay audit

Any AI taking over should start here. Do not treat prior chat history as evidence; the relay, the laptop files, and this branch are the evidence.

## Scope and safety

- Repository: `shanerm420/ps4-media-player`.
- Audit branch: `audit/relay-2026-11-24`; created from published `main` at `f13e46232474c4845f4be25d619fe2cd735a49dd`.
- This branch is additive. Never overwrite or edit original repository files in place.
- Do not force push. Do not amend unless the owner explicitly asks. Do not commit secrets, tokens, keys, or private data.
- No pull request has been opened. Open one only if asked.
- Owner instruction: Big-pickle keeps this branch current as findings conclude; Freebuff is the rules/document editor when owner-approved. The owner may change roles; read the latest relay tail first.
- Owner-stated development method: two Termux phones, one Linux laptop, and the owner's PS4 Pro; other PS4s may exist but are not required. The setup is intended to work with free AI and minimal cost. See `METHOD.md`.

## Where things live

- Laptop: `ssh -o BatchMode=yes <laptop-user>@<laptop-host>`
- Kit root: `<laptop-home>/Desktop/PS4Dev/ps4-publish-b`
- Project: `<laptop-home>/Desktop/PS4Dev/ps4-publish-b/ps4-media-player`
- Relay: `<laptop-home>/Desktop/PS4Dev/ps4-publish-b/RELAY.md`
- Local audit clone: `<audit-clone>`
- Audit directory: `<audit-clone>/audit/2026-11-24`

## First actions for a takeover

1. Read `HANDOFF.md`, `FINDINGS.md`, `METHOD.md`, and the tail of `audit/2026-11-24/current/RELAY.md.txt`. That file is a redacted excerpt, not a byte-identical relay snapshot.
2. Check the live relay size on the laptop with `stat -c %s <relay-path>`; obtain `<relay-path>` from the owner or the private relay, not from this public file.
3. If it changed, read everything after your last known offset. Message numbers collide; identity is sender + UTC timestamp.
4. Verify hashes yourself with `md5sum` on the laptop. Do not accept a claim because another agent wrote it.
5. Append one relay message under a lock file writable on your side, for example `flock $HOME/relay.lock`, at the bottom only, with a UTC timestamp and sender name. `/tmp` is not writable in Termux; do not assume the laptop's lock path is visible. Do not edit another sender's text.
6. Update this branch only with concluded, independently checked findings.

## Relay protocol in force

- There are two AIs, Freebuff and Big-pickle, and one shared laptop. The laptop is evidence, not a third participant.
- Watch `RELAY.md` byte size. A size change is the wake event; offsets replace clock-based guessing.
- One reply per turn, then wait. Do not rapid-fire messages.
- ACK within 10 seconds of noticing a message; never ACK an ACK.
- R37: each side verifies independently. Owner-supplied facts are marked `owner-stated`; tree-checkable facts are verified from the laptop.
- Corrections are appended, never inserted above earlier text.
- A quiet relay does not prove an unchanged tree. Documents have moved before a relay append landed.

## Current verified state

| Item | Value |
|---|---|
| MANIFEST.md | `a357932d80435583f0c327349a98d4bd`, 130916 B, 156 B headroom |
| HISTORY.md | `e4f789afdcf455bb717ff4c12c1f6c17` |
| RULES.index | `db62edde6f4cd4efc1b585494232a935`, 48 rules, 48 falsifiers |
| AGENTS.md | `73db1911ed718e0f80b27876c8d5c852`, 640-line ceiling |
| src/vis_art.c | `a5c2fb3939e39d404b9f68ed48835ffa` |
| src/main.c | `e66562c6a452019ca406c7950c7a1f8a` |

Re-verify before acting. These values can change without this document changing. The public relay copy is a redacted excerpt, not a byte-identical snapshot; do not claim relay byte-identity from this branch.

## Verification commands

Run from the project directory on the laptop:

```bash
python3 tools/check-manifest.py
python3 tools/rule-index.py
python3 tools/publish-docs.py --check
md5sum MANIFEST.md ../HISTORY.md ../RULES.index AGENTS.md src/main.c src/vis_art.c
```

Notes:

- `check-manifest.py`: 30 passed, 1 failed — `STATUS.html` stale; owner must run `make status`. Section 16 adds three checks: RULES.index parses, ids agree both ways (38 R + 10 G = 48), every entry carries a falsifier. The current fixed checker is copied at `current/tools/check-manifest.py`, md5 `e26b6f9a7c20ff94564b5d104b203ebe`.
- `rule-index.py`: 48 rules, 0 problems, exit 0.
- `publish-docs.py --check`: 84 files, 410 KB, 76 pages, link check 0. This tool rewrites `_scratch/publish/`; take hashes before running it.
- Falsifier-number audit: for each R rule, check numbers in the falsifier against that rule's own statement/evidence/status. Current flags: R1 `12.52`, R6 `16.6`, R15 `3.69`; all are legitimate derived figures, not a gate.

## Source state

- A3 in `src/vis_art.c`: texture created at source size, `SDL_UpdateTexture(s_tex, NULL, c->p, c->w * 4)`, scaling via `SDL_RenderCopy`; `s_up`, `s_up_cap`, and `upscale_bilinear` removed.
- `pr_upscales` and `pr_upscale_bytes` no longer increment; their zeros are decorative, not runtime evidence.
- A3b in `src/main.c`: `SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")` before accelerated renderer creation and a readback log after renderer startup.
- `build/src/vis_art.o` compiled clean. `build/src/main.o` project make is blocked by pre-existing unquoted `BUILD_TIME` macro at `src/main.c:3994`; manual compile with quoted build-time macros succeeds.
- Runtime counter window and visual/ghosting check are UNVERIFIED. Do not claim console results from host reasoning.

## Open owner decisions

- Exact firmware string from Settings > System Information. Owner recollects "12.5, or 12.52, I don't know which"; tree has 12.52 prose, boot string `12.508.001 (0x12508001)`, and older Unity log 12.50.
- Whether the console was previously on 12.50 before an update.
- Loader build/version.
- Go/no-go on evicting a section so R39 and R39b fit; only 156 B headroom remains.
- `make status` for stale `STATUS.html`.
- Console A3 counter window: `pr_upload_bytes / pr_uploads = 2,073,600` at 960x540.
- Full-canvas visual check, including >=300 frames for ghosting.

## How to update the audit branch

Copy new files into `audit/2026-11-24/current/`, add new dated backups under `backups/`, regenerate diffs and checksums, then commit only additive changes:

```bash
git -C <audit-clone> add audit/2026-11-24
git -C <audit-clone> \
  -c user.name='shanerm420' \
  -c user.email='shanerm420@users.noreply.github.com' \
  commit -m "docs: update relay audit findings"
git -C <audit-clone> push origin audit/relay-2026-11-24
```

Do not modify `main`. Do not overwrite `original/`, `backups/`, or previous findings. Add a new dated file or append a dated section instead.
