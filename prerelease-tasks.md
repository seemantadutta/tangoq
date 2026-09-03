# Pre-release tasks

> Historical planning record for the earlier optional Tango Mode work. It is not
> the active release checklist, and some proposed work below has since shipped.
> Use [`docs/release-1.0.2-checklist.md`](docs/release-1.0.2-checklist.md) for the
> current TangoQ release gate.

Each entry below records what was established during the earlier design work so
that the reasoning does not have to be re-derived.

---

## 1. Windows settings directory is shared with stock Mixxx — FIXED

Fixed in `5598b07d84`. Kept here as the record of what it was.

TangoMode stored its settings, log and database in `%LOCALAPPDATA%\Mixxx`, the
same directory a stock Mixxx install uses, so the two shared one
`mixxxdb.sqlite` and took turns migrating the schema.

**Cause:** the pre-1.12.0 Windows migration in `versionUpgrade()`. On any first
run (no `[Config]/Version` yet) it looked for
`~/Local Settings/Application Data/Mixxx/mixxx.cfg`, adopted it, and redirected
the whole settings path there. `Local Settings` is still a **junction** to
`AppData\Local` on current Windows, so that "legacy" path resolves to
`%LOCALAPPDATA%\Mixxx` — a stock Mixxx install. The strings look unrelated,
which is why reading the code ruled it out twice.

**Fix:** both legacy migrations removed, macOS pre-1.9.0 as well as Windows
pre-1.12.0. In a fork they can only adopt *upstream's* settings, since TangoMode
has never had an install of its own that old.

**Lesson worth keeping:** two hypotheses were formed by reading and both were
wrong; instrumenting the four stages between `CmdlineArgs` and `SettingsManager`
found it in one run. Write to a fixed file, not `qDebug()` — Mixxx's logging is
initialised from the very path under investigation.

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

### Setting the cue: a Tango-only "Set Start" button — decided

The DJ needs a way to place that cue. Stock Mixxx already has one, but it is the
group of four intro/outro cue buttons, which is four buttons and a mental model
(intro start, intro end, outro start, outro end) to learn for a feature that
needs exactly one point. **Tango gets a single button instead.**

**What it binds to.** The existing `intro_start_set` control — *not*
`intro_start_activate`. `_set` unconditionally places the intro-start cue at the
playhead; `_activate` seeks to the cue if one already exists, so pressing it
twice would move the deck instead of moving the mark. One button, one meaning:
*start here*.

Binding to the stock intro-start cue rather than inventing a Tango-private one
is what makes the whole design cheap: it persists in the database already, it
survives Tango being turned off, and `audibleStartSecond()` below reads it with
no new storage.

**Naming.** Label it for its effect, not its mechanism — "Set Start" or
"Start Here". Not "Cue": the deck already has a big red CUE button doing
something else entirely, and reusing the word is the one thing guaranteed to
confuse.

**Visibility: Tango mode alone.** `[AutoDJ],keep_queue > 0`. Not
`show_intro_outro_cues` — it is not part of that group and must not depend on it.

**Both the group of four and the settings menu entry are hidden in Tango —
decided.** Gating only the menu entry is not enough: a DJ who had already enabled
"Intro & Outro Cues" would keep the four buttons *and* lose the toggle that hides
them. Gating both gives a clean round trip, because **neither gate writes
`[Skin],show_intro_outro_cues`** — the setting keeps its value, so turning Tango
off restores exactly what the DJ had.

| | Tango off | Tango on |
|---|---|---|
| Group of four | per `show_intro_outro_cues` | **hidden** |
| "Intro & Outro Cues" menu entry | shown | **hidden** |
| "Set Start" | hidden | **shown** |

The group of four is at
`res/skins/LateNight/decks/row_5_transportLoopJump.xml:185-223` and already has a
`visible` connection on `show_intro_outro_cues`. It now needs
`show_intro_outro_cues AND NOT tango`, and **two `<Connection>` blocks binding the
same property do not AND — they fight.** Get the AND from **nesting**: leave the
existing group untouched and wrap it in an outer group carrying the negation.
Same wrapper idiom for the menu entry at
`res/skins/LateNight/helpers/skin_settings_full_deck.xml:60-65`, following
`mic_unit_unconfigured.xml:49-53`:

