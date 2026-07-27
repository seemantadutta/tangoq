# TangoMode — release checklist

Everything Tango adds on top of stock Mixxx, ordered so the first section is
what you would run if you only had twenty minutes before a gig.

Companion document: `tangomode_pause_marks_test_cases.md` covers pause marks and
display names in depth. This one covers the rest and does not repeat it.

Two rules that apply throughout:

- **With Tango mode off, none of this should exist.** No tags, no separator
  line, no Set Length, no LIVE indicator, no extra menu items, no stops. Stock
  Mixxx behaviour, byte for byte. This is the single most important property in
  the whole build — if a change leaks outside Tango mode, that is a release
  blocker regardless of how good the feature is.
- **Green unit tests say nothing about anything visual.** The tests assert
  processor state. Every widget bug this project has had passed them.

---

## Tier 0 — smoke test

If something here fails, stop and fix it before testing anything else.

| # | Check | Expect |
|---|-------|--------|
| S1 | Launch, Tango mode **off** | Plain Mixxx. No dancer icon, no LIVE indicator, no Set Length |
| S2 | Toggle Tango on (Preferences checkbox) | Dancer icon appears, Set Length appears |
| S3 | Toggle via keyboard | Windows/Linux `Ctrl+Alt+Shift+T`, macOS `⌘⌥⇧T`. The Preferences label shows the right one for the platform |
| S4 | Build a short queue, run a set | Tandas transition, cortinas play, nothing unexpected stops |
| S5 | Quit with tracks on both decks | Clean exit — see R4 |

---

## Tier 1 — the Tango core

### T1 Keep Queue cursor
Played tracks **stay** in the list rather than being consumed.

- The cursor advances past each played track; played rows grey out
- Editing rows *above* the cursor does not move it — it stays anchored to the
  track that is playing
- Removing the playing track's row, or a row above it, leaves the cursor sane
- "Eject decks and reset AutoDJ queue state" clears played status, ejects both
  decks, and restarts from the top. Only offered while Auto DJ is **stopped**

*Watch for:* the cursor jumping after an edit. This is the feature everything
else in Tango is built on.

### T2 Cortinas
- "Add to Auto DJ as cortina" appends and tags in one action
- The toggle flips an existing row in place, both directions
- Tag reads `[-- CORTINA --]` in blue
- A repeated cortina is **not** flagged as a duplicate
- The deck title shows `[CORTINA]` while one is loaded

### T3 Cortina Fade envelope
With Cortina Fade on, a cortina should run: **before-gap → fade-in → hold →
fade-out → after-gap**, then hand over to the next tanda.

- The crossfader does **not** flick to the cortina's side and back before the
  fade-in starts
- The before-gap stops the cortina deck on purpose and resumes it — Auto DJ must
  survive that pause and not treat it as the end of the set
- Fade-in, fade-out and gap lengths follow Preferences
- Hold time shown in Preferences = length − fade-in − fade-out
- The toolbar nudge changes cortina length live; Preferences follows it
- Cortina Fade settings are editable **only** while Auto DJ is stopped, and a
  running set keeps the values it started with

*Weakest area in the build.* The gaps run on real timers and no unit test can
walk the envelope.

### T4 Set timing
- **Set Length** counts every queued row — nothing is silently excluded
- **Ends** and **Left** update as the set plays
- **Set End Time** drives the over/under reading
- Cortinas count as their configured length, not their file length
- The numbers survive queue edits and re-sorting

---

## Tier 2 — protection against mistakes

These exist because losing a queue mid-milonga is unrecoverable.

### P1 The queue cannot be reordered or wiped
Every one of these must refuse while Tango mode is on:

- Track menu → "Add to Auto DJ (top)" and "(replace)"
- **Double-click** on a track
- Controller / keyboard controls for top and replace
- The playlist and external-library **sidebars** (right-click a whole playlist)
- Drag and drop *within* the queue is fine — that is reordering by hand

*Why the list is long:* these were blocked in the track menu first, and four
other paths still worked. Check all of them.

### P2 LIVE mode
- LIVE is off by default and is turned on deliberately (right-click the
  indicator)
- While LIVE, the **first** Auto DJ disable press only arms "Confirm Stop?"; a
  second within the window actually stops. Covers the button, `Shift+F12` and
  MIDI
- While LIVE, deck play/pause keys **D** and **L** do nothing
- The automatic end-of-set stop is **exempt** — it goes straight through, since
  no one is there to confirm it
- Leaving LIVE restores everything

### P3 Duplicates
- A track queued twice shows `[-- DUPLICATE --]` in amber
- **Cortinas are excluded** — repeating a cortina is normal and must stay unflagged
- The tag clears when the duplicate is removed

---

## Tier 3 — regressions with a history

Each of these has broken at least once.

| # | Check | Expect |
|---|-------|--------|
| R1 | Queue runs dry while a track plays, then you append more | Auto DJ stays on, the idle deck reloads, the set continues |
| R2 | Set ends for real | The last track plays **out in full**, then Auto DJ switches off — not when it starts |
| R3 | Resume onto the last queued track | Plays, Auto DJ stays **on**, crossfader follows the playing deck |
| R4 | Quit with Tango on and both decks loaded | Clean exit. No new `%LOCALAPPDATA%\CrashDumps\tangomode.exe.*.dmp`; `mixxx.log` ends with `Mixxx shutdown complete with code 0` |
| R5 | Relaunch after R4 | Queue comes back **ungreyed**. Greyed rows mean the previous run crashed |
| R6 | Set ends on a cortina | The cortina is audible through its whole envelope and is not left stranded at its cue point |
| R7 | Toggle Auto DJ off and on mid-set | The idle deck's waveform does not flicker or reload |
| R8 | Drag tracks onto the Auto DJ panel | Accepted — no "forbidden" cursor. Works docked and floating |

---

## Tier 4 — packaging and presentation

| # | Check | Expect |
|---|-------|--------|
| K1 | Executable name | `tangomode.exe` on Windows; shortcut and installer entries match |
| K2 | About dialog | Wordmark legible on **both** light and dark themes |
| K3 | Theme switch while running | Everything stays readable; no invisible text or lines |
| K4 | Preferences → Auto DJ | Tango section reads correctly, shortcut spelled for the platform |
| K5 | macOS build | Shortcut is `⌘⌥⇧T`; Gatekeeper bypass documented in `INSTALL.md` |
| K6 | Fresh profile | First launch with no existing settings behaves sanely |

---

## Known gaps at release

Not bugs — deliberate decisions, worth being able to answer if a user asks.

- **Session-only state.** Cortina marks, pause marks and display names are
  cleared on every launch. A clean slate each time is the intended default; a
  Preferences option to retain them is a possible follow-up.
- **The Auto DJ queue itself persists**, because it is a real database playlist.
  So the tracks come back but their Tango annotations do not.
- **No user documentation yet.** The vocabulary to explain is small — cortina,
  pause mark, display name, LIVE mode, Set End Time — but it does not exist in
  writing anywhere a DJ would find it.
