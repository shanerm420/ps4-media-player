# Logging: how we got here, and what /data accepts (appendix)

Moved out of `MANIFEST.md` on 2026-11-24 under rule G9: the manifest is at its
128 KB ceiling and rules must not be squeezed to fit detail. This is history and
evidence, not a rule. The rules it supports are R22 (one log, one folder, sweep)
and R23 (package check).

### The three other logs the owner found (read 2026-11-23, all ruled out)

| File | What produced it | Relation to the Piglet run |
|---|---|---|
| `Loader_Logs.txt` (9,870 bytes) | the console's own loader verifying an RSA/SHA-256 signature on `/mnt/sandbox/NPXS39041_000/app0/Media/rsa.pub`, repeated dozens of times | **none.** NPXS39041 is a system application (its sandbox, its `Media/` folder), the lines carry no date, and the words BREW00093, Piglet, EGL and GL do not appear. It is a system app's sign-in/verification loop, not our sample. |
| `memory.log` (7,200 bytes) | `sceLibcMspaceCreate()` / `sceLibcMspaceMallocStatsFast()` pairs, one pair per line, no timestamps, no title id | **none.** It is a libc heap/mspace diagnostic dump — the kind a plugin or a debug build writes — and has no event in it. |
| `UnityOrbisBridge.log` (305 bytes) | a Unity-Orbis bridge: `Currently running on PlayStation 4 (12.50)`, `App has broken from the sandbox in 0s!`, SCE libraries loaded, SCE message dialog | **none, and it is six days older.** Dated 11/17/2026 02:57:47, firmware 12.50, and the bridge's own log — the Piglet run was on 11/23 on 12.52. |

So none of the three can say anything about the empty Piglet log. They are worth
one fact each: the console does run apps that write their own logs to disk, and
`UnityOrbisBridge.log` shows a homebrew app escaping its sandbox and writing a
dated log successfully on 12.50 — which supports "writing a log from homebrew
works here", and therefore supports the reading that v1's redirect itself failed
rather than every write being impossible. Marked as an inference, not a result.

### Logging from homebrew on 12.52, and where /data is writable (2026-11-23)

**Verified in this project's own code and packages:**

- The player writes `/data/ultimate-ps4-media-player/app.log` with
  `fopen(..., "w")` (`src/applog.c:225`), and its earliest boot line opens the
  same path with POSIX `open(..., O_WRONLY|O_APPEND|O_CREAT, 0644)` and `write()`
  (`src/main.c:91`). The owner pulls that file over FTP after every run, so
  **libc file I/O into a `/data` subfolder is proven on this console** — the
  method is proven, the folder is not.
- `sceKernelOpen`, `sceKernelClose`, `sceKernelWrite`, `sceKernelWritev` and
  `sceKernelFsync` are declared in `orbis/libkernel.h` (lines 339, 81, 451, 453,
  143) and linked through `-lkernel`; `dup2` is at `unistd.h:40`;
  `sceKernelGetFsSandboxRandomWord` at `libkernel.h:185`. All used by the v2
  package.
- **The toolchain's `fcntl.h` defines only `O_RDONLY`/`O_WRONLY`/`O_RDWR`.** An
  open that means to create a file has to supply `O_CREAT`/`O_TRUNC`/`O_APPEND`
  itself. This is a trap worth remembering: `sceKernelOpen(path, O_WRONLY, 0666)`
  cannot create anything, and a silent failure follows.
- The first logged package's `eboot.bin` carries `/data/piglet-gpu-test.log`
  (one string), so its redirect compiled in; the file still came back empty.

**Answered by the v3 run, 2026-11-23:** the `/data` root, `/data/pkg` and
`/data/ultimate-ps4-media-player` all accepted `sceKernelOpen` with
`O_WRONLY|O_CREAT|O_TRUNC` from this title — `path OK … (fd 5)`, `(fd 6)`,
`(fd 7)` — so a title **can** create files at the `/data` root and in
`/data/pkg`, and `O_CREAT`/`O_TRUNC` with the FreeBSD 9 values work at all
three. The three USB mounts answered `rc 0x80020002`, which is "no such file or
mount", not "permission denied".

**Still unknown, and not to be repeated as fact:** whether `freopen` on stdout
works in this libc (v1's silence says it did not, but v1 could not report its
own return); whether stdio buffers survive a PS-button close (v3's kernel writes
did, which is why it fsynced each line).

**Where the sources disagree** (five phrasings, checked 2026-11-23):

- ConsoleMods' *PS4:Files and Directories* describes `/data` as where **homebrew
  apps keep their own folders** (Apollo's user folders are the example) and
  lists `/mnt/usb0`, `/mnt/usb1`, `/mnt/usb2`, `/mnt/ext0` for removable
  storage — which supports "a `/data` subfolder per title", the pattern this
  player already follows, and the USB paths v2 tries. It does not say the
  `/data` root itself is writable by a title.
  <https://consolemods.org/wiki/PS4:Files_and_Directories>
- Two install guides (MacMyths, TechBloat) call `/data/` or `/data/pkg/`
  "writable", but they mean **writable over FTP by the user**, not by a running
  app's sandbox — a different permission subject, and the reason v1's silent
  failure cannot be ruled out from documentation alone.
  <https://macmyths.com/how-to-install-pkg-files-on-ps4-over-usb-and-ftp>
- The PS4 Developer Wiki's *Homebrew Enabler* page says a HEN-loaded fSELF has
  "system permissions, for example RW access to all filesystem partitions" —
  the direct counter-claim, predicting `/data` root *is* writable.
  <https://www.psdevwiki.com/ps4/Enabling_Homebrew> Unresolved; v2's six-path
  table answers it in one run instead of an argument.
- OpenOrbis' *Building Homebrew* says only that `/app0/` is the package root and
  that bundled data must be inside the package; it is silent on writable paths,
  so it neither supports nor contradicts the two readings above.

### Round-state notes (moved)

The round-by-round notes that used to sit here, up to v118 and the GPU rounds,
are in **`ROUND-NOTES.md`** - history and detail, not rules (G9).

