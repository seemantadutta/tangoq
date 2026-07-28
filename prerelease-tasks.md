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

### Step zero — the reproduction is now two-way

Renaming `%LOCALAPPDATA%\Mixxx` aside and relaunching **created
`%LOCALAPPDATA%\TangoMode`**. So the path logic is capable of producing the right
answer; an existing `Mixxx` directory somehow wins over it. Renaming the folder
back should restore the wrong behaviour, giving a clean A/B to instrument
against.

**This is not the all-clear it looks like.** That `Mixxx` folder came from this
machine's own pre-rename TangoMode builds — but a user who has never run
TangoMode and simply has **stock Mixxx installed** has the same folder. If the
mechanism keys on the directory existing, that user gets the shared settings and
database this task exists to prevent. The experiment cannot distinguish the two,
so it does not close the question.

Already ruled out: `upgrade.cpp` has exactly two `setSettingsPath()` redirects,
`~/.mixxx/` (macOS, pre-1.9) and `~/Local Settings/Application Data/Mixxx/`
(Windows, pre-1.12). Neither is `%LOCALAPPDATA%\Mixxx`.

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

## 2. New transition mode: **Tanda Transition**

**Status:** specified, not implemented. Roughly half a day, plus one unresolved
design question (see "The gap problem" below) that could make it longer.

A fifth transition mode alongside the four stock ones, rather than changing what
"Skip Silence" means. Two reasons:

1. **Tango off must stay stock Mixxx, byte for byte.** Redefining Skip Silence
   would change behaviour for someone using that mode with Tango off who happens
   to have an intro cue set.
2. **It gives DJs something nameable.** The cue is only the first thing this mode
   wants; the gap rule, no-crossfade and cortina awareness belong with it. Once
   the mode exists those have a home instead of accreting as special cases inside
   a stock mode until nobody can say what "Skip Silence" means in this build.

### The behaviour, as it should be documented for DJs

> **Tanda Transition** — the next track starts at your **Intro Start cue** if you
> set one, otherwise at its first audible sound. The outgoing track plays through
> to its last audible sound. Between them is a gap of the number of seconds in the
> box. Tracks are never crossfaded into each other. Cortinas keep their own fade
> envelope when Cortina Fade is on.

Every clause is verifiable by ear, which is the test of whether it is worth
documenting.

### The seconds box means the opposite of stock

In stock Mixxx the value is a **crossfade** length and a **negative** value means
a gap. In Tanda Transition a **positive** value is the gap, and **overlap is not
possible at all** — by design, since two tango tracks should never be played over
one another.

- The spinbox minimum becomes **0** while this mode is selected, and the label
  should read *Gap* rather than *Transition*.
- **Migration:** `[Auto DJ] Transition` is shared across modes. Switching from
  Skip Silence to Tanda should take `abs()` of the stored value — a DJ on `-3`
  wanted a 3 second gap and gets a 3 second gap, the same audible result.

### The gap problem — settle this first

The gap in stock Skip Silence is **not a pause**. `useFixedFadeTime()`
(`autodjprocessor.cpp:2786-2790`) produces it by cueing the incoming track
*backwards into its leading silence*:

```cpp
pFromDeck->fadeBeginPos = fadeEndSecond;
pFromDeck->fadeEndPos = fadeEndSecond;                     // hard cut
pToDeck->startPos = toDeckStartSecond + m_transitionTime;  // negative -> earlier
```

That silently conflicts with the whole point of the cue: if the DJ set a cue to
skip the first few seconds, starting the deck `gap` seconds before it replays
exactly what they asked to skip — and if the cue is near the start of the file,
there may not even be that much room.

Three ways out, to choose between:

1. **Delay the start instead of cueing earlier.** True silence, works with any
   cue. Needs a timer, which the cortina envelope already has in
   `m_cortinaGapTimer` / `slotCortinaGapElapsed()` — generalising that is the
   real work in this task, and the reason the estimate could grow.
2. **Cue back only as far as there is silence**, clamped so it never crosses the
   cue. Cheap, but the gap silently shortens on some tracks, which makes the
   documented behaviour a lie.
3. **Only honour the cue when no gap is configured.** Cheapest, and the worst —
   two settings that quietly cancel each other are exactly the kind of thing
   this mode exists to avoid.

