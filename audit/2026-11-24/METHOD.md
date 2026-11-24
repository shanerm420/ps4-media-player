# Development method and setup — owner-stated

Recorded 2026-11-24 at the owner's instruction. This is part of the project method, not just chat context. A future AI should be able to reproduce the workflow from this file and `HANDOFF.md`.

## Scope statement

The owner describes this as a major project. The workflow is deliberately built to be reproducible with inexpensive hardware and free AI assistance; it does not depend on a fleet of consoles or a paid lab setup.

Items below marked `owner-stated` came from the owner directly. Items marked `verified` were checked from the laptop, repository, or relay files. This public copy uses placeholders for hosts, users and local paths; obtain real endpoints from the owner or the private relay, never from this file.

## Hardware and roles

- `owner-stated`: development uses **two phones running Termux** and **one Linux laptop** as the shared file host.
- `owner-stated`: the two phones are the two AI workstations; the laptop hosts the kit, project, relay file, backups, and toolchain.
- `verified`: laptop SSH target is `<laptop-user>@<laptop-host>`; kit root is `<laptop-home>/Desktop/PS4Dev/ps4-publish-b`; project is `<laptop-home>/Desktop/PS4Dev/ps4-publish-b/ps4-media-player`.
- `verified`: one OpenCode phone side was recorded in the relay as `<phone-host>`, Termux `sshd` port `<termux-sshd-port>`, user `<phone-user>`. Treat that as a historical endpoint, not a permanent secret or a guarantee.
- `owner-stated`: a second phone is also in the path; its endpoint is not recorded here.
- `owner-stated`: the active console is the owner's **PS4 Pro**. Other PlayStation 4 units are available to the owner, but **multiple consoles are not required** for this project.
- `owner-stated`: the current work targets only the PS4 Pro.

## Console method

- Packages move by FTP to `/data/pkg/` on the PS4; GoldHEN FTP is port `2121`; package installation is done on the console from Package Installer using `hdd:/data/pkg/`.
- Evidence is pulled from `/data/ultimate-ps4-media-player/`: `app.log`, `app.old.log`, `config.ini`, and BMP captures in `shots/`.
- Probe/test logs go under `/data/Alpha PS4 test kit/` from BREW00101 onward.
- The exact firmware string remains an open owner check. The tree contains 12.52 prose, boot string `12.508.001 (0x12508001)`, and an older UnityOrbisBridge log saying 12.50. Do not silently correct it.
- The console is the decider for timing, visuals, and firmware facts. Host numbers rank approaches only.

## AI coordination method

- `owner-stated`: the workflow is intended to work with **free AI**, without requiring a paid setup.
- `verified`: the two agents communicate by appending to a shared file, `RELAY.md`, on the laptop. No external relay service, public URL, or secret token is required.
- `verified`: the relay is append-only. Identity is sender + UTC timestamp; message numbers have collided repeatedly.
- `owner-stated / verified in use`: sync by watching the byte size of `RELAY.md`. When the size changes, read everything after the previous offset. Do not guess timing from message numbers.
- Each AI independently verifies tree-checkable claims. Owner-only facts — exact console, exact firmware string, loader build — are marked `owner-stated`.
- Corrections are appended, not silently applied to another agent's text.

## Cost and reproducibility principles

- Reuse the laptop that already hosts the project.
- Reuse phones already running Termux; SSH is enough for file relay and verification.
- Use the open toolchain and the checked-out repository already present; do not assume a paid SDK or paid cloud service.
- Use GitHub as an additive backup and handover surface; do not overwrite original published files.
- Keep dated backups, hashes, and diffs so a future AI can reconstruct how the tree changed.
- Keep open decisions explicit. Console reads, display verdicts, and physical installs stay with the owner.

## What this setup deliberately avoids

- No assumption that multiple PS4 consoles are needed.
- No assumption that a paid AI service is needed.
- No reliance on one AI's memory as evidence.
- No silent overwrite of the original repository or project documents.
- No console-speed or visual claim derived from host reasoning.

## Takeover rule

A new AI starts from `HANDOFF.md`, `FINDINGS.md`, this file, and the live relay. It verifies hashes and file state itself, appends one message with its own sender identity, and updates the GitHub audit branch only with concluded findings.
