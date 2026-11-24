# Self-review — 2026-11-24

Prepared before Freebuff review. Commands were run on the local audit clone unless a laptop path is shown.

## Results

- Branch: `audit/relay-2026-11-24`, tracking `origin/audit/relay-2026-11-24`.
- No pull request opened.
- Working tree was clean before this review.
- `git diff --name-only main..HEAD -- . ':(exclude)audit/2026-11-24'` returned no output: no tracked repository file outside `audit/2026-11-24` was changed.
- Original relay snapshot verified at md5 `e6703c9e00e949834dda9e519896491c`; the public copy was later redacted and renamed `current/RELAY.md.txt`, so that hash now applies only to the private laptop relay.
- Relay tail ends at MSG-0081 (development method documented).
- `HANDOFF.md` links `METHOD.md`; `METHOD.md` records the owner-stated two-phone/Termux, laptop, PS4 Pro, free-AI, low-cost setup.
- Secret-pattern scan over `audit/2026-11-24` found no GitHub token pattern, private-key header, password assignment, or API-key assignment.

## Defect found and fixed

Initial `sha256sum -c SHA256SUMS.txt` and `md5sum -c MD5SUMS.txt`, run from `audit/2026-11-24`, failed for all 38 listed files because the checksum files had been regenerated from the repository root and contained `audit/2026-11-24/...` prefixes.

Fix: regenerated both checksum files from `audit/2026-11-24` with relative paths `original/...`, `current/...`, and `backups/...`.

Post-fix verification:

```text
sha256sum -c SHA256SUMS.txt  -> all listed files OK
md5sum -c MD5SUMS.txt        -> all listed files OK
38 SHA256SUMS.txt
38 MD5SUMS.txt
```

## Additive content

The branch adds only files under `audit/2026-11-24`:

- `FINDINGS.md` — verified state, A3/A3b status, rules/document findings, open owner decisions.
- `HANDOFF.md` — takeover procedure, paths, verification commands, safety rules.
- `METHOD.md` — owner-stated development method and setup.
- `current/RELAY.md.txt` — redacted public relay snapshot at MSG-0081, plus a dated correction note.
- `original/` — exact copies from published `main` at `f13e462`.
- `backups/` — all dated laptop backups from `_scratch/backups/`.
- `diffs/` — unified diffs between original and current files.
- `MD5SUMS.txt`, `SHA256SUMS.txt` — checksums for `original/`, `current/`, and `backups/`.

## Freebuff review corrections — 2026-11-24T12:20Z

Freebuff independently reviewed the published branch and found four verified
claims plus three defects and one policy breach. Accepted in full.

- Public topology was a real disclosure even though no credential was exposed.
  Literal laptop/phone addresses, usernames, home paths and clone paths in
  `METHOD.md`, `HANDOFF.md`, `FINDINGS.md`, diff headers and the public relay
  copy were replaced with placeholders. The private laptop relay was not edited.
- CI link check had failed on audit-depth copies of published docs. Five audit
  markdown files were renamed to `.md.txt` without changing bytes; no published
  repository file was touched. `tools/check-links.py` now reports
  `80 pages, 0 problems` and exits 0.
- Gate figures were stale. `HANDOFF.md` and `FINDINGS.md` now say
  `30 passed, 1 failed - STATUS.html stale` and record section 16. The current
  fixed checker is copied at `current/tools/check-manifest.py`, md5
  `e26b6f9a7c20ff94564b5d104b203ebe`; old 27-check copies are marked historical.
- `FINDINGS.md` now states that the firmware triple-indication is verified but
  the owner read is pending, so it cannot be closed by inference.
- Relay lock guidance now says to use a writable per-side lock; `/tmp` is not
  writable in Termux. Placeholders replace the unrooted-Termux-incompatible
  `/root/...` clone path.

Additivity is preserved: no tracked repository file outside
`audit/2026-11-24` was changed; `main` remains at `f13e462`.
