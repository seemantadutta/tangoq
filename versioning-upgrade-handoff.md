# Handoff: TangoQ version/upgrade-path bug (settings reset on every upgrade)

Status: superseded by the schema-based implementation on
`fix/tangoq-config-versioning`. TangoQ now uses
`[Config] TangoQConfigVersion`, starting at 1, and never routes `tangoq.cfg`
through the inherited Mixxx configuration ladder. Missing and malformed schemas
enter through schema 0 so future migrations cannot be skipped. A configuration
from a newer schema is protected from saving and prevents normal startup.

On macOS, TangoQ now only calculates its own container path. It no longer moves
the stock Mixxx settings directory. The separate database importer remains an
explicit copy operation.

This is a design-and-fix task for **TangoQ**, a Mixxx fork
(`/Users/seemanta/Projects/mixxx`, app bundle `TangoQ.app`, bundle id
`io.github.seemantadutta.tangoq`). Read this whole doc before touching code. Do
NOT start coding on sight; confirm scope with the user first, because the change
runs against real users' config files.

## The problem in plain terms

TangoQ shows its own product version, `1.0.x` (from `TANGOQ_VERSION` /
`VersionStore::forkVersion()`). But the old upgrade code still thought in Mixxx's version
numbers, which are `2.x`. Because `1.0.1` is numerically **less than** `2.4.0`,
the upgrade code believes the user is on a very old pre-2.4 Mixxx and re-runs a
Mixxx 2.4 migration **every time the user upgrades from one TangoQ version to the
next**. That migration silently **overwrites two of the user's settings**:

- `[Waveform] WaveformType` (rewritten via `upgradeToAllShaders(...)`)
- `[Waveform] FrameRate` (forced to 60)

So every TangoQ-to-TangoQ upgrade quietly resets the user's waveform style and
frame rate. It is an annoyance, not data loss (no tandas, cues, or library are
touched), but it is wrong and it repeats on every upgrade.

## Exactly where it happens

`src/preferences/upgrade.cpp`, function that upgrades the config object:

- Line 332: reads `configVersion` from `[Config] Version`.
- Line 366: if `configVersion == VersionStore::version()` it logs "at current
  version" (the no-upgrade path). On a real upgrade the stored version differs,
  so it falls through.
- Lines 375-566: the legacy Mixxx ladder (`1.6.0`, `1.7`, `1.8`, ... `1.11`).
  TangoQ configs (`1.0.1`) do not match any of these `startsWith` checks, so
  they skip straight through.
- **Lines 572-593: the offending block.**

  ```cpp
  if (QVersionNumber::fromString(configVersion) < QVersionNumber(2, 4, 0) || ...) {
      // moves user to an all-shader waveform + sets 60 fps
      config->set("[Waveform]","WaveformType", upgradeToAllShaders(...));
      config->set("[Waveform]","FrameRate", 60);
      configVersion = "2.4.0";
      config->set("[Config]","Version", "2.4.0");
  }
  ```

  `1.0.1 < 2.4.0` is true, so this fires and resets the two settings.
- Lines 598-603: since `configVersion` is now `2.4.0` (>= cleanVersion 2.4.0), it
  stamps `[Config] Version` with the current product version (e.g. `1.0.2`).

Net effect: after a `1.0.1 -> 1.0.2` upgrade the config holds `Version = 1.0.2`,
which is **again** `< 2.4.0`, so the next upgrade (`1.0.2 -> 1.0.3`) fires the
same reset. It never stops.

## Why this matters right now

The user is moving all 23 early-access DJs to `1.0.2`. On macOS, **uninstalling
the app does not clear the sandbox container**, so each user's existing
`tangoq.cfg` (with `Version = 1.0.1`) survives the reinstall. First launch of
`1.0.2` reads that stored `1.0.1`, fires the block, and resets their waveform and
frame-rate settings. "Uninstall and reinstall" does not avoid it.

## What is NOT affected (settled facts)

- **DB schema versioning is independent of all this.** The DB schema is a
  monotonic integer, `MixxxDb::kRequiredSchemaVersion` (currently 39, in
  `src/database/mixxxdb.cpp`), driven by `res/schema.xml` revisions. It has no
  connection to `[Config] Version` or the product version string. You can bump
  DB schema freely regardless of what is done here. (Separate, unrelated future
  item: give fork schema revisions an offset like 1000 so they cannot collide
  with upstream Mixxx's increments on a rebase. Not part of this task.)

## Options considered

### Option C (a tempting shortcut the user asked about): pin the product version to "2.5.6" forever

If the reported version were `2.5.6`, new configs would sort above `2.4.0` and
stop re-firing. **Rejected as the primary fix** because:

- It does NOT save the 23 current users: their stored value is `1.0.1`, read
  before any new version is written, so the block still fires once on the move to
  1.0.2.
- It freezes the user-facing version at "2.5.6" forever, so DJs can no longer
  tell 1.0.2 from 1.0.3 from the About dialog or title bar. The whole point of a
  `1.0.x` product version is so the user can tell people "install 1.0.2."
- It deepens coupling to Mixxx's number line (the opposite of the intended
  direction) and is fragile the day TangoQ rebases onto Mixxx 2.6 / 3.0 with new
  thresholds.

It does have one true property worth noting: it converts a *perpetual* re-fire
into an at-most-once re-fire. But the recommended option below achieves that too,
without the costs.

