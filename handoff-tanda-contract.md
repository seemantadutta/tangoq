# Handoff: Tanda transition contract

This briefing covers the live state of the current work. It deliberately does
**not** repeat `CLAUDE.md`, which loads automatically and already covers the
build, the branch model, and the general codebase gotchas. Read `CLAUDE.md`
first, then this.

## What this project is

TangoMode is a Mixxx fork. It adds a Tango DJ mode for playing pre-arranged
milonga sets. Everything Tango is gated on `[AutoDJ],keep_queue`. With that off,
the build must be stock Mixxx, byte for byte. The existing test suite is the
proof of that invariant.

The binary is `tangomode.exe`. **Never launch it.** The user runs it. Its
console output arrives as mojibake and redirection does not fix it. Build, run
tests, then hand over a manual checklist.

## The one standing rule for this work

**Do not diagnose from code reading alone.** For any real change: add logging
behind `TANGO_TRANSITION_TRACE`, ask the user to run TangoMode, read the logs,
*then* propose a fix. Reading code here has produced wrong diagnoses that a
single log line settled in minutes. This rule is why the trace macro exists.

Also: **short sentences, one idea each.** The user asked for this twice.

## The design we are building

One idea replaces three competing gap mechanisms. Call it the **contract**:

> Cut the outgoing deck at its last audible sound. Produce N seconds of true
> silence. Bring the incoming deck in at entry point E with a fade of length F.

- Hard cut is just F = 0.
- N is one number for all boundaries. It is 3 seconds today.
- Internally N is **two** numbers (`TandaToTanda` and `CortinaBoundary`), kept
  identical for now. If DJs later want them split, it is a one-line change. Do
  not collapse them to one.

Start points are scoped to **cortinas only**. A cortina can start anywhere. A
regular track always starts from its first sound. The user has one track (El
Choclo) that wants an offset but many cortinas with unusable build-ups, so
cortinas won the scope.

## The active plan

`C:\Users\sedutt\.claude\plans\tanda-transition-contract.md` is the current
plan. Read it first. It has six stages:

- **A** — observe baseline with trace logging. Done.
- **B** — countdown timer in the UI. Code-complete, built, awaiting the user's
  manual verification.
- **C** — extract the contract as a pure refactor. Acceptance test is a
  trace-diff identical to the Phase A baseline.
- **D** — route every boundary through the contract, including tanda→tanda.
  Highest risk. Drops the fade gates. Hard cut becomes F = 0.
- **E** — remove the Cortina Gap preference, unscope the marker helpers, one N
  in Set Length.
- **F** — land it.

Note there is a second, older plan file `cozy-seeking-finch.md` that plan mode
surfaces automatically. **Ignore it.** It is the superseded single-gap-timer
approach. The contract plan replaced it.

## Where we actually are right now

Stage B is built and on disk. The last relink was clean. `TANGO_TRANSITION_TRACE`
is still **ON**, so `[TT]` lines appear in the log — that is intentional for
these stages.

Stage B added a countdown so a gap does not look like a dead set. Key facts:

- New control `[AutoDJ],gap_remaining` holds whole seconds left, 0 when idle.
- Every gap now starts through one method, `startGap(int ms)`. It starts the
  silence timer *and* the countdown from the same call, so the on-screen number
  and the actual silence cannot drift apart. All three `m_cortinaGapTimer.start()`
  sites were routed through it.
- The readout is an amber `Gap: N s` label in the Auto DJ pane, beside the set
  timer (`labelTangoGapCountdown` in `dlgautodj.ui`).
- The countdown polls at 200ms, not 1s, so it never visibly skips a number.
- Pure static helper `gapRemainingSeconds(qint64 elapsedMs, int totalMs)` does
  the rounding-up. It is public as a test seam. Two tests pin it.

**The immediate pending task** is the user's manual check of Stage B:

1. Countdown appears on both cortina boundaries (fade on), blank otherwise.
2. Stop Auto DJ mid-gap — countdown disappears, nothing starts later.
3. Eject the waiting deck mid-gap — countdown disappears.

Items 2 and 3 are the ones most likely to be wrong. `cancelCortinaFade()` calls
`endGap()`, but nobody has watched it happen yet. Cortina-fade-*off* shows no
countdown yet — that is correct until Stage D.

## Non-obvious traps that have already bitten us

**The `startPos` units trap.** At the end of `calculateTransition()`, `startPos`
is normalized to a *fraction* of track length (`/= toDeckDuration`).
`setPlayPosition()` wants that fraction. Mixing seconds and fractions here is a
silent, wrong-position bug.

**Borrowed silence.** Stock makes its gap by cueing the incoming track
*backwards* into its own leading silence (`startPos = toDeckStartSecond +
m_transitionTime`, negative time). This collapses the instant a start point sits
past the silence — the rewind lands in real audio. This is exactly why the
contract replaces it with a real timed pause.

**The playhead is placed immediately on load.** `playerTrackLoaded` seeks to
`startPos` at load time (around cuecontrol.cpp:2958). The later AutoDJ re-cue is
usually a no-op. An earlier wrong theory said AutoDJ moves the playhead later —
the logs disproved it. Trust the trace, not the call graph.

**`fadeNow()` early-returns in Tango.** Any re-cue code inside it is dead in
Tango mode. Do not treat that site as live.

**`engine/` includes nothing from `library/`.** That is a deliberate layering
wall. `CortinaRegistry` lives in `library/` as an in-memory singleton precisely
so engine code stays clean. Respect the wall — it is why unscoping the marker
helpers is deferred to Stage E.

**Silent no-ops everywhere.** `ControlObject::set()` on a nonexistent control
does nothing. `ControlProxy::valid()` checks the key, not existence. A skin
`<Connection>` to a missing control is dropped with no warning. A duplicate
`<Mark>` is discarded with only a `qWarning`. When something does not appear,
suspect a wrong key before a wrong value.

**`keep_queue` never fires `valueChanged` for its own change.** It is a
change-request toggle. All logic must live in `controlKeepQueueChangeRequest()`,
not in `controlKeepQueue()`, which effectively never runs.

**The Auto DJ model fully rebuilds on every edit.** You get a transient
`rowsRemoved` (rowCount briefly 0) then `rowsInserted`. Anything reacting to
those signals must tolerate the empty blip.

**`TrackId(int)` is deleted.** Use `TrackId(QVariant(1))` in tests.

## Build reminders (the ones that actually catch people)

- Always `-- -j 2`. Default parallelism hits a compiler heap limit.
- `-DWARNINGS_FATAL=ON` is in the cache. CI uses it; without it you ship code the
  Windows job rejects.
- `LNK1104: cannot open file 'tangomode.exe'` means the app is open. Ask the user
  to close it.
- Skin/QSS/SVG edits need an app *restart*, not a rebuild — they load live from
  `res/skins/`.