Option 1 is the one that matches the documented sentence. Decide before writing
code.

### Where the code has to change

Six sites, all mechanical apart from the above:

| Site | Change |
|---|---|
| `autodjprocessor.h:169` enum | **append** one value — see the trap below |
| `dlgautodj.cpp:224` | one `addItem` for the dropdown |
| `calculateTransition()` | new `case`, starting from the `FixedSkipSilence` branch |
| 3 set-length conditions | they test `FixedFullTrack \|\| FixedSkipSilence`, or `== FixedSkipSilence` for the audible-range refinement |
| `dlgautodj.cpp:773`, `upgrade.cpp:84` | the Tango default becomes the new mode |
| `autodjprocessor_test.cpp` | a case with a cue, one without, one for the gap |

**Enum ordering trap.** The mode is persisted as a raw enum index —
`upgrade.cpp:84` literally writes `TransitionMode = "3"` for Skip Silence. The
new value **must be appended** at the end. Inserting it anywhere else silently
changes the saved mode of every existing user.

**Set Length sign flip.** `recomputeKeepQueueUpcomingDuration()` currently does
`seconds -= transitions * m_transitionTime`, which *adds* time because the value
is negative. With a positive gap it becomes `seconds += transitions * gap`. Get
this wrong and Set Length — a headline feature — is quietly wrong.

### In Tango mode, this is the *only* mode

The dropdown is restricted to **Tanda Transition** while Tango DJ Mode is on, and
the full stock list returns when it is off. Consistent with the rest of the Tango
lockdown (toolbar controls, column sorting, the queue-reordering actions), and it
removes a large amount of interaction surface: cortinas, pause marks and the set
timing then only have to be correct under one transition mode instead of four.

Today the code merely *nudges* — `dlgautodj.cpp:773` switches Full Intro + Outro
to Skip Silence when Tango is enabled at factory defaults, and leaves the
dropdown fully usable. That becomes a restriction instead.

Prefer **repopulating** the combo box with the single item over greying out a
four-item list: the DJ sees the mode they are in and no alternatives, which is
self-documenting. This is safe because `slotTransitionModeChanged()` reads
`itemData(index)` rather than assuming index equals the enum value
(`dlgautodj.cpp:482`), so the indices moving does not matter.

Consequences, both intended:

- **Enabling Tango now changes the transition mode unconditionally**, including
  for existing Tango users currently on Skip Silence. That is the right
  behaviour once Tango owns the transition — but it means the migration rule
  above (`abs()` of the stored gap) is doing real work, not just tidying.
- **Wanting a stock transition means turning Tango off.** Acceptable: the modes
  it hides all crossfade tracks into one another, which a milonga set should
  never do.
- `kTangoGapSeconds = -2` at `dlgautodj.cpp:769` becomes `+2` under the new sign
  convention.

### Cortinas honour the cue too — decided

A cortina often has a lead-in worth skipping. Setting a cue 11 s in must make the
**Cortina Fade envelope start there**: the silent lead-in is taken relative to the
cue, and the fade-in begins at the cue, not at the analysed first sound.

This is the part that makes the task structural rather than a one-line addition.
`getFirstSoundSecond()` — "where does this track's audio begin" — has **seven**
call sites, and the cortina envelope depends on three of them:

| Line | Role |
|---|---|
| 1561, 1600 | inside `maybeHandleCortinaFade()` — the envelope's reference point for the fade-in and phase timing |
| 1751 | `slotCortinaGapElapsed()` — cueing the *next* track after the cortina |
| 2322 | another consumer; check what it does before touching it |
| 2390 | the definition itself |
| 2673 | the stock `FixedSkipSilence` branch — **must not change** |
| 2720 | the cortina's own start: `getFirstSoundSecond(pToDeck) - m_cortinaGapSeconds` |

Rather than patch each one, introduce a single notion of where a track's *useful*
audio begins:

```cpp
// The DJ's cue wins over analysis: they know the track.
double AutoDJProcessor::audibleStartSecond(DeckAttributes* pDeck) {
    const double cue = getIntroStartSecond(pDeck);
    return cueIsSet(cue) ? cue : getFirstSoundSecond(pDeck);
}
```

Then the cortina case falls out with no special handling — pre-roll becomes
`audibleStart - Nc`, and the envelope references `audibleStart`.

