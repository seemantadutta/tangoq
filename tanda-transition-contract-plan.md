# Tanda transition contract plan

This plan replaces the earlier countdown-first attempt. The countdown is out of
scope until the audio behaviour is complete.

## Goal

In Tango mode, every Auto DJ handoff follows one contract:

> Cut the outgoing deck at its last audible sound. Produce N seconds of true
> silence. Start the incoming deck at entry point E. Apply fade length F from E
> when that boundary supports a fade.

This resolves the conflict between fixed gaps and arbitrary start points. A gap
is no longer created by cueing the incoming deck backwards into its leading
silence. The gap is wall-clock silence, so the incoming entry can be anywhere in
the file.

## Contract terms

```cpp
enum class BoundaryType {
    TandaToTanda,
    CortinaBoundary,
};

enum class TrackRole {
    TangoTrack,
    Cortina,
};

struct TangoTransitionContract {
    BoundaryType boundaryType;
    double outgoingEndSecond;
    double gapSeconds;
    double incomingEntrySecond;
    double fadeInSeconds;
};
```

- `outgoingEndSecond`: last audible sound from the existing -60 dB analysis.
- `gapSeconds`: true silent wall-clock gap.
- `incomingEntrySecond`: user start point if present, otherwise first audible
  sound from analysis.
- `fadeInSeconds`: fade starts at `incomingEntrySecond`. It does not pre-roll
  audio before the entry point.

## Extensibility decisions

There is one gap value in the UI today, but the code keeps two resolver arms:

```cpp
double gapSecondsFor(BoundaryType type) const;
```

Today both arms return the same value. This keeps a later UI split cheap:

- tanda -> tanda gap
- cortina boundary gap

There is one fade use today, but the code keeps fade resolution separate:

```cpp
double fadeSecondsFor(BoundaryType type, TrackRole incomingRole) const;
```

Today only incoming cortinas can get a non-zero fade. This keeps later track
fades possible without rewriting the handoff executor.

## Explicit non-goals for this work

- No countdown timer.
- No crossfader movement as gap-progress feedback.
- No deck-gain fade rewrite unless Stage 2 proves crossfader motion is worth
  pursuing and conflicts with the existing cortina fade.
- No change to stock Auto DJ when Tango mode is off.

## Stage 1a — Trace the current behaviour

Purpose: establish evidence before changing transition behaviour.

Changes:

- Add `TT_TRACE` behind the existing `TANGO_TRANSITION_TRACE` CMake option.
- Keep the option compiled out when disabled.
- Log one line per transition decision, not per position callback.
- Include enough values to identify:
  - boundary type inferred from loaded tracks
  - Tango mode state
  - cortina fade state
  - outgoing last sound
  - incoming first sound
  - incoming intro-start cue / start point
  - chosen `startPos`
  - gap source and gap seconds
  - phase changes in the existing cortina fade path

Evidence:

- Build `mixxx-test`.
- Run:

  ```powershell
  .\mixxx-test.exe --gtest_filter='*CueControl*:*Hotcue*:AutoDJ*:*AnalyzerSilence*'
  ```

- User runs TangoMode manually with:
  - tanda -> tanda
  - tanda -> cortina, fade on
  - cortina -> tanda, fade on
  - tanda -> cortina, fade off
  - cortina -> tanda, fade off
  - at least one incoming track with a manual start point
  - at least one cortina with a manual start point

Done when the log confirms how the current code routes those boundaries. A
surprise in the trace stops the plan.

## Stage 1b — Add Tanda Transition enum and UI plumbing

Purpose: give the new behaviour a named mode before changing transition
execution.

Changes:

- Append a new `TransitionMode::TandaTransition` enum value. Do not insert it in
  the middle. Transition mode is persisted as a raw enum index.
- Add `Tanda Transition` to the Auto DJ transition-mode dropdown.
- Keep stock transition modes selectable for now.
- When Tango mode is enabled, set the transition mode to `Tanda Transition`.
  This is the simple rule: Tango starts in its intended transition mode. If the
  DJ wants to override it afterward, the dropdown still allows that.
- Set the Tanda Transition gap default to `3` seconds.
- In Tanda Transition, the transition/gap spinbox has positive-gap semantics:
  `3` means three seconds of silence.
- Do not remove the Cortina Gap preference yet.
- Do not route any handoff through the new mode yet beyond a safe placeholder
  that preserves current behaviour until the executor lands.

Sign handling:

- Stock modes keep their existing transition-time semantics.
- Tanda Transition uses positive gap seconds.
- Entering Tanda Transition from a negative Skip Silence value should display
  `abs(value)`, with the intended default being `3`.
- Detailed switching behaviour back to stock modes can stay conservative in this
  stage. The important invariant is that stock modes do not silently change
  their runtime semantics.