### Option B (recommended for 1.0.2): stop routing TangoQ configs through the pre-2.4 Mixxx ladder

This fork is always based on Mixxx 2.5.6, which is already past the 2.4
all-shader migration. No genuine pre-2.4 config exists in the TangoQ user base.
So the 2.4 block (and the whole legacy ladder) should simply not apply to a
TangoQ config.

Two ways to detect "this is a TangoQ config" that do NOT rely on the version
string (important, because the version string `1.0.1` is exactly what looks
pre-2.4):

1. **Fingerprint a TangoQ-only key.** A real TangoQ config always has
   tango-specific keys that stock Mixxx never writes, e.g. `[AutoDJ] TangoEndTime`,
   `[AutoDJ] CortinaFadeMode`, `[AutoDJ] CortinaLength`, `[AutoDJ] TandaGap`. If
   any is present, treat the config as already clean and skip the legacy ladder
   (including the 572 block). This also **saves the 23 current users from even
   the one-time reset**, because detection is independent of their `1.0.1`
   string.
2. **Unconditional, since the binary is definitionally TangoQ.** Because this
   codebase only ever ships as TangoQ on a 2.5.6 base, the pre-2.4 migrations are
   dead weight. Guard them out entirely. New users (no config) are already
   handled by the empty-config path near line 334, which just stamps the current
   version. Simpler, but relies on "no user will ever import a genuinely ancient
   Mixxx config," which is safe for this fork.

Prefer approach 1 (fingerprint) if you want to be conservative and still protect
brand-new-from-old-Mixxx edge cases; approach 2 if the user wants the smallest
possible diff.

Either way, keep the displayed product version at `1.0.x`. The implemented split
uses `VersionStore::forkVersion()` for the TangoQ product and retains
`VersionStore::version()` for the Mixxx base version.

### Option A (the eventual clean architecture, defer past 1.0.2)

The ROADMAP item "Version & upgrade-path decoupling from Mixxx" describes the
full solution: introduce a TangoQ-owned config-schema counter (e.g.
`[Config] TangoQConfigVersion`) separate from the displayed product version, run
only TangoQ migrations off that counter, and stop reasoning about the Mixxx
number line at all. This is more work and more risk and is not needed for 1.0.2.
Option B is a strict subset of it and is forward-compatible with it.

## How to do it safely (test-first, non-negotiable)

The migration runs against real users' configs, so write the failing test
BEFORE changing logic:

1. Find the existing upgrade/preferences tests (look under `src/test/` for an
   `upgrade`-related test; if none, add one). Construct a `ConfigObject` in
   memory with `[Config] Version = 1.0.1` and a set `[Waveform] WaveformType`
   and `FrameRate`, run the upgrade, and assert (currently failing) that
   `WaveformType`/`FrameRate` are **unchanged**. Also cover:
   - a config carrying `Version = 2.5.6` (some early-access users have this from
     an old shared-settings bug) stays unchanged,
   - a brand-new empty config still gets stamped with the current version,
   - after upgrade, a second upgrade does not re-fire (idempotent).
2. Then implement Option B and watch the test go green.
3. Confirm the displayed version is still `1.0.x` and the DB schema path is
   untouched.

## Build / run / verify notes (this fork is unusual)

- **8 GB Apple-silicon MacBook.** Build with Ninja, always `-j 2`:
  `/opt/homebrew/bin/ninja -C build -j 2 mixxx` (app) or `... mixxx-test` (tests).
  `cmake` is not on PATH.
- **Run the tests with `QT_QPA_PLATFORM=offscreen`** on macOS or they hang
  forever in teardown. This bug IS unit-testable (config in/out), so lean on a
  test rather than manual runs.
- **Never launch the app yourself.** The user launches `TangoQ.app` from the
  Dock; console output is mojibake anyway. For any manual check, install with
  `./tools/tangoq_build_macos.sh install` (run from repo root; it copies into
  `~/Applications/TangoQ.app`, no compile) and hand the user a checklist.
- **Config file:**
  `~/Library/Containers/io.github.seemantadutta.tangoq/Data/Library/Application Support/TangoQ/tangoq.cfg`.
  `[Config] Version` is the field in question. It is held in memory and flushed
  to disk only on a clean quit. There is a backup from an earlier session at
  `tangoq.cfg.bak.hcdebug` if you need a real-world sample.
- **Log** (same directory, `tangoq.log`) is buffered and flushed on clean quit;
  it reads 0 bytes while running. `qWarning`/`qDebug` land there.
- There is a companion doc `highcontrast-waveform-handoff.md` in this repo with
  more environment detail (same machine, same rules). Note: that doc mentions the
  user's `WaveformType` is 17. Be aware this very bug can reset that value on
  upgrade, which is a nice real-world confirmation of the problem.

## Repo conventions

- Do this on its own branch off `main` (the protected trunk; never target
  `tangomode` or `master`). Keep the diff small and reviewable.
- **The user opens and merges PRs.** Provide commit/PR text; do not push or open
  PRs unless asked.
- No `Co-Authored-By` trailer on commits. Avoid em-dashes in user-facing copy.
- The authoritative description of this item lives in `ROADMAP.md` under
  "Version & upgrade-path decoupling from Mixxx"; keep it in sync if you change
  the plan.
