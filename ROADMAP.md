# TangoQ Roadmap & Known Issues

TangoQ is a [Mixxx](https://github.com/mixxxdj/mixxx) fork that adds a **Tango DJ
mode** for DJs who play pre-arranged *tanda* sets at milongas. A milonga set is an
ordered list of tandas (groups of 3–4 tracks by one orchestra) separated by
*cortinas* (short non-tango breaks): nothing is random, nothing is deleted by
playback, and the set stops at the end.

This document tracks fork-specific work — what has shipped, what is planned, and
the known rough edges. It complements `RELEASE_NOTES.md` (user-facing release
summaries) and `prerelease-tasks.md` (the active pre-release checklist).

The guiding invariant of the fork: **with Tango mode off, TangoQ behaves like
stock Mixxx.** Every Tango behaviour is gated so the stock path stays intact.

---

## Completed

### Tango mode & queue
- **Tango mode is permanent.** TangoQ is a tango-only build: Tango mode is locked
  on at startup and its UI switch (the preferences checkbox and keyboard shortcut)
  has been removed. The gating code is retained, so the change is cleanly
  revertible. Earlier the mode was a toggle (keyboard shortcut + preferences)
  honouring the "only while Auto DJ is stopped" rule; that has been superseded.
- **Cursor-based Auto DJ queue.** The queue plays in order, keeps played tracks,
  and stops at the end. Shuffle, skip, random, re-queue and list re-sorting are
  locked while in Tango mode.
- **Queue protection across every entry point.** "Add to Auto DJ (top)" and
  "(replace)" are blocked not only in the track menu but also for the
  double-click action, the controller controls, and the playlist / external-library
  sidebars, so none of them can reorder or wipe a Tango queue.
- **LIVE mode double-click safety.** Double-clicking a track during LIVE mode no
  longer replaces tracks on any deck.
- **Reset action ejects the decks.** "Eject decks and reset Auto DJ queue state"
  clears the decks as well as the queue state, for a truly clean slate.

### Auto DJ transitions & timing
- **Auto DJ disables after the last track finishes**, not when it starts to play.
- **Pause-after-any-track**, cortinas included, plus a session display name for any
  track. Together these cover announcements, performance tracks and intro/outro
  blocks: a performance is a track you rename and mark; an intro block is one you
  put a mark after. A red separator line in the list shows where the set will stop.
  (An earlier fixed set of four track types — milonga/intro/outro/performance — was
  removed: it required declaring the shape of the set up front, but the set is
  actually built on the fly during the milonga.)
- **Cortina settings in Preferences**: default cortina length, cortina transition
  mode (hard cut or auto fade), fade-in / fade-out, and a derived hold-time readout.

### Cockpit & HUD
- **Toolbar HUD** with a large countdown to the next track / cortina / set end, and
  a row of tanda progress pips.
- **Final-30-seconds "breathe".** In the last 30 seconds the countdown pulses from
  faint to full red (a smooth sinusoid) to warn the DJ, then returns to steady
  white.
- **Milonga Timing settings.** The Settings panel can show or hide each cockpit
  readout independently — set time, end time, cortina nudge, the HUD countdown
  timer, and the HUD progress pips. The HUD keeps its reserved size when a toggle
  is off, so the toolbar never reflows.
- **HUD hidden while Auto DJ is stopped.** With no set running there is no countdown
  to show, so the HUD paints nothing rather than sitting at `--:--`.
- **Cortina tagging in the deck area**, not just in the Auto DJ list.
- **Dancer icon** shows a red couple only in Tango mode and disappears entirely in
  stock mode, so plain Mixxx never shows the Tango marker.

### Branding & packaging
- Renamed the app binary / target to the fork name (`tangoq.exe`).
- Rebranded user-facing dialogs and menus, the About dialog, version info, the
  package summary / installer shortcut tooltip, and the macOS microphone-permission
  prompt from "Mixxx" to "TangoQ" (legitimate upstream attribution and license
  notices are retained).

### Stability
- **Fixed a crash on quit in Tango mode with tracks on the decks.** `PlayerManager`
  was destroyed before the `Library` that owns `AutoDJProcessor`; a deck destructor
  emitted `PlayerInfo::trackChanged` from `unloadTrack()`, which was answered inline
  by walking every deck — reading one that had already been freed. (The symptom was
  first seen as played rows staying greyed on the next run: the played flags are
  cleared only by `TrackDAO::finish()` on the clean-shutdown path, so a crash
  silently keeps them.)
- Silenced the shutdown warning `QSqlDatabasePrivate::removeDatabase: connection
  'MIXXX-2' is still in use`.
- **Duplicate detection** on add, with a warning, while ignoring cortinas (which are
  legitimately reused across a set).
- Drag-and-drop support into the Auto DJ docking / floating window.

---

## Planned

### Set-time accuracy: audible / start-cue durations
Use each track's audible span everywhere the set time is shown, so estimates match
what actually plays.
- **Selection-duration line** (near LIVE): currently whole-file, with cortina length
  counted as `min(CortinaLength, file)`. Upgrade each non-cortina track to its
  audible span — `LAS − FAS` from the `N60dBSound` cue when analyzed, and `LAS − S`
  when a Tango start (`S`) cue exists; fall back to whole file otherwise. A selection
  is small, so loading those cues is cheap.
- **Set over/under** (`recomputeKeepQueueUpcomingDuration` /
  `getRemainingSetDuration`): already uses cortina length and `LAS − FAS`
  (mode-aware for Skip-Silence / Tanda Transition; cache-limited to avoid loading
  every row). Gap: it does **not** subtract the `S` start cue — add `LAS − S` so the
  estimate matches playback. Keep the "don't `getTrack()` every row" rule (only
  refine already-cached rows).
- Reuse `keepQueueAudibleSeconds` / `keepQueueTrackPlaySeconds` so the selection line
  and the set calculation agree on what a queued track costs.

### Cockpit & UI
- Curated right-click menu in the toolbar area (`QMainWindow`-based).
- Remove the disabled "Add to Auto DJ (bottom / replace)" entries entirely rather
  than greying them out.
- Rename "Set DJ Start" / "Set DJ Start here" to **"Set Start"** / **"Set Start here"**.

### Now-playing / external display
Export the currently playing track to external display software (OBS or any screen
tool). Leans on the existing now-playing tracking; the main decision is mechanism —
a now-playing text/JSON file that other software reads is simpler and more flexible
than a built-in second window.

### macOS packaging follow-ups (require a Mac)
- Decide the TangoQ macOS icon source and point `packaging/macos/regenerate_icns.sh`
  at it; it still references the upstream Mixxx artwork.
- Build with `-DMACOS_BUNDLE=ON` (see `tools/tangoq_build_macos.sh`) and visually
  verify the Dock/Finder icon, the microphone-permission prompt, the About dialog,
  and the resulting `.dmg`.

---

## Known issues & polish

- The waveform turns purple during the last ~30 s, but that change is not in sync
  with the HUD "breathe"; it would be nicer if the two were synchronized.
- When breathing under 30 s and then pausing, the `--:--` should also turn red and
  breathe, so the DJ can tell they paused inside the final window.
- Make the breathe timing and duration configurable in Settings, with the cortina
  breathe time set separately from the track breathe time.
- The countdown sits slightly left of center; it should render centered over the
  playheads.

---

## Deferred by design

These are intentionally shelved because a clean slate each launch is the right
default for a DJ. If built, they should be opt-in via a Preferences checkbox to
*retain* state, leaving clean-slate as the default.

- **Persisting session annotations across launches.** Cortina designations, pause
  marks and display names are session-only and cleared on every launch. Persisting
  a cortina tag risks mistakenly branding a normal track (and applying a fade by
  mistake) unless the fade controls between two cortinas are locked. The Auto DJ
  queue itself already persists (it is a real database playlist), so this is only
  annotations on top: a JSON sidecar in the settings directory is enough (~a day's
  work). The one hazard is **load ordering** — pause marks are positional and must
  load *after* the Auto DJ model has rows, while the track-id-keyed annotations
  (cortinas, display names) can load any time. Scope them to the existing "Eject
  decks and reset Auto DJ queue state" action so marks live with the queue rather
  than branding a track forever.
- **Inline preview at the cue end point** via a single button in the track / Auto DJ
  list. Low priority — Mixxx already supports track preview.

---

## Long-term / research

- Detect tracks with large gaps or audio dropouts.
- **Tanda suggestion engine.** Needs a well-tagged library and more design work. One
  path: tag tracks as popular/unpopular during live gigs, build a dataset over
  months to years, then use it to suggest tandas based on expected dancer outcomes.
  (Framing and the finding that the underlying data does not yet exist live in the
  untracked `tanda-insights.md`.)
