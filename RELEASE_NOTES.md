# Tango DJ Mode for Mixxx

A set of Auto DJ enhancements for DJing at a typical tango milonga, built on top of
**Mixxx 2.5.6**.

Standard Mixxx is unchanged until you turn the mode on: everything below is gated
behind a single preference, **Preferences → Auto DJ → "Enable Tango DJ mode"**.

---

## Highlights

### Cursor queue (Keep Queue)
- Played tracks **stay in the list** and play **in order**; Auto DJ **stops at the
  end** instead of removing tracks or looping.
- A cursor follows the next track and re-anchors to the playing/last-played track
  **by identity** across the model rebuilds that every queue edit triggers — so
  adding, removing or reordering tracks (running or stopped) never resets your
  place.
- The idle deck is reloaded automatically when an edit changes which track is
  cued next.

### Control lockdown for live sets
While Tango mode is on, controls that could derail a live set are disabled:
**Shuffle, Add Random, Repeat, Skip, Fade Now** and **column sorting**. The
"Add to Auto DJ Queue (top)/(replace)" context-menu items are greyed out.

### Set timing & end-time readout
- The Auto DJ toolbar shows **"Milonga Ends: HH:MM:SS — HH:MM:SS left"** (end time
  in red), or **"Milonga length"** when stopped.
- Set a **target end time** and see at a glance whether you are running over or
  under.
- Timing accounts for the configured gap/crossfade, and in **Skip Silence** mode
  uses each track's analyzed audible range so trimmed silence is not
  over-counted. The upcoming-tracks total is cached and recomputed only when the
  queue, cursor or mode changes.
- A duration readout shows the **total time of the currently selected tracks**.

### Cortinas
- **"Add to Auto DJ Queue as Cortina"** tags a track (session-only) with a blue
  **"!!!CORTINA!!!"** title prefix in the Auto DJ list.
- A **preferred cortina length** feeds the set-length and end-time projection.
- Manually crossfading out of a cortina updates the end time and time-left (it
  does not change the projected set length).

### LIVE mode (performance lock)
- **Enter/Exit LIVE mode** from the LIVE indicator in the Auto DJ view.
- While LIVE, accidental-stop guards are armed: the first stop request only arms
  a **short confirmation window** (a red countdown that drains over the Auto DJ
  button); a second request **within that window** actually stops.
- Stray keystrokes that could disrupt playback are suppressed.

### Now-playing highlight
- The **currently playing** track is shown in **red** in the Auto DJ list.

### Auto DJ Side Panel
- An always-visible, **dockable** view of the Auto DJ queue
  (**View → Auto DJ Side Panel**) so the queue stays visible while you browse the
  library for the next track to add.
- **Dock** it to the side or **float** it; it keeps the skin theme in both states.
  Re-dock via the title-bar button, a double-click, or the right-click menu.
- It is a second, live view onto the same Auto DJ queue, and its
  size/position/visibility persist across restarts.
- Available **only in Tango mode** (hidden entirely otherwise).

---

## Notes & limitations
- Tango mode is a **single persistent preference**; turning it off restores
  standard Auto DJ behavior.
- **Cortina tags** and **LIVE mode** are **session-only** and reset at each
  launch.
- Tango fade defaults (Skip Silence + a short gap) are applied only while the fade
  settings are still at Mixxx's factory defaults, so your custom fade settings are
  never overwritten.
- Skin theming targets the **LateNight** skin (classic and palemoon variants).
- Based on **Mixxx 2.5.6**.

---

## Enabling
1. Open **Preferences → Auto DJ**.
2. Tick **"Enable Tango DJ mode"** and apply.
3. Load your set into the Auto DJ queue and start Auto DJ.
4. Optionally open **View → Auto DJ Side Panel** to keep the queue in view while
   browsing, and **Enter LIVE mode** once the set is running.