```xml
<WidgetGroup>
  <SizePolicy>me,min</SizePolicy>
  <Layout>vertical</Layout>
  <Children>
    <!-- existing template / group, unchanged -->
  </Children>
  <Connection>
    <ConfigKey>[AutoDJ],keep_queue</ConfigKey>
    <Transform><Not/></Transform>
    <BindProperty>visible</BindProperty>
  </Connection>
</WidgetGroup>
```

Four skins carry the same entry — LateNight, Tango, Shade, Deere. **LateNight is
the release skin; do that one and leave the others stock** rather than
maintaining the gate in four places.

**The start marker ships with the button — not deferred.** `intro_start_position`
is gated on `show_intro_outro_cues` in *both* `waveform.xml:94-102` and
`decks/overview.xml:80-86`, and that setting **defaults to off**. So without this,
pressing "Set Start" changes nothing anywhere on screen: no way to see where the
mark landed, whether it landed, or that a second press moved it. For a feature
whose whole job is "skip the first 11 seconds", that is not optional.

`VisibilityControl` parses a **single bare ConfigKey**
(`src/waveform/renderers/waveformmark.cpp:129-133`) — no `<Transform>`, no OR — so
add a **second** `<Mark>` node for `intro_start_position` in each file gated on
`[AutoDJ],keep_queue`, leaving the stock node alone. Do not duplicate the
`MarkRange` or any outro node. The button itself also carries a 2-state icon
driven by `intro_start_enabled` (as the stock button does), so it reads *set* vs
*unset* independently of the marker.

Accepted cosmetic edge: the stock marks stay gated on `show_intro_outro_cues`
alone, so a DJ who had it enabled sees the intro/outro marks in Tango with the
new mark drawn over the stock one at the identical position. Exact overdraw —
indistinguishable from a single mark.

### Where the start point comes from — resolved

The earlier open question ("how is *no cue set* represented?") is settled, and
the first answer was wrong. `getIntroStartSecond()` (`autodjprocessor.cpp:2317`)
looks like the right helper but has a middle branch: when intro *start* is
invalid and intro *end* is set, it returns `introEndSecond - m_transitionTime`.
Under Tanda, `m_transitionTime` is a **gap**, so routing through it would leak
the gap length into a start-point calculation. Tanda needs its own helper:

```cpp
// The DJ's cue wins over analysis: they know the track.
double AutoDJProcessor::audibleStartSecond(DeckAttributes* pDeck) {
    const mixxx::audio::FramePos introStart = pDeck->introStartPosition();
    if (introStart.isValid() && introStart <= pDeck->trackEndPosition()) {
        return framePositionToSeconds(introStart, pDeck);
    }
    return getFirstSoundSecond(pDeck);
}
```

No `cueIsSet()` predicate is needed — validity *is* the test. And note
`AnalyzerSilence::setupMainAndIntroCue()` already places the intro cue at first
sound during analysis, so for an analysed track with no manual cue the two
branches agree by construction. The distinction that worried us ("cue at 0:00"
versus "no cue") does not arise.

### Open decisions

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

## 5. Tango HUD — toolbar heads-up display

Fill the empty toolbar band (the `me,min` expand spacer in `toolbar.xml`,
between the left mode toggles and the right clock/REC/SETTINGS cluster) with an
at-a-glance orientation display for the milonga DJ. Read-only: it reflects Auto
DJ / Tango state and never touches transition logic.

### What shipped (supersedes the flow-strip design below)

The **flow strip** — an idealized `TTVTTM` pattern painted in the toolbar with a
highlight, a mismatch `!`, a configurable Preferences pattern, and a "Set flow
position" re-anchor menu — was built, then **removed**. In use it added cognitive
load and the toolbar could not hold the set's full, growing history. It was
replaced by an ordinary, user-selectable **Item Type** column in the Auto DJ
list:

- Each tanda header shows its type letter (**T/V/M/N**); a cortina shows a dim
  blue **`c`**, a performance track a dim green **`p`**; loose tracks and a
  tanda's own member rows stay blank. The lowercase marks are deliberately
  subordinate so the eye rides the tanda rhythm.
- It behaves like any column (header menu, movable, persists) and exists only in
  Tango mode, so stock Auto DJ is byte-for-byte unchanged.
- Performance tracks (new session-only `PerformanceRegistry`, mutually exclusive
  with cortinas) also get a green `[-- PERFORMANCE --]` title prefix and pauses
  **before and after** the track.

