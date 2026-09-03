# TangoQ configuration migration plan

Status: implemented on `fix/tangoq-config-versioning`, pending review

Target: the first TangoQ release that ships this work, currently planned as
1.0.2

Related visualization:
[`tangoq-versioning-migration-flow.dot`](tangoq-versioning-migration-flow.dot)

## Goal

Separate TangoQ's configuration migration history from Mixxx's product-version
history. This prevents TangoQ upgrades from interpreting a product version such
as `1.0.1` as an ancient Mixxx version and rerunning the Mixxx 2.4 waveform
migration.

The current bug changes two user settings during each TangoQ product upgrade:

- `[Waveform] WaveformType` is passed through the old all-shader conversion.
- `[Waveform] FrameRate` is set to `60`.

This work must preserve an existing DJ's settings, introduce a clean foundation
for future TangoQ configuration migrations, and remain independent of library
database migration.

## Decisions

1. `[Config] Version` remains the TangoQ product version, for example `1.0.2`.
   It is useful for provenance and display, but it must not select configuration
   migrations.
2. Add an integer `[Config] TangoQConfigVersion`. Its first value is `1`.
3. Any existing non-empty `tangoq.cfg` without the new key is adopted as TangoQ
   configuration schema 1. Adoption does not apply first-run defaults or any
   inherited Mixxx version-threshold migration.
4. A genuinely empty configuration receives the existing TangoQ first-run
   defaults, the current product version, and `TangoQConfigVersion = 1`.
5. Future TangoQ configuration changes use explicit, sequential integer
   migrations. A product release that does not change the configuration
   structure leaves `TangoQConfigVersion` unchanged.
6. Do not identify TangoQ configurations by looking for TangoQ-specific setting
   keys. Early builds did not necessarily write every such key, making a
   fingerprint unreliable.
7. The inherited Mixxx configuration migration ladder must not run for files in
   TangoQ's settings directory. This includes the pre-1.7 home-directory import
   at the beginning of `Upgrade::versionUpgrade()`, which must not move a stock
   Mixxx configuration into TangoQ.
8. Database schema behavior is out of scope. Do not change
   `MixxxDb::kRequiredSchemaVersion`, `res/schema.xml`, the Mixxx database import
   prompt, or any library database migration code in this PR.

## Version responsibilities

| Value | Example | Responsibility |
| --- | --- | --- |
| TangoQ product version | `1.0.2` | Release identity shown to users |
| TangoQ configuration schema | `1` | Selects TangoQ settings migrations |
| Mixxx database schema | `39` | Describes the inherited library database structure |

These values are independent. In particular, the product version must never be
compared with a Mixxx migration threshold.

## Required behavior by user path

| Starting state | Required first launch behavior |
| --- | --- |
| No `tangoq.cfg` | Apply TangoQ first-run defaults; write the current product version and config schema 1 |
| TangoQ 1.0.1 config, unchanged defaults | Preserve every existing setting; adopt config schema 1; update the product version |
| TangoQ 1.0.1 config with DJ customizations | Preserve the custom waveform type, frame rate, and all other settings; adopt config schema 1; update the product version |
| Early-access config carrying product version 2.5.6 | Preserve settings; adopt config schema 1; update the product version |
| Current product version but missing the schema key | Still adopt config schema 1; the product-version equality shortcut must not skip adoption |
| Existing config schema 1 from an older TangoQ release | Run no config migration; update only the product version |
| Existing config with a future schema greater than this binary supports | Preserve the file byte-for-byte, suppress all settings saves, explain the incompatibility, and exit before normal startup |

Running the same release again must be idempotent: no first-run defaults, legacy
migrations, or setting changes may be applied on the second launch.

## Proposed control flow

Define the current TangoQ configuration schema as a named integer constant in
the preferences upgrade implementation. Keep the key name and current value in
one place so future migrations cannot drift apart.

`Upgrade::versionUpgrade()` should follow this order:

