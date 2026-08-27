# TangoQ — a Mixxx fork for tango DJs

This is a fork of [Mixxx](https://github.com/mixxxdj/mixxx) that reshapes it into
a dedicated **tango DJ app** for DJs who play pre-arranged tanda sets at milongas.
At its core is a cursor-based Auto DJ. The binary is **`tangoq.exe`**, not
`mixxx.exe`.

Positioning: TangoQ is a **one-way departure** from a stock DJ app to a dedicated
tango DJ app. It is not a toggleable "tango mode" that you switch off to get stock
Mixxx back. Internally, tango behaviour still keys off the `[AutoDJ],keep_queue`
control (used for gating and skin visibility), so that control is the hook to
trace when tango-specific behaviour is involved. Some of that plumbing predates
the repositioning and may be simplified in an upcoming refactor, so verify the
current flow in code rather than assuming.

## Hard rules

**Use clear language, avoid run on sentences.**



**Never launch `tangoq.exe`.** The user launches it. It has no
`WIN32_EXECUTABLE` property, so it is a console-subsystem binary and its
`qDebug` output arrives in the Claude Code window as mojibake. Redirecting both
streams with `Start-Process -RedirectStandardOutput/-RedirectStandardError` was
tried and still leaks — redirection is not a workaround. Build, run the tests,
then hand over with a specific list of what to check by hand.

**End every code-changing response with a status line**, on its own line, last:

- **Ready to test** — compiled and relinked, the binary on disk has the change.
- **Build and test** — still needs building.

`LNK1104: cannot open file 'tangoq.exe'` means the app is open. Ask the user
to close it; once it relinks, *tell them it is ready* rather than starting it.

**Skins load live from the source tree** (`res/skins/...`), because
`mixxx_SOURCE_DIR` is baked into `build/CMakeCache.txt`. Skin XML, QSS and SVG
edits need an **app restart, not a rebuild** — say so instead of asking for a
build. (`build/res` holds only shaders.)

## Build and test

Configured with Ninja, `RelWithDebInfo`, in `build/`:

```bash
cd build
cmake --build . --target mixxx-test --config RelWithDebInfo -- -j 2
./mixxx-test.exe --gtest_filter='*CueControl*:*Hotcue*:AutoDJ*:*AnalyzerSilence*'
cmake --build . --config RelWithDebInfo -- -j 2      # the app
```

- **Always `-- -j 2`.** Default parallelism hits `C1076: compiler limit:
  internal heap limit reached` under memory pressure.
- **`-DWARNINGS_FATAL=ON`** (already set in the cache). The CMake option
  defaults to OFF but CI passes it, so without it a local build happily
  compiles code that fails the Windows job.
- Pre-commit hooks do not fire locally. Scope them with `--from-ref`/`--to-ref`,
  **never** `--all-files`.
- Doc-only commits get `[skip ci]` — a docs push once cancelled an in-flight
  MSI build.

## Repo and branches

- `origin` = `github.com/seemantadutta/mixxx` (the fork), `upstream` =
  `mixxxdj/mixxx`.
- `master`/`main` and all upstream branches and tags are **mirrors of
  upstream**. Never merge feature work into them.
- **`tangomode`** is the long-lived integration branch, based on the `2.5.6`
  release tag, and is the fork's default branch.
- Feature work goes on topic branches → PR into `tangomode` → delete the topic
  branch. **The user opens and merges PRs**; provide the commit/PR text only.
- Commit or push only when asked.

Releases are GitHub Releases on `tangomode` tagged like `tango-2.5.6-v1`, with
unsigned installers attached and `RELEASE_NOTES.md` as the body. Download links
for users must point at **release assets** (permanent), never Actions artifacts
(they expire and need a login).

## How to work here

**Plan before implementing.** For anything beyond a small fix, write a plan and
get it approved before touching code. Features here are shaped through
discussion — the user has strong domain reasons that are not visible in the
code, and refines designs mid-flight. Stage large features so the visible,
low-risk part lands first and can be verified before anything touches
`AutoDJProcessor`'s transition logic.

**Validate each stage as it lands**, not all at the end. Say up front which
parts are automatically testable and which are not, then do both. Be explicit
about the gap: never let "I tested this stage" imply coverage of the parts that
can't be unit-tested. ControlObject-driven behaviour is testable
(`src/test/cuecontrol_test.cpp`, `src/test/autodjprocessor_test.cpp`); skin XML,
dialogs and painting are manual-only.

**Do not reason from code alone.** Wrong diagnoses here are usually resolved in
minutes by a test or by instrumenting, after hours of reading. Write the failing
test first, and trace a value to its *use*, not its assignment.

The feature set was frozen for a first release on **2026-07-26** and the project
is in a hardening phase. Default to deferring a new feature idea and capturing
it in `ROADMAP.md`/`prerelease-tasks.md` rather than building it, unless the user
says otherwise.

## Domain context

The user is a tango DJ who plays live milongas from pre-arranged sets. That
drives the design:

- A milonga set is an ordered list of **tandas** (groups of 3–4 tracks by one
  orchestra) separated by **cortinas** (short non-tango breaks). Nothing is
  random, nothing is deleted by playback, and the set stops at the end.
- **LIVE mode means a real performance.** Home testing is a separate mode of
  use, and features can legitimately exist in one and not the other. Destructive
  conveniences are deliberately hidden in LIVE mode. Ask which mode a feature is
  for rather than assuming a gate that blocks in LIVE is a bug.
- TangoQ is a dedicated tango app, not a mode the user toggles. There is no
  user-facing switch that returns it to stock Mixxx, so do not add tango on/off
  affordances or optimise for discovering one without asking. (Internally, tango
  behaviour still keys off `[AutoDJ],keep_queue`; see Codebase gotchas.)

## Codebase gotchas

**Silent no-ops.** Several things fail without an error:

- `ControlObject::set()` on a control that does not exist does nothing.
- `ControlProxy::valid()` checks the *key*, not existence.
- A skin `<Connection>` to a missing control is dropped without a warning
  (`legacyskinparser.cpp`).
- A second `<Mark>` on a control already seen is discarded with only a
  `qWarning` (`waveformmarkset.cpp`) — which is why the Tango start marker binds
  to a `tango_start_position` mirror rather than to `intro_start_position`.

**A ControlObject does not emit `valueChanged` for a change it made itself.**
`keep_queue` has a change-request handler, so every write to `keep_queue` lands
in `controlKeepQueueChangeRequest()` and `controlKeepQueue()` effectively never
runs. Anything that must track `keep_queue` belongs in the request handler.

**The Auto DJ model fully rebuilds on every edit.** `BaseSqlTableModel::select()`
emits a transient `rowsRemoved` (rowCount briefly 0) then `rowsInserted` with
everything — there are no granular deltas. Code reacting to those signals must
tolerate the transient empty state.

**Key controls.** `[AutoDJ],keep_queue` is the internal control that gates tango
behaviour; `[AutoDJ],keep_queue_off` is its inverse, which exists because a skin
`<VisibilityControl>` takes a single bare ConfigKey and cannot negate.

## Where things are written down

- `prerelease-tasks.md` — the live task list and design decisions, including the
  Tanda transition spec.
- `ROADMAP.md` — the fork's roadmap: completed work, planned features, known
  issues, and long-term ideas.
- `RELEASE_NOTES.md`, `INSTALL.md` — user-facing, for non-technical DJs.
- `ghostdeck-phase1-plan.md` and `tanda-insights.md` are untracked scratch files
  and unrelated to the current work — leave them alone.

## Debugging a crash

There is no debugger on this machine, but a full workflow exists: minidumps land
in `%LOCALAPPDATA%\CrashDumps\tangoq.exe.<pid>.dmp`, WER records are in the
Application event log under `Application Error` (Id 1000 gives faulting module +
offset, Id 1001 the bucket), and `dbghelp.dll` ships with Windows and can be
driven from Python via ctypes to symbolize. Match the PDB to the dump —
`build/tangoq.pdb` is overwritten on every relink, and a stale one resolves to
plausible-looking nonsense rather than failing.

A clean shutdown ends `mixxx.log` with `Mixxx shutdown complete with code 0`. If
Auto DJ rows come up greyed as played on the next run, the previous run did
**not** exit cleanly — `TrackDAO::finish()` only runs on the clean path, so play
state deliberately survives a crash.
