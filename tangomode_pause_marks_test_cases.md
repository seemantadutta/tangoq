# Pause marks and display names — manual test cases

Two mechanisms, both Tango-mode only: **pause after any track** (including
cortinas) and a **session display name** for any track. Between them they cover
announcements, performances, and intro/outro blocks — a performance is a track
you renamed and marked, an intro block is one you put a mark after.

With Tango off, none of it should be visible or have any effect.

Ordered by risk rather than by feature. The regression checks in Tier 2 matter
as much as the marks themselves — this area has touched the transition engine
several times.

Setup used for a realistic pass:
`intro → intro → [mark] → tracks → cortina → tracks → cortina → tracks → [mark] → outro → outro`

---

## Tier 1 — the mechanisms

### T1.1 Mark a normal track
**Expect:** the title shows `[-- PAUSE AFTER --]`, a red separator line is drawn
under the row, the track plays out in full, and the set **stops**. Pressing Auto
DJ continues into the next track.

*Also:* the mark is **consumed** — after resuming, the tag and the line are both
gone, and a later run through the same row does not stop again.

### T1.2 Mark a cortina
**Expect:** the cortina plays its **whole envelope** — before-gap, fade-in, hold,
fade-out — and only then does the set stop.

*Why it matters:* a cortina hands over through its own gap timer rather than the
fade trigger, so it is a second place the stop decision is made. The two once
diverged and a cortina played straight on through. This is the case with **no
automated coverage** (see Known weak spots), so it is the most valuable manual
check in this document.

### T1.3 Display name
Set a display name on a queued track, e.g. `[PERF] Tango de Roxanne`.

**Expect:** the Title column shows the name; **artist and album stay real**. The
menu item reads "Change display name..." once one is set. Clearing the box
restores the real title.

### T1.4 The two compose
Mark a track *and* give it a display name.

**Expect:** `[-- PAUSE AFTER --] [PERF] Tango de Roxanne`, with the separator
line below it.

### T1.5 The separator line
Mark two tracks well apart in a long queue.

**Expect:** a red line under **each** marked row, under that row and not a
neighbour. Scroll both off the top and back — the lines return. Resize the
window — they redraw.

*Why it matters:* this line was never drawn at all until it was fixed, and the
failure was silent — the row still said `[-- PAUSE AFTER --]`, so everything
looked half-right. It is drawn from row geometry in `WTrackTableView::paintEvent`
and nothing in the test suite can see it. Scrolling matters because the paint
loop only walks the rows currently on screen.

### T1.6 Blocks by hand
Two intro tracks, a mark on the second, then the milonga. La Cumparsita marked,
then outro tracks.

**Expect:** the intros transition into each other normally; the set stops at the
mark; the milonga runs; it stops after La Cumparsita; the outros then transition
into each other normally. This is the whole intro/outro story, built from marks.

---

## Tier 2 — regressions

### T2.1 A plain set with no marks at all
Normal tandas and cortinas, nothing marked or renamed.

**Expect:** exactly the old behaviour — same fades, no unexpected stops.

*The single most valuable check:* everything here rests on an unmarked queue
being indistinguishable from stock Mixxx.

### T2.2 Cortina → tanda with Cortina Fade on, unmarked
**Expect:** the whole envelope still runs and the set continues into the next
tanda. Being a cortina must not by itself stop anything.

### T2.3 End of queue
**Expect:** the last track plays out in full, and Auto DJ switches off only once
it has ended.

### T2.4 Resuming onto the last queued track
Let the set stop, append **one** track, press Auto DJ.

**Expect:** it plays, Auto DJ stays **on**, and the crossfader follows the deck
that is actually playing. Auto DJ switches off when that track ends, not when it
starts.

### T2.5 LIVE mode
End a set in LIVE mode.

**Expect:** the automatic stop goes through rather than waiting for the "confirm
stop" press, which nothing would ever give.

### T2.6 Set Length counts everything
**Expect:** every queued row contributes — intro music, outro music, everything.
Nothing is silently excluded. Check the **track count** as well as the time.

### T2.7 Quit cleanly
Quit with Tango mode on and tracks loaded on **both** decks.

**Expect:** no crash. Check `%LOCALAPPDATA%\CrashDumps` for a new
`tangomode.exe.*.dmp`, and that `mixxx.log` ends with `Mixxx shutdown complete
with code 0`. If the Auto DJ queue comes back with rows greyed as played, the
previous run did not exit cleanly.

---

## Tier 3 — interactions

| # | Case | Expect |
|---|------|--------|
| T3.1 | Reorder so a marked row moves | The mark follows its track |
| T3.2 | Edit rows above a marked row | The mark stays on its track, not its old index |
| T3.3 | Remove a marked track from the queue | The mark and its line disappear with it |
| T3.4 | Mark a track that is already a cortina | Both tags show; the line is drawn |
| T3.5 | Display name set, then cleared | Title shows the name, then the real title |
| T3.6 | Tango mode off | No tags, no line, no stops, no Set Length |
| T3.7 | Deck warning with Auto DJ **stopped** | Loading a marked track shows `[PAUSE AFTER]` in red |
| T3.8 | Deck warning with Auto DJ **running** | Same, and it follows the marked track from deck to deck |
| T3.9 | Multi-row selection | "Pause after" and "Set display name" are hidden — both apply to exactly one row |

---

## Known weak spots

Worth re-checking after any change to Auto DJ transitions or the cortina fade:

- **The cortina hand-off has no automated coverage.** Its gaps are driven by a
  real `QTimer`, so the fake decks cannot walk the envelope, and when Auto DJ
  stops a deck itself `ControlProxy` does not notify the sender. T1.2 and T2.2
  are the only things guarding it. If a marked cortina ever fails to stop, or an
  unmarked one stops, this is the regression.
- **Object lifetime at shutdown.** `PlayerManager` is destroyed before the
  `Library` that owns `AutoDJProcessor`, so the processor briefly outlives its
  own decks. Anything new that reacts to a deck signal can dereference a freed
  deck — which crashed on quit once already. T2.7 is the check.
- **Widget display is not unit-tested at all.** Two bugs proved it. The deck
  warning was invisible whenever Auto DJ was stopped, because the code driving it
  hung off deck signals that `toggleAutoDJ(false)` disconnects. And the separator
  line was *never drawn at all*, in any build: it asked `visualRect()` for the
  geometry of column 0, which is one of the hidden internal columns, so the rect
  came back empty and every marked row hit the `continue`. Both passed every
  test, because the tests assert processor state rather than what a widget
  renders. Green tests say nothing about this area — look at it.

  Two traps for anyone editing `WTrackTableView::paintEvent()`: never derive row
  geometry from a *cell* (use `rowViewportPosition()` + `rowHeight()`, which
  cannot be hidden out from under you), and never take a colour from
  `palette()` — skins style this view through QSS, which does not populate the
  QPalette, so palette colours come out as defaults.