**Removed with the strip:** the `hud_flow_*` controls, `publishHudFlow`, the
`[Auto DJ],TandaFlowPattern` preference, the mismatch `!`, and the session flow
anchor in `TandaQueueState`. **Kept in the HUD:** the countdown (now a two-line
label-over-large-time) and the track pips. The design notes below are retained
as history; the countdown/pip/flash decisions still apply to the HUD that remains.

### Decided

- **Countdown — one switching line.** A single line that counts down to the next
  transition and relabels itself by what comes next: `Next track in mm:ss` when
  the upcoming queue item is a normal track, `Cortina in mm:ss` when it is a
  cortina. Not two lines.
- **Tanda sequence — idealized, configurable pattern.** Render a configured
  template (e.g. `T,T,V,T,T,M`) as the "map" and highlight the current position.
  Do **not** derive the real sequence from the queue in v1 — that needs reliably
  tagged tracks. Deriving the actual sequence from a tagged library (or scanning
  track data to infer it) is a **v2** idea.
- **Configurable in Preferences.** The expected pattern lives in a new preference,
  backed by a config key the HUD reads.
- **Countdown flashes red in the final 30 s.** During the last 30 seconds of the
  active countdown (track or cortina) the countdown text flashes red. If the
  cortina is shorter than 30 s it flashes for its whole duration; the same rule
  applies to a track, and the code must handle a track shorter than 30 s even
  though that is unusual. Live-stage behaviour.
- **Pips match the Auto DJ list.** Track-in-tanda pips reuse the list's look:
  8 px discs, 5 px gaps, three states (played = filled, playing = left-half pie,
  unplayed = outline), rendered red to tie them to the current (red) tanda. The
  live widget should share `drawProgressPip` with `wtandaqueueview.cpp`.
- **Layout — centered first.** Center the HUD content in the spacer region and
  test it before finalizing. Accept that adding widgets later may nudge things;
  revisit left/right anchoring (which is reflow-safe) only if centering proves
  jarring.
- **Gating.** Shown under `[AutoDJ],keep_queue` (Tango). Once the mode default is
  flipped on (see below), that simply means "always shown".

### Confirmed in-between-state behaviour (from review)