1. Open `tangoq.cfg` directly from the supplied TangoQ settings path. Do not
   inspect, copy, move, or remove legacy Mixxx configuration files.
2. Read `[Config] Version` and determine whether this is a genuinely empty
   configuration.
3. For an empty configuration:
   - apply TangoQ first-run defaults;
   - write the current product version;
   - write `TangoQConfigVersion = 1`;
   - set `m_bFirstRun` and return.
4. For an existing configuration, inspect `TangoQConfigVersion` before any
   product-version early return:
   - missing: enter as schema 0 and sequentially adopt schema 1 without changing user settings;
   - supported older value: run each TangoQ migration in sequence;
   - current value: run no migration;
   - future value: preserve it, warn, suppress later saves, and stop startup
     before the database or main window is initialized.
5. After successful adoption or supported migrations, stamp `[Config] Version`
   with `VersionStore::forkVersion()`.
6. Return the configuration without entering the inherited Mixxx version ladder.

A malformed or negative schema value is logged explicitly and treated as schema
0. It then passes through the same sequential adoption path as a missing schema.
It must never be stamped directly with the current schema because doing that
would skip future migration steps.

The existing always-on VSync compatibility normalization is not the migration
bug addressed here. Keep or refactor it only as needed to preserve current
behavior; do not combine unrelated setting changes with schema adoption.

## Implementation scope

Expected implementation files:

- `src/preferences/upgrade.cpp`
- `src/preferences/upgrade.h`, only if declarations become unnecessary or a
  small testable helper is introduced
- `src/preferences/settingsmanager.*`, `src/coreservices.*`, and `src/main.cpp`
  to reject and protect future-schema files
- `src/util/sandbox.*` to calculate TangoQ's macOS path without moving Mixxx
  settings
- a new focused upgrade test under `src/test/`
- `CMakeLists.txt` to register the new test source

The inherited legacy ladder may be removed or isolated behind an unreachable
path. The acceptance requirement is behavioral: no TangoQ configuration may
enter it, and the pre-1.7 code may not move files from the user's Mixxx paths.
Deleting code that becomes unused, including obsolete prompts and helper
methods, is acceptable within this focused cleanup.

The product version bump to 1.0.2 is release preparation rather than migration
selection. Tests and migration logic must use `VersionStore::forkVersion()` for the
current product value and must not encode 1.0.2 as a migration threshold.

## Test-first matrix

Add a focused `UpgradeTest` using a temporary settings directory and a real
temporary `tangoq.cfg`. Write the regression test before changing the upgrade
logic.

Required tests:

1. `1.0.1` with a customized `WaveformType` and non-60 `FrameRate` retains both
   values, gains config schema 1, and receives the current product version.
2. `1.0.1` without custom TangoQ values is adopted without applying first-run
   defaults.
3. `2.5.6` is adopted without changing settings.
4. An existing configuration whose product version already equals
   `VersionStore::forkVersion()` still gains a missing config schema key.
5. A fresh empty configuration gets first-run defaults, the current product
   version, config schema 1, and `isFirstRun() == true`.
6. An existing schema-1 configuration from an older product release changes
   only the product version.
7. Save, reload, and run the upgrade a second time; all settings and schema
   values remain unchanged.
8. A future schema value is not downgraded or overwritten, and remains
   byte-for-byte unchanged after `SettingsManager::save()` and destruction.
9. Malformed and negative schema values follow the schema-0 adoption path and
   emit no legacy Mixxx migration effects.
10. macOS settings-path resolution leaves a neighboring Mixxx directory
    untouched and creates no destination as a side effect.

Where practical, include a sentinel setting outside the waveform group and
assert that it is unchanged. Also assert that an adopted existing configuration
does not receive an absent first-run-only skin or Auto DJ default.

## Verification

On the development Mac, build with limited parallelism:

```sh
/opt/homebrew/bin/ninja -C build -j 2 mixxx-test
```

Run the focused suite offscreen:

```sh
QT_QPA_PLATFORM=offscreen ./build/mixxx-test \
    '--gtest_filter=UpgradeTest.*'
```

Then run formatting and repository checks for the branch diff. Do not launch the
application automatically. If a manual release check is desired, the user can
install and launch TangoQ, then verify a backed-up copy of a customized config:

- the customized waveform type and frame rate remain unchanged;
- `[Config] Version` is the current product version;
- `[Config] TangoQConfigVersion` is `1`;
- a clean quit persists those values;
- the next launch makes no further changes.
- a future-schema file produces a clear incompatibility message and is not
  rewritten;
- a stock Mixxx settings directory remains in place and unchanged.

## Field diagnostics

File logging must be initialized from the resolved TangoQ settings path before
`SettingsManager` reads or migrates `tangoq.cfg`. Otherwise the most useful
startup migration messages reach only the terminal and are absent from a field
report's `tangoq.log`.

Every migration attempt writes one concise informational summary in this form:

```text
TangoQ config migration: appProduct=1.0.2 sourceProduct=1.0.1 foundSchema=<missing> supportedSchema=1 outcome=adopted
```

The supported outcomes are `fresh`, `adopted`, `unchanged`, `migrated`,
`rejected-newer`, and `migration-unavailable`. Rejections, invalid schemas, and
suppressed saves are warnings. Individual migration steps and the configuration
path are debug messages and are written only when debug logging is enabled. Do
not log individual settings or their values.

For a normal field report on macOS, ask the user to reproduce the problem, quit
TangoQ, and attach:

```text
~/Library/Containers/io.github.seemantadutta.tangoq/Data/Library/Application Support/TangoQ/tangoq.log
```

For an on-demand detailed report, first quit TangoQ and launch it from Terminal:

```sh
"/Applications/TangoQ.app/Contents/MacOS/TangoQ" \
    --log-level debug \
    --log-flush-level debug
```

The debug flush level is important for startup failures because it persists each
message promptly. `tangoq.log` is the current session and numbered files are
rotated earlier sessions. Warn users that a complete debug log may contain
usernames, music-file paths, audio-device names, and controller information even
though the migration summary itself contains no setting values.

The focused unit tests verify migration outcomes and file preservation without
depending on exact log wording. An application-level smoke test should use a
temporary settings path, enable debug logging and flushing, confirm the expected
summary appears in `tangoq.log`, and confirm a rejected future-schema config has
the same checksum before and after startup.

## Database policy and deferred work

TangoQ currently shares Mixxx's database-schema counter. A copied database from
a newer Mixxx release is already accepted when that release marks its schema as
backward-compatible with TangoQ's supported schema. An incompatible structural
change must be deliberately ported; assigning TangoQ an arbitrarily high shared
number would not make the structure compatible.

Before TangoQ introduces its first database-specific structural change, create a
separate design and PR with two migration lanes:

- `mixxx.schema.version` continues to describe the last upstream Mixxx database
  schema understood by TangoQ;
- a TangoQ-owned setting such as `tangoq.schema.version`, starting at 1, selects
  TangoQ-only database migrations.

That avoids collisions without requiring hundreds of placeholder revisions or
changing database behavior in this configuration fix.

## Acceptance criteria

- Upgrading an existing TangoQ configuration does not change the DJ's waveform
  type, frame rate, or unrelated settings.
- Fresh installs still receive TangoQ defaults.
- Every successfully handled config has product provenance and config schema 1.
- Repeated launches are idempotent.
- TangoQ does not execute or import inherited Mixxx configuration migrations.
- Product-version comparisons no longer select configuration migrations.
- Migration decisions are captured in `tangoq.log` from the start of settings
  initialization.
- No database file, database schema constant, or schema XML revision changes.
- Focused tests and branch formatting checks pass.

## Delivery

The implementation is on the dedicated `fix/tangoq-config-versioning` branch,
rebased on the latest `main`. Keep it as a separate PR. The user will review,
push, open, and merge according to the repository workflow.
