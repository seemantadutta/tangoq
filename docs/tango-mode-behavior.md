# Tango Mode behavior notes

This document records user-visible behavior contracts for Tango Mode. These notes
are intentionally about behavior, not implementation history, so they can be used
later when changing UI, storage, or transition internals.

## Start marker, first audible sample, and clear/reset semantics

Tango Mode exposes a visible start marker as `S` in the overview and `START` in
the waveform. This marker is backed by Mixxx's stock Intro Start cue.

There are two related but distinct positions:

- FAS: the analyzer's first audible sample, stored in the analyzed audible-range
  cue.
- S: the user-visible start marker, stored as the Intro Start cue.

On a freshly analyzed track, S and FAS commonly start at the same position
because silence analysis creates the default Intro Start cue at FAS.

If the DJ sets S to a timestamp `t`, only S moves. FAS remains the analyzer's
first audible sample.

Clearing S must not mean moving S back to FAS. Clearing S means there is no valid
user-visible start marker. In that state, Auto DJ falls back to FAS internally,
but the waveform and overview must not draw an S marker.

Resetting S is different from clearing S. Reset sets S to `00:00`, which means
the DJ intentionally wants the track or cortina to begin at the physical start of
the file, including any leading silence.

The Auto DJ list title may show a display-only start-time mark when S exists and
differs from FAS, for example `[-- 00:15 --] El Choclo` or
`[-- CORTINA -- 00:15 --] Crawling`. This is a heuristic until Tango Mode has
its own authorship storage: if S and FAS are identical, the list omits the time
mark because it cannot distinguish an analyzer-created default from a DJ-created
marker at the same position. A reset to `00:00` is shown when FAS is elsewhere.

Summary:

- Set Start: set S to the chosen timestamp.
- Clear Start: remove/disable the visible S marker; Auto DJ falls back to FAS.
- Reset Start: set S to `00:00`.