- **Countdown = time until the next track becomes audible, always true.** Add the
  silent gap for Tanda Transition and the cortina after-gap; for crossfade modes
  subtract the transition time (audible at crossfade start). During a live gap,
  show the gap counting down (read the gap timer's remaining).
- **During a cortina:** preview the *upcoming* tanda — show its pips but **dimmed**
  and advance the flow highlight to it. Restore full brightness when the tanda's
  first track actually starts.
- **Last track of the set:** label becomes **"Set ends in"** (no next item).
- **Auto DJ stopped while Tango on:** keep showing `--:--` and no pips — a cue for
  the DJ to glance there once Auto DJ starts. Do not hide the HUD.
- **Paused track:** show `--:--` (not a frozen number).
- **Flow anchor is session-only:** do **not** persist it across restart/crash. On
  restart the DJ re-syncs with the "Set flow position" menu. (Overrides the
  earlier "persist the anchor" note.)
- **Layout never clips:** the widget measures its content (wide tandas / long
  patterns) and fits rather than clipping silently.

Implementation note: the countdown label now has three states (track / cortina /
set-ends), so the single `hud_next_is_cortina` flag generalises to a "next kind"
value; and the cortina case needs the upcoming tanda's state plus a "dimmed" flag.

### Staging

1. **Countdown line** — prove the centered container + the C++→skin data bridge
   with the single switching countdown. Lowest risk.
2. **Tanda pips** — a row rendering the configured T/V/M pattern, current tanda
   highlighted.
3. **Track-in-tanda pips** — played / current / remaining, mirroring the Auto DJ
   list's tanda indicators.

### Open / to settle at implementation

- **Display-binding mechanism.** Legacy skins bind numeric controls, not strings.
  Decide between exposing numeric `ControlObject`s (seconds + `next_is_cortina`)
  that the skin formats, vs. a small custom toolbar label widget the processor
  updates directly. Settle this as the first implementation step of Stage 1.
- **"Cortina in" definition.** Whether it counts only the current track's remain,
  or the current track plus the full remaining tango tracks up to the cortina.

### Flow anchor — "Set flow position" tanda menu

With mixed ungrouped tracks and tandas, the ordinal-based flow index cannot know
where in TTVTTM the DJ actually is. The motivating case: a special *performance
track* queued as a loose (ungrouped) track between tandas — it is not a tanda, so
it breaks the ordinal count and the flow highlight drifts. Fix: a context-menu
"Set this tanda as ->"
submenu on tanda headers (only headers, reusing the existing
`WTandaQueueView` menu that already hosts "Change tanda type") whose six items
are the flow slots. Selecting one anchors that tanda to that slot, and the HUD
re-bases: `position = (tandaOrdinal - anchorOrdinal + anchorSlot) mod patternLen`.

- Distinct from "Change tanda type" (which sets the musical T/V/M type / list
  label). This sets HUD flow position only.
- Label the six slots unambiguously (e.g. `1 - Tango` ... `6 - Milonga`, tick the
  current), since bare `T T V T T M` repeats.
- Hold one anchor `(tandaId -> slot)` in memory (session-only, **not** persisted -
  see the confirmed decisions above); most recent sync wins. Keyed by tandaId so
  it survives queue edits within the session.
- The submenu is **generated from the configured pattern** — its item count,
  order and types all follow the Preferences pattern, nothing hard-coded. So
  this must land *after* the configurable-pattern prefs are wired.

### Type-aware flow (confirmed)

The flow strip must reflect what the DJ *actually* plays, not just an idealized
template. Near the end of a milonga the DJ may deviate — e.g. `TTVTTT` instead of
`TTVTTM` (a Tango where the Milonga would be). Decision (option 1): drive the
strip from each tanda's **actual type**, which the DJ already sets with the
existing "Change tanda type" menu. The configured pattern is the *expected*
default; the assigned type overrides per tanda. So swapping the last M for a T
sets that tanda's type to Tango and the strip shows `T T V T T T`.

This also resolves the earlier observation that changing a tanda's type does
nothing to the HUD — once the strip is type-aware, it updates immediately (the
`spansChanged -> rebuild -> publish` path already fires). The current HUD publishes
only a flow *ordinal*; type-aware flow needs the per-tanda type published too.

### Ideal vs real, and how they reconcile (settled)

Two layers. The **ideal flow** (the Preferences pattern) is always present and
fixed — it is a *map*, not a claim about the set. The **real flow** (the
groupings) may be empty, partial, longer, reordered, or deviating. Reconciliation
is *not* "merge into one mutated string": the ideal is a dim backdrop, and the
real paints onto it only where groupings exist.

- **No groupings:** show the ideal, dimmed, nothing lit, no pips — a pure
  reference map. The countdown line still works (it is deck-timing driven, not
  grouping driven).
- **Placement is positional (option 1):** the *nth* grouped tanda occupies the
  *nth* slot (mod pattern length), and its **actual type overwrites that slot's
  letter**. The highlight follows the current tanda's slot. Correct when the DJ
  builds in pattern order; the anchor menu corrects drift.
- **Longer than one cycle:** the strip is one cycle-window; the highlight wraps by
  modulo.
- **Loose tracks, performance tracks and cortinas do not consume slots.** The
  ordinal count walks real tandas (spans) only, exactly as the resolver does now.
  Their effect is *drift*: a performance track standing in for, say, the second T
  pushes the next tanda onto a contradicting slot — which lights the mismatch
  mark, and the DJ resyncs with "Set flow position".

### Mismatch mark — trailing red `!` (settled)

A single red `!` at the **end of the strip** signals that the real flow has
diverged from the ideal, prompting a re-anchor. Rule:

- Compare **only occupied slots** in the current cycle-window against the ideal.
  A slot is occupied only when a real tanda maps to it.
- `!` fires when an occupied slot's type **contradicts** the ideal there.
- **Incompleteness never trips it.** Empty / not-yet-built slots are ignored, so a
  partial prefix (`T`, `TT` against `TTVTTM`) shows no `!`. Building the list one
  tanda at a time is the normal case and must stay quiet.
- **Stateless — option (a).** The `!` reflects the current overlay each moment.
  Anchoring to a correct slot clears it; an intentional deviation (`TTVTTT`)
  leaves it quietly lit. No "acknowledge" state in v1 (option (b) deferred).
- Ships as one numeric control `hud_flow_mismatch` (0/1), computed in the resolver
  (part of the type-aware step) and painted by the HUD.

```text
ideal:   T  T  V  T  T  M
strip:   T [V] V  T  T  M   !     ← occupied slot 2 (V) contradicts ideal (T)
partial: T [T]                    ← T or TT: matches, no !
```

### Flow-strip data channel (settled)

The HUD is decoupled from the processor through numeric `ControlObject`s and is
built by the skin parser with the default constructor, so a pattern *string*
cannot cross to it cleanly. Resolve the whole strip in C++ and feed the HUD
pre-digested slots; the widget only draws letters. New `[AutoDJ]` controls:

- `hud_flow_len` — active pattern length.
- `hud_flow_highlight` — current slot index, or -1.
- `hud_flow_slot_0` .. `hud_flow_slot_7` — type per slot (0=T,1=V,2=M,3=N;
  -1 = empty / show ideal default dimmed).
- `hud_flow_mismatch` — the trailing `!` flag.

Cap of 8 slots (a milonga cycle is ~6). This makes the configurable pattern, the
type overlay and the anchor all "compute the slots differently in C++" with no
string over controls and no config pointer plumbed into the widget. The HUD stops
hard-coding `kFlowPattern` entirely.

### Build order for the flow work

1. **Foundation + configurable pattern.** Add the `hud_flow_*` slot channel; add
   the `[Auto DJ],TandaFlowPattern` preference (default `TTVTTM`, validated to
   T/V/M/N, length 1-8) with a field in `dlgprefautodjdlg.ui`; the resolver reads
   the pattern and publishes slots; the HUD renders from slots. (Expected default.)
2. **Type-aware flow + mismatch `!`.** Overlay each tanda's actual type onto its
   positional slot; compute `hud_flow_mismatch` per the rule above. Fixes the
   end-of-milonga `TTVTTT` case and the type-change no-op.
3. **"Set flow position" tanda menu** (items generated from #1; session-only
   `tandaId -> slot` anchor; resolver rebases). Anchors the flow / clears drift.
4. **30 s red flash** (independent of the above).

### Related decision — remove the Tango "mode"

Make the app Tango-by-default with no user-visible mode. Recommended path is
**(B) flip the `keep_queue` default on and hide the toggle, keeping the internal
gates** — reversible, upstream-mergeable, and preserves a hidden dev switch for
stock-parity checks — rather than **(A)** ripping out all 56 `keep_queue` gates.
Decoupled from the HUD. If adopted, update the "stock byte-for-byte with Tango
off" invariant in `CLAUDE.md`. **Pending: A vs B.**

---

## 6. Windows installer UpgradeCode — FIXED (needs a 1.0.2 release note)

The companion to section 1: that one was the shared *settings* directory, this is
the shared *installer* identity.

**Cause:** `CPACK_WIX_UPGRADE_GUID` was inherited byte-for-byte from upstream
Mixxx (`921DC99C-4DCF-478D-B950-50685CB9E6BE`). WiX uses the UpgradeCode as the
"product family" key, so Windows treated TangoQ and an installed Mixxx 2.5.6 as
the same product: installing TangoQ over Mixxx hit the already-installed / major-
upgrade path instead of installing alongside it.

**Fix:** gave TangoQ its own fork-specific UpgradeCode
(`F10598B3-D0E4-47FC-9E86-E2EC982931BB`) in `CMakeLists.txt`, with a comment
warning not to revert it on an upstream merge. TangoQ and Mixxx can now coexist.

**Consequence for early-access users — must be in the 1.0.2 release notes.**
Changing the UpgradeCode means the new installer no longer upgrades an existing
(old-GUID) TangoQ in place; installing 1.0.2 leaves two TangoQ entries unless the
old one is removed first. `RELEASE_NOTES.md` is currently stale (still describes
the removed "Enable Tango DJ mode" toggle) and should be rewritten fresh for
1.0.2; include this line:

> **Upgrading from an earlier TangoQ:** this version changes the installer's
> product identity, so Windows will not upgrade an existing TangoQ in place.
> **Uninstall your current TangoQ first**, then install this one.

**Verify on Windows when the MSI is built:** (1) installs cleanly with stock
Mixxx 2.5.6 present, no already-installed block; (2) Add/Remove Programs lists
"Mixxx" and "TangoQ" separately; (3) both run side by side.