Evidence:

- Focused test filter passes.
- A small UI/config test if an appropriate seam exists:
  - enum value persists
  - dropdown can select `Tanda Transition`
  - enabling Tango selects `Tanda Transition`
  - gap value is positive

Manual check:

- With Tango off, stock modes are still visible.
- Turning Tango on selects `Tanda Transition`.
- Stock modes can still be selected manually afterward.

## Stage 1c — Introduce the contract resolver

Purpose: separate decision-making from execution.

Changes:

- Add `TangoTransitionContract`.
- Add `BoundaryType` and `TrackRole`.
- Add `gapSecondsFor(BoundaryType)`.
- Add `fadeSecondsFor(BoundaryType, TrackRole)`.
- Add a start helper for Tango transition entry:

  ```cpp
  double tangoEntrySecond(DeckAttributes* pDeck) const;
  ```

  It reads the deck's intro-start position directly. If valid and inside the
  track, it wins. Otherwise it returns `getFirstSoundSecond(pDeck)`.

- Do not use `getIntroStartSecond()` for this helper. It has stock Auto DJ
  fallback behaviour involving intro end and transition time. That is not the
  Tango entry rule.

Evidence:

- Resolver unit tests only.
- No runtime transition routing change yet.
- Existing focused test filter passes.

## Stage 1d — Execute the contract for cortina boundaries first

Purpose: prove the wall-clock gap executor on the already-special Tango path
before touching tanda -> tanda.

Changes:

- Route tanda -> cortina and cortina -> tanda through the contract executor when
  Tango mode is on.
- Preserve existing cortina fade behaviour where possible.
- Fade starts at `incomingEntrySecond`.
- Hard cut is `fadeInSeconds = 0`.
- Replace borrowed-silence assumptions with a real timer gap.

Evidence:

- Tests for:
  - incoming cortina with start point enters at the start point
  - incoming cortina without start point enters at first sound
  - hard-cut cortina boundary still gets the same wall-clock gap
  - Tango off ignores this path

- Manual check:
  - cortina with start point skips its build-up or speech
  - gap is fixed and silent
  - fade begins at the marked entry point

## Stage 1e — Execute the contract for tanda -> tanda

Purpose: replace stock negative-transition borrowed silence for Tango handoffs.

Changes:

- In Tango mode, tanda -> tanda uses the same contract executor.
- Outgoing cuts at last audible sound.
- Timer holds N seconds of silence.
- Incoming starts at start point if set, otherwise first sound.
- Fade is zero for now.

Evidence:

- Tests for:
  - tanda -> tanda with no start point starts at first sound after gap
  - tanda -> tanda with start point starts at the start point after gap
  - Tango off Skip Silence at negative transition time is unchanged
  - set-length accounting uses positive gap time correctly

- Manual check:
  - El Choclo-style track starts at the marked point
  - regular tanda tracks still trim leading and trailing silence
  - every boundary gets the same audible gap length

## Stage 1f — Collapse UI/settings to one gap value

Purpose: remove the visible two-gap model only after the executor proves one
contract can handle every boundary.

Changes:

- Remove the Cortina Gap preference row.
- Keep `gapSecondsFor(BoundaryType)` split internally.
- The transition/gap setting supplies both resolver arms.
- Update set-length calculation to match the one-gap contract.

Evidence:

- Set Length tests with cortinas in both fade modes.
- Resolver test proving the two gap arms can return different values through a
  test seam, even though production returns one value today.

## Stage 2 — Optional crossfader movement during the silent gap

Purpose: evaluate visual feedback after the audio contract works.

Rules:

- The timer defines the gap.
- Crossfader motion is visual feedback only.
- Only move the crossfader while both decks are silent.
- Incoming audio starts after the crossfader reaches the target side.

Open conflict to evaluate:

- Existing cortina fade may use the crossfader.
- If gap-progress motion conflicts with cortina fade, choose one:
  - skip crossfader progress on boundaries with fade-in, or
  - move fade-in to deck gain in a later stage.

This stage is intentionally reversible.

## Stage 3 — Countdown UI

Purpose: add explicit visible feedback after the transition system is complete.

Rules:

- Countdown reads the same gap state/timer as the contract executor.
- It must not own timing.
- It is UI-only.

Manual checks:

- visible during all wall-clock gaps
- hidden otherwise
- clears on Auto DJ stop, Tango off, eject, or manual takeover

## Final acceptance

- Tango off remains stock Auto DJ.
- In Tango mode, every boundary has one clear rule.
- A start point means "start audible playback here."
- Fixed gaps do not depend on a track having leading silence.
- Cortina fades start at the chosen entry point.
- Countdown and crossfader progress are not required for the audio feature to be
  considered complete.

