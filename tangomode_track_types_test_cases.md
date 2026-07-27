# Track types, pause marks and display names — manual test cases

Covers the "pause after" marks, the four track types (Milonga / Intro / Outro /
Performance), and the session-only display names. All of it is Tango-mode only:
with Tango off, none of it should be visible or have any effect.

Ordered by risk rather than by feature. The regression checks in Tier 2 matter as
much as the new behaviour — this work touched the transition engine three times.

Setup used for a realistic pass:
`intro → intro → tracks → cortina → tracks → cortina → tracks → outro → outro`

---

## Tier 1 — the new behaviour

### T1.1 Intro → milonga boundary
Two Intro tracks at the top, then a normal queue.

**Expect:** the two intros transition into *each other* normally; the set
**stops** at the intro→milonga boundary. Pressing Auto DJ starts the milonga.

*Watch for:* stopping after **every** intro (the same-type run is what makes this
work) or not stopping at all.

### T1.2 Milonga → outro boundary
La Cumparsita last, then Outro tracks.

**Expect:** stops after La Cumparsita; resumes on demand; outro tracks then
transition into each other normally.

### T1.3 Cortina → outro boundary (regression: this was broken once)
Make the **last milonga track a cortina**, followed directly by an Outro.

**Expect:** the cortina plays its full envelope, then the set **stops**.

*Why it matters:* a cortina hands over through its own gap timer rather than the
fade trigger, so it is a second place the stop decision is made. The two once
diverged and the outro played straight on through the boundary. Covered by
`TrackType_BoundaryStopsAfterACortinaToo`.

### T1.4 Performance, run through twice
**Expect:** starts at **0:00** (not cued past its opening), plays complete, then
stops. Resume. Later, when the set reaches the same performance again, it stops
**again**.

*Why:* a performance is sticky, unlike the one-shot pause mark. If it is being
consumed, the second time it would play straight into the next tanda.

### T1.5 Set Length arithmetic
Note the number. Type one queued track **Intro** → it should drop by roughly that
track's length. Type it **Performance** → it comes back.

Also check the **track count**, not just the time: the counts used to come from
the raw row count and now exclude intro/outro rows.

---

## Tier 2 — regressions

### T2.1 A plain set with no types at all
Normal tandas and cortinas, nothing typed, no marks.

**Expect:** exactly the old behaviour — same fades, no unexpected stops.

*The single most valuable check:* everything here rests on an untyped queue being
indistinguishable from stock Mixxx.

### T2.2 Cortina → tanda with Cortina Fade on
**Expect:** the whole envelope still runs — before-gap, fade-in, hold, fade-out —
and the set continues into the next tanda.

### T2.3 End of queue
**Expect:** the last track plays out in full, and Auto DJ switches off only once
it has ended.

### T2.4 Announcement pause on a cortina
Mark a mid-set cortina "pause after".

**Expect:** stops after its envelope; the mark is **consumed**, so the set does
not stop there again on a later run.

### T2.5 LIVE mode
End a set in LIVE mode.

**Expect:** the automatic stop goes through rather than waiting for the "confirm
stop" press, which nothing would ever give.

---

## Tier 3 — interactions

| # | Case | Expect |
|---|------|--------|
| T3.1 | Explicit mark **and** a type boundary on the same row | Stops **once**, not twice |
| T3.2 | "Pause after this track" on a Performance track | Absent from the menu |
| T3.3 | Reorder so a boundary moves (drag a milonga track above an intro) | Stops at the **new** boundary |
| T3.4 | Edit rows above a marked row | The mark follows its track |
| T3.5 | Remove a marked track from the queue | The mark disappears with it |
| T3.6 | Set an existing cortina to Outro | Tag flips `[-- CORTINA --]` → `[-- OUTRO --]` |
| T3.7 | Mark an Intro track as a cortina | Becomes a cortina and reverts to Milonga |
| T3.8 | Display name set, then cleared | Title column shows the name, then the real title; artist/album always real |
| T3.9 | Tango mode off | No tags, no submenu, no stops, no Set Length |
| T3.10 | Deck warning with Auto DJ **stopped** | Loading a marked track shows `[PAUSE AFTER]` in red |
| T3.11 | Deck warning with Auto DJ **running** | Same, and it follows the marked track from deck to deck |

---

## Known weak spots

Worth re-checking after any change to Auto DJ transitions or the cortina fade:

- **The cortina before-gap has no automated coverage.** When Auto DJ stops a deck
  itself, `ControlProxy` does not notify the sender, so the fake decks never
  deliver the play change a real engine does. If the last cortina of a set is
  ever left stopped at its cue point, this is the regression.
- **Resuming into a Performance track.** The 0:00 start is applied when the
  transition is calculated, which happens *before* the pause. If a performance
  ever starts partway in, suspect that path.
- **Widget display is not unit-tested at all.** The deck warning bug (invisible
  while Auto DJ was stopped) passed every test because they assert processor
  state, not what a widget shows.