**Route only the Tango-gated sites through it.** Line 2673 is the stock Skip
Silence branch; sending that through a cue-aware helper would change stock
behaviour, which is the thing this whole design is avoiding. The cortina paths
are already behind `m_cortinaFadeEnabled && keepQueueEnabled()`, and the new
Tanda case is Tango-only by construction, so those are safe.

**Keep the negative pre-roll.** The comment at 2711-2716 explains that
`startPos` is deliberately *not* clamped to `>= 0`: a hot-start or un-analysed
cortina gets synthetic silence pre-rolled before 0:00, which is what stops the
onset reaching the output at full crossfader. With a cue 11 s in there is plenty
of room and the value stays positive, but both cases still have to work.

### Open decisions

- **How is "no cue set" represented?** `getIntroStartSecond()` may return 0, the
  track start, or an invalid position. Confirm before writing `cueIsSet()` — the
  whole design rests on distinguishing "cue at 0:00" from "no cue".
- **Re-check the cortina envelope under the new mode.** Cortina Fade currently
  has to work across whichever transition mode is selected. Once Tango pins the
  mode, its interaction is with Tanda Transition only — a simplification, but the
  envelope tests (G1, G2, P2) must be re-run against it rather than assumed,
  along with a new case: a cortina with a cue, confirming the fade-in starts
  there and the gap before it is still true silence.

---

## 3. Mixxx branding in user-visible strings

**Status:** audited, not started. Cheap if scoped; a trap if done as a
find-and-replace.

Dialogs and messages still say "Mixxx" where they mean this application. The
surface is roughly **60 translatable strings** in `.cpp` plus **11 `.ui` files**.

### Three kinds of string — only one should change

| Kind | Example | Change? |
|---|---|---|
| The app talking about itself | *"A deck is currently playing. Exit Mixxx?"* (`mixxxmainwindow.cpp:1568`) | **Yes** |
| Pointing at the upstream project | manual and wiki links, `mixxx.org`, the About credits, "Mixxx DJ Hardware Guide" | **No** |
| Historical or technical | *"Mixxx versions before 1.11"*, `MixxxControl`, "Mixxx Effect Chain Presets" as a file-format name | **No** |

A blanket replace would rewrite the second group, which is factually wrong and
undercuts the trademark position taken deliberately in README and INSTALL — this
build is careful to say what is Mixxx's and what is not. It would also break
every translation and balloon the diff against upstream.

### Recommended scope for the release

Fix only the first group, ordered by how often a DJ actually sees it. Roughly
10-15 strings covering nearly all real exposure:

- `mixxxmainwindow.cpp` — exit confirmation (1568), the sound-device failure
  dialog and its buttons (727-753), the menu-bar prompt (637)
- `database/mixxxdb.cpp:94` — "Try renaming it and restarting Mixxx"
- `library/scanner/libraryscannerdlg.cpp:19` — the library-scan wait message
- `engine/sidechain/shoutconnection.cpp:81,86` — "Mixxx encountered a problem"
- `library/browse/browsetablemodel.cpp:225` — "Mixxx Library"
- `library/dlgtrackmetadataexport.cpp:17` — the file-modification notice

Leave the long tail of preference tooltips. A DJ deep in ReplayGain settings
reading *"Mixxx will avoid an abrupt volume change"* is not confused about what
they are running, and each one costs a translation.

`dlgabout.cpp:469` already reads *"TangoMode is a modified version of Mixxx
for…"*, which is the tone to match: name this build, credit the original.

### Worth considering instead of literals

`VersionStore::applicationName()` already returns `"TangoMode"`. Strings in the
first group could take it as `%1` rather than hard-coding either name, which
keeps a single source of truth and survives a future rename. Costs a
re-translation of the touched strings either way, so it is nearly free to do it
the better way while in there.

---

## 4. Already known, not blocking

- **Flatpak CI fails.** Pre-existing and unrelated to the `.msi` / `.dmg`. A
  soundtouch cherry-pick (`4f35d26249`) is outstanding for it.
- **README fork notice.** Done, but the repo is still presented as a Mixxx fork
  in places outside README/INSTALL if anyone looks closely.
- **User documentation.** Still unwritten. The vocabulary is now small — cortina,
  pause mark, display name, LIVE mode, Set End Time — so this is a page, not a
  manual.
