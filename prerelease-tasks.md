# Pre-release tasks

Things to close before cutting the first TangoMode release. Each entry records
what has already been established, so nothing has to be re-derived.

---

## 1. Windows settings directory is shared with stock Mixxx

**Status:** diagnosed but **not fixed**. Not a blocker for testing installers.

TangoMode on Windows stores its settings, log and library database in
`%LOCALAPPDATA%\Mixxx` — the same directory a stock Mixxx install uses. So the
two share one `mixxxdb.sqlite` and `mixxx.cfg`, and take turns migrating the
schema whenever their versions differ. macOS was fixed by branding the bundle
(`d4348e74fe`); Windows still needs it.

### Verified

- **Reproducible.** Launch the build and watch `%LOCALAPPDATA%\Mixxx` receive
  `soundconfig.xml` in real time while `%LOCALAPPDATA%\TangoMode` stays empty
  (it exists, created 25 Jul, zero files).
- **Not `MIXXX_SETTINGS_PATH`.** `build/src/config.h` shows it undefined on
  Windows; CMakeLists only sets it for `UNIX AND NOT APPLE`.
- **Not the macOS migration.** `setSettingsPath(Sandbox::migrateOldSettings())`
  is inside `#ifdef Q_OS_MACOS`.
- **Not `CmdlineArgs`' constructor default.** Its `#else` branch was changed to
  build the path explicitly from `VersionStore::applicationName()`; the object
  recompiled and the app still wrote to `Mixxx`. The change was reverted rather
  than left in.
- `VersionStore::applicationName()` **does** return `"TangoMode"`
  (`versionstore.cpp:38`), and `main.cpp:207` passes it to
  `setApplicationName()` before `CmdlineArgs::Instance()` at `:211`. Note
  `MixxxApplication` is not constructed until `:231`, so all of this happens
  with no `QCoreApplication` in existence — `CmdlineArgs::parse()` even asserts
  that (`DEBUG_ASSERT(!QCoreApplication::instance())`).

### Next step — instrument, do not guess

Two wrong hypotheses have already cost time here. Print the path at each stage
and read the log rather than reasoning about it:

1. `CmdlineArgs`' constructor — what it computes
2. `CmdlineArgs::getSettingsPath()` — what it returns when `CoreServices` asks
   (`coreservices.cpp:425`)
3. what `SettingsManager` receives
4. what `Upgrade::versionUpgrade(settingsPath)` returns — the current prime
   suspect, since upgrade logic is exactly the kind of code that relocates
   settings between versions

One build-and-launch cycle should settle it.

### When fixing

Whatever the cause, prefer deriving the directory from
`VersionStore::applicationName()` over relying on `QStandardPaths`' implicit
naming, which is evidently sensitive to initialisation order.

**Consequence to accept:** existing Windows users' settings stay behind in
`Mixxx` and they start fresh in `TangoMode` — the same trade-off already taken
on macOS, and much cheaper before there are users.

---

## 2. Honour an explicit Intro Start cue in Skip Silence mode

**Status:** understood, not implemented. Small and contained.

**Want:** set a cue a few seconds into a track you know, and have Auto DJ start
that track there on the transition.

### What stock Mixxx already does

`calculateTransition()` reads the **Intro Start** cue and uses it as the
incoming track's start position:

```cpp
const double introStart = getIntroStartSecond(pToDeck);   // ~line 2536
...
toDeckStartSeconds = introStart;                          // -> pToDeck->startPos
```

| Transition mode | Where the incoming track starts |
|---|---|
| **Full Intro + Outro** | Intro Start cue ✔ |
| **Fade at outro start** | Intro Start cue ✔ |
| **Skip Silence** | `getFirstSoundSecond()` — the analysed `N60dBSound` cue ✘ |
| **Full Track** | 0.0 ✘ |

So the feature exists, but only in the intro/outro modes. Skip Silence — the
mode Tango sets as its default — never consults the cue.

Switching mode is not the answer: intro/outro modes crossfade over the
intro/outro length, whereas a tango set wants a clean gap between tracks.

### Proposed change

In the `FixedSkipSilence` branch of `calculateTransition()`
(`autodjprocessor.cpp` ~2665), prefer an explicit Intro Start cue when the track
has one, and fall back to first-sound when it does not:

```cpp
const double introStart = getIntroStartSecond(pToDeck);
toDeckStartSecond = introStart > 0.0 ? introStart : getFirstSoundSecond(pToDeck);
```

Points to settle while implementing:

- **How is "no cue set" represented?** `getIntroStartSecond()` needs checking —
  an unset cue may come back as 0, as the track start, or as invalid. The
  condition above assumes 0/absent; confirm before relying on it.
- **Interaction with the cortina envelope.** `slotCortinaGapElapsed()` cues the
  next track with `getFirstSoundSecond()` directly, so a cue on a post-cortina
  track would still be ignored unless that path is changed too. Decide whether
  it should be.
- **Scope.** Gate behind `keepQueueEnabled()` if it should be Tango-only, or
  leave it general — arguably it is a plain improvement to Skip Silence and a
  candidate to send upstream.
- **Tests.** `autodjprocessor_test.cpp` already covers Skip Silence; add a case
  for "cue set" and one for "no cue set, still trims silence".

---

## 3. Already known, not blocking

- **Flatpak CI fails.** Pre-existing and unrelated to the `.msi` / `.dmg`. A
  soundtouch cherry-pick (`4f35d26249`) is outstanding for it.
- **README fork notice.** Done, but the repo is still presented as a Mixxx fork
  in places outside README/INSTALL if anyone looks closely.
- **User documentation.** Still unwritten. The vocabulary is now small — cortina,
  pause mark, display name, LIVE mode, Set End Time — so this is a page, not a
  manual.
