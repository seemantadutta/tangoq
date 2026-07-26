# Tango DJ Mode — Manual Test Cases

Comprehensive manual test plan for the **Tango DJ Mode** feature set added to this
Mixxx fork. Use it as a pre-release checklist to confirm every part of the
feature works and, just as importantly, that **stock Mixxx behaviour is unchanged
when Tango mode is off**.

Tango DJ Mode turns Auto DJ into a tool for playing a **pre-arranged milonga set**:
the queue plays in order, nothing is deleted by playback, it stops at the end, and
a set of milonga-specific aids (cortina handling, set-length/end-time readouts, a
performance lock, an always-visible queue) are layered on top. Everything is gated
behind the single **Tango DJ Mode** switch; with it off, Auto DJ is stock Mixxx.

> This is the single comprehensive Tango DJ Mode test document. Section M covers
> the **dockable Auto DJ Queue panel** in full (gating, toggle, shared-model sync,
> float/dock, persistence, styling, edge cases, shutdown).

---

## How to use this document
Run each case and mark **Result** as Pass / Fail, noting anything odd (especially
any audible click, wrong colour, control that should be locked but isn't, or a
crash). Cases marked **(MUST PASS)** are the release-blocking core.

Release must-pass set: **A1, A2, B1, B3, C1, D1, E1, F3, G1, G2, K1, K5, L1, M2,
M6, M16, M28, R1, R2**.

## Preconditions
- A build that includes all Tango changes (base commit `3ebac449e7` … HEAD).
- Ability to toggle **Tango DJ Mode** in Preferences → Auto DJ.
- Two Auto DJ decks configured as usual; nothing else playing.
- A library with several tracks, at least one crate and one playlist.
- A rebuildable **tanda-shaped queue**, e.g.:
  - Track A, B (a short "tanda"),
  - **Cortina C** (a short non-tango clip),
  - Track D, E (a second "tanda"),
  - spare Track F.
- For cortina cases: prepare at least **one analyzed cortina with real leading
  silence** and **one "hot-start" cortina** (audio begins immediately, and/or is
  un-analyzed). Analyze tracks via right-click → **Analyze**, the **Analyze**
  sidebar feature, or by loading each to a deck once.
- Skin: **LateNight (palemoon or classic)** for the fully-themed experience;
  note in results if you test another skin (Tango/Deere/Shade are not fully
  themed for these widgets).
- Suggested cortina-fade settings for easy observation: **Cortina Fade on,
  fade-in 5 s, fade-out 5 s, gap 2 s, cortina length 20 s.**
- For the dockable-panel float tests (Section M): a **second display** is optional
  but useful.

---

## A. Enabling Tango DJ Mode

### A1 — Enable from Preferences (MUST PASS)
1. Preferences → Auto DJ. Check **Tango DJ Mode**. Apply.
2. Open the Auto DJ view.

**Expected:** The Auto DJ toolbar shows the lit **dancing-couple** indicator, plus
the Tango-only readouts (Set Length, target end time, over/under, cortina nudge,
LIVE). The queue behaves in cursor mode (Section B).

**Result:**

### A2 — Couple indicator is a read-only status light (MUST PASS)
1. With Tango on, click the dancing-couple button in the toolbar.

**Expected:** Nothing happens — it is mouse-transparent (state is driven only from
Preferences). Lit = Tango on, dim = off.

**Result:**

### A3 — Toggle locked while Auto DJ is running
1. Start Auto DJ. Open Preferences → Auto DJ.

**Expected:** The **Tango DJ Mode** checkbox (and the other set-timing prefs) are
disabled while Auto DJ runs; stop Auto DJ → they re-enable.

**Result:**

### A4 — Tango defaults applied only at factory defaults
1. With transition mode/time at factory defaults, enable Tango.

**Expected:** Fade mode becomes **Skip Silence** and the transition becomes a
**−2 s** gap (defaults). If you had already customised those (e.g. −4 s), enabling
Tango does **not** overwrite them.

**Result:**

### A5 — Disabling Tango restores stock Auto DJ
1. Turn Tango off. Inspect the Auto DJ toolbar and behaviour.

**Expected:** All Tango-only toolbar widgets disappear; Auto DJ reverts to stock
behaviour (played tracks removed, requeue/shuffle available, etc.).

**Result:**

---

## B. Keep Queue (cursor) behaviour

### B1 — Plays in order, keeps played tracks (MUST PASS)
1. Tango on. Queue A, B, C, D. Start Auto DJ.
2. Let it advance a couple of tracks.

**Expected:** Tracks play top-to-bottom. Played tracks **stay in the list** (greyed)
instead of being removed; the currently-playing one is highlighted (Section D).

**Result:**

### B2 — Stops at the end; won't re-enable until tracks added
1. Let the set play to the last track and finish.
2. Try to enable Auto DJ again with no unplayed tracks left.

**Expected:** Auto DJ stops at the end and will **not** re-enable while every track
is played (a set ends deliberately). Adding an unplayed track lets it enable again
and it continues from the cursor (not the top). *(To replay the whole set from the
top, see Section L.)*

**Result:**

### B3 — Live edits keep the cursor anchored (MUST PASS)
1. While a set plays, add tracks at the bottom, and remove/reorder **upcoming**
   tracks below the cursor.
2. Also remove/reorder a **played** track above the cursor.

**Expected:** Playback is unaffected and the "next" track stays correct in every
case — the cursor re-anchors to the playing track after each edit. No jump back to
the top.

**Result:**

### B4 — Delete the cued next track
1. While playing, delete the track currently cued on the idle deck (the next one).

**Expected:** The idle deck reloads the new next track; playback continues cleanly.

**Result:**

### B5 — Delete the currently-playing track
1. While playing, delete the row that is currently playing.

**Expected:** It keeps playing on its deck (only the idle deck is ever reloaded);
the cursor re-anchors and the set continues.

**Result:**

### B6 — Skip button behaviour
1. Note the **Skip** button state in Tango mode.

**Expected:** Skip is **disabled** in Tango mode (you advance deliberately by
editing the list / using the crossfader), consistent with the lockdown (Section C).

**Result:**

---

## C. Tango lockdown

### C1 — Toolbar controls locked (MUST PASS)
1. Tango on. Inspect Shuffle, Add Random, Repeat Playlist, Skip, Fade Now.

**Expected:** Shuffle / Add Random / Repeat are disabled and **dimmed**; Skip and
Fade Now are disabled (greyed icons). These would wreck a pre-arranged set.

**Result:**

### C2 — Column sorting disabled
1. Click the Auto DJ list column headers.

**Expected:** Clicking headers does **not** re-sort (the sort indicator is hidden);
the play-order column ordering is preserved.

**Result:**

### C3 — Context-menu add lockdown
1. Right-click a library track. Inspect the **Add to Auto DJ Queue** items.

**Expected:** In Tango mode, **(top)** and **(replace)** are disabled (they'd
disrupt the arranged cursor set); **(bottom)** stays enabled.

**Result:**

### C4 — Keyboard/MIDI paths respect the lock
1. Trigger Shuffle / Skip / Add-Random / Fade-Now via keyboard shortcut or MIDI.

**Expected:** They no-op in Tango mode (the processor guards them, not just the
buttons).

**Result:**

---

## D. Track colouring in the Auto DJ list

### D1 — Now-playing highlighted red (MUST PASS)
1. Tango on, start a set.

**Expected:** The currently-playing row renders in **red**. Played rows above it
are **grey**; upcoming rows below are normal.

**Result:**

### D2 — Cortina rows blue with prefix
1. Tag a queued track as a cortina (Section E).

**Expected:** The row shows a blue **`!!!CORTINA!!!`** title prefix. A playing
cortina still reads as playing (red wins over blue).

**Result:**

### D3 — Colouring is Tango-only
1. Turn Tango off and inspect the Auto DJ list and other library views.

**Expected:** No red now-playing / blue cortina styling outside Tango mode; other
track tables are unaffected.

**Result:**

---

## E. Cortina tagging

### E1 — Tag / untag in the Auto DJ list (MUST PASS)
1. Right-click a track in the Auto DJ queue (Tango on).
2. Choose **Set as Cortina**; right-click again and choose **Set as Track**.

**Expected:** The action label flips between "Set as Cortina" and "Set as Track".
Tagging adds the blue `!!!CORTINA!!!` prefix; untagging removes it. Set-length
estimate updates immediately (Section H).

**Result:**

### E2 — Add from the library as a cortina
1. Right-click a library track → **Add to Auto DJ Queue as Cortina**.

**Expected:** The track is appended to the bottom of the queue and tagged as a
cortina (blue prefix). Not disabled by the Tango add-lockdown (it's a bottom-add).

**Result:**

### E3 — Session-only scope
1. Tag a cortina, quit and relaunch Mixxx.

**Expected:** Cortina tags are **session-only** — they clear on restart (by design).

**Result:**

### E4 — Tag scope is the Auto DJ list only
1. Confirm the "Set as Cortina/Track" toggle appears on the Auto DJ queue list, not
   in ordinary library/crate views.

**Expected:** The in-place toggle is scoped to the Auto DJ list; library views show
"Add … as Cortina" instead.

**Result:**

---

## F. Cortina length: budget + live nudge

### F1 — Preferences field is stop-only
1. Preferences → Auto DJ, note the **Cortina length** field (default 45 s).
2. Start Auto DJ and reopen the page.

**Expected:** Editable (5–600 s) while stopped; **greyed while Auto DJ runs**.

**Result:**

### F2 — Cockpit nudge buttons
1. Tango on. In the toolbar, use the cortina **−** / **+** buttons.

**Expected:** Each press changes the cortina length by the step (default ±2 s),
clamped 5–600 s. The value readout (`45 s`) updates live, works while Auto DJ is
running, and the change persists across sessions.

**Result:**

### F3 — Nudge step spinner (MUST PASS)
1. Change the **±N s** step spinner (range 1–10) to, say, ±5.
2. Press the cortina **+** button.

**Expected:** The length now jumps by 5 s per press. The `s` suffix and `±` prefix
stay put; only the number changes. The step is editable any time (live) and
persists. On LateNight/palemoon it is themed like the transition spinbox and sits
level with the − / + buttons.

**Result:**

### F4 — Cockpit ↔ Preferences stay in sync
1. With Preferences → Auto DJ open (Auto DJ running, field greyed), nudge the
   cortina length in the toolbar.

**Expected:** The greyed Preferences field updates to the new value (reflects the
live change), even though it can't be edited there while running.

**Result:**

### F5 — Nudging updates the estimate live
1. Watch **Set Length** / **Ends** (Section H) while nudging cortina length with a
   cortina in the queue.

**Expected:** The estimate moves as you nudge (cortina budget feeds it).

**Result:**

---

## G. Cortina Fade transition

Prereq: Preferences → Auto DJ → **Cortina transition = Cortina Fade**; fade-in 5,
fade-out 5, gap 2, cortina length 20.

### G1 — Full envelope, tanda → cortina → tanda (MUST PASS)
1. Queue A, B, **C (cortina)**, D. Start; play to the B → C boundary.

**Expected, in order and audibly:** B **hard-cuts** at its last sound → ~2 s
**silence** → C **fades in** over ~5 s → **holds** ~10 s → **fades out** over ~5 s →
~2 s **silence** → D **hard-starts** at full.

**Result:**

### G2 — No pop on a hot-start / un-analyzed cortina (MUST PASS)
1. Repeat G1 using a cortina with **no leading silence** (and/or un-analyzed).

**Expected:** **No click/pop** at the cut into the cortina. (The engine pre-rolls
synthetic silence for the cut to land in.) Compare with an analyzed cortina — both
clean.

**Result:**

### G3 — Gaps are true silence
1. Listen to both gaps in G1.

**Expected:** Both are real silence (crossfader fully off the cortina side), no
bleed of B's tail or D's head.

**Result:**

### G4 — Envelope timing / hold
1. With X=5, Z=5, length=20, time the phases.

**Expected:** ~5 s in, ~10 s hold, ~5 s out (hold = length − X − Z). The
**Cortina hold time** label in Preferences shows this derived value and warns when
X+Z exceeds the length.

**Result:**

### G5 — Short cortina (no silent tail)
1. Use a ~10 s cortina with X=5, Z=5, gap=2.

**Expected:** Fade-in/out scale to fit; the file ending does **not** strand the
transition — D still hard-starts after the after-gap.

**Result:**

### G6 — Back-to-back cortinas
1. Queue A, **C1(cortina)**, **C2(cortina)**, D.

**Expected:** Each cortina gets its own gap/envelope/gap; the C1→C2 handoff keeps
C2's lead-in so C2's before-gap engages cleanly (no full-volume blast).

**Result:**

### G7 — Cortina as the last queue item (documented limitation)
1. Queue A, B, **C(cortina)** with nothing after.

**Expected:** Auto DJ reaches end-of-set at the B→C cut and **disables**, so C plays
unfaded. No crash. (Recommendation: keep ≥1 track after the final cortina.)

**Result:**

### G8 — Fighting the crossfader mid-fade
1. During the envelope, nudge the crossfader partway; then slam it fully to the
   incoming (next-track) side.

**Expected:** A partial move springs back (the envelope re-asserts each callback). A
full slam to the incoming side triggers Mixxx's manual "faded fully over" behaviour
= an early manual skip to the next track (documented). Pausing the cortina deck
hands control back to you.

**Result:**

### G9 — Set-length accounts for the gaps
1. With Cortina Fade on, compare Set Length to Hard-cut mode.

**Expected:** In fade mode each cortina costs ≈ length + 2×gap, and cortina
boundaries are excluded from the normal per-track transition adjustment.

**Result:**

### G10 — Preferences UI gating
1. Toggle Cortina transition between Hard cut and Cortina Fade.

**Expected:** Under Hard cut the fade-in/out/gap fields and hold label are disabled;
under Cortina Fade they enable. All are locked while Auto DJ runs. Apply / Cancel /
Reset-to-defaults behave (defaults: Hard cut, 5/5/2).

**Result:**

---

## H. Set Length / Ends / Left readout

### H1 — Stopped vs running readout
1. Tango on, queue several tracks. Read the toolbar readout stopped, then start.

**Expected:** Stopped shows **Set Length: H:MM:SS**. Running appends **Ends: HH:mm:ss**
(red) and **Left: H:MM:SS**; Set Length stays constant, Left counts down.

**Result:**

### H2 — Ends holds while playing, slips while paused
1. Let it play, then pause the playing deck a while.

**Expected:** While playing, the Ends clock holds steady; while paused, Left freezes
and Ends slips later in real time (correct).

**Result:**

### H3 — Skip-Silence / cortina accounting
1. Use Skip Silence mode with analyzed tracks and a cortina.

**Expected:** Estimate uses each track's audible range (not full file) in Skip
Silence, budgets the cortina length for cortinas, and accounts for the gap per
boundary. Reasonable, slightly-safe (ends a touch early).

**Result:**

### H4 — No waveform flicker toggling Auto DJ
1. Toggle Auto DJ off/on a few times with the readout visible.

**Expected:** No flicker of the idle deck's waveform or the toolbar on toggle.

**Result:**

---

## I. Target end time + over/under

### I1 — Target end time editable any time
1. Tango on. Edit **Set End Time** (default 23:30:00), including while running.

**Expected:** Accepts 24-hr HH:mm:ss any time (e.g. set 00:15:00 if the night is
extended); persists across sessions.

**Result:**

### I2 — Over/under delta
1. With a set running, compare the projected **Ends** to the target.

**Expected:** The delta shows **▲ +H:MM:SS over** (red) when the projected end is
past the target, **▼ −H:MM:SS under** (green) when before, **● on time** at parity.
Updates as you nudge cortina length / edit the queue / change the target.

**Result:**

---

## J. Selected-tracks duration (context menu)

### J1 — Duration line for a selection
1. Tango on. Select several tracks (library or Auto DJ list), right-click.

**Expected:** A greyed, non-clickable info line shows total time + track count,
e.g. `12:34  (5 tracks)`. Tango-only; absent when Tango is off.

**Result:**

---

## K. LIVE mode (performance lock)

### K1 — Enter/exit deliberately (MUST PASS)
1. Tango on. **Right-click** the "LIVE" indicator (far right of the toolbar) →
   **Enter LIVE mode**. Then exit the same way.

**Expected:** The LIVE label turns **red/bold** when on, grey when off. There is no
left-click toggle (deliberate only). Session-only: it starts off at every launch.

**Result:**

### K2 — Two-step stop guard + drain overlay
1. In LIVE mode with Auto DJ running, press the Auto DJ (disable) button once.

**Expected:** Auto DJ does **not** stop; instead a red **liquid-drain** overlay
depletes over ~3 s on the Auto DJ button (a "confirm stop?" window). Press again
within the window to actually stop; do nothing and it re-arms (stays enabled).

**Result:**

### K3 — Guard covers all stop paths
1. Repeat K2 using **Shift+F12** and a mapped MIDI control.

**Expected:** The two-step guard applies to the button, the shortcut, and MIDI (one
choke point). A non-confirming action within the window disarms it.

**Result:**

### K4 — Deck play/pause keys suppressed
1. In LIVE mode, press **D** and **L** (deck 1/2 play).

**Expected:** No effect — the play/pause keys are suppressed so a playing deck can't
be stopped by accident. (Deck play button clicks and MIDI still work.)

**Result:**

### K5 — LIVE is Tango-gated and cleans up (MUST PASS)
1. Turn LIVE on, then turn **Tango off**.

**Expected:** LIVE is forced off (indicator hidden), the drain guard can't fire, and
D/L suppression is released. On exiting LIVE or destroying the view, key suppression
is always cleared (no lingering suppression).

**Result:**

---

## L. Eject decks and reset AutoDJ queue state

### L1 — Restart a played-out set from the top (MUST PASS)
1. Tango on. Play a set to the end (all rows grey). Stop Auto DJ.
2. Right-click in the Auto DJ queue → **Eject decks and reset AutoDJ queue state**
   → confirm.

**Expected:** Any tracks still loaded on the decks are ejected, all rows return to
normal colour (marked unplayed) and the cursor resets to the top, so re-enabling
Auto DJ plays from the first track. Your library play counts are **not** changed.

**Result:**

### L2 — Gated to stopped + non-LIVE + Auto DJ list
1. Look for the item while Auto DJ is **running**, while in **LIVE** mode, in a
   **library** view, and with **Tango off**.

**Expected:** The item is **absent** in all of those — it only appears on the Auto
DJ queue list, in Tango mode, while Auto DJ is stopped and not in LIVE mode.

**Result:**

### L3 — Confirmation required
1. Trigger the action but choose **Cancel**.

**Expected:** Nothing changes (no reset without explicit confirmation).

**Result:**

---

## M. Dockable Auto DJ Queue panel

An always-visible, dockable panel that is a second view of the same Auto DJ queue
model (**View → Auto DJ Queue**), so the queue stays on screen while you browse the
library. It is a **Tango-mode-only** feature and does not exist in the UI when Tango
is off. Re-docking is by button / double-click / right-click menu — the native title
bar is intentionally replaced by a themed one.

*Menu gating & toggle*

### M1 — Hidden outside Tango mode
1. Launch with Tango **off**. Open the **View** menu.

**Expected:** There is **no "Auto DJ Queue" item** at all (absent, not greyed); the
View menu looks like stock Mixxx.

**Result:**

### M2 — Appears when Tango is enabled (MUST PASS)
1. Enable Tango. Open the **View** menu.

**Expected:** **Auto DJ Queue** now appears, unchecked by default (opt-in).

**Result:**

### M3 — Disabling Tango hides the panel
1. Enable Tango, open the panel (docked or floating). Disable Tango.

**Expected:** The panel hides immediately and the **View → Auto DJ Queue** item
disappears; no crash.

**Result:**

### M4 — Re-enabling Tango does not auto-reopen
1. After M3, re-enable Tango.

**Expected:** The menu item reappears **unchecked**; the panel stays hidden until
you open it again.

**Result:**

### M5 — Menu toggle and close button stay in sync
1. Click **View → Auto DJ Queue** to open (docks on the right, menu item checked).
2. Click it again to close. Reopen, then click the **✕** on the themed title bar.

**Expected:** The menu item and the panel's visibility stay in sync in every case
(toggle on/off, and the title-bar close unchecks the menu item).

**Result:**

*Visible while browsing / shared-model sync*

### M6 — Stays visible across library views (MUST PASS)
1. Open the panel. In the sidebar click **Tracks**, a **Playlist**, a **Crate**,
   then **Auto DJ**.

**Expected:** The panel stays visible and unchanged the whole time — it does not
disappear when you leave the Auto DJ view.

**Result:**

### M7 — Add while browsing
1. Open the panel; click **Tracks** in the sidebar; add a track to the Auto DJ
   queue (drag or right-click → Add to bottom).

**Expected:** The new track appears in the panel immediately, without switching away
from the Tracks browser.

**Result:**

### M8 — Docked view ↔ panel live sync
1. Open the panel and the docked **Auto DJ** view. Reorder / remove / add tracks in
   the docked view.

**Expected:** The panel reflects every change live (same underlying model).

**Result:**

### M9 — Tango styling shows in the panel
1. Start a set with a tagged cortina.

**Expected:** The panel shows the same now-playing red, played grey, cursor, and
blue `!!!CORTINA!!!` styling as the docked Auto DJ view.

**Result:**

*Float / dock switching*

### M10 — Float button
1. Click the float/dock button on the themed title bar.

**Expected:** The panel detaches into a floating window that keeps the themed
gradient title bar (no native OS bar).

**Result:**

### M11 — Re-dock paths
1. While floating, re-dock using each of: the float/dock **button**, a
   **double-click** on the title bar, and right-click title bar → **Dock to Side**.

**Expected:** Each re-docks the panel to the side. (Dragging the bar moves the
floating window but is not expected to re-dock.)

**Result:**

### M12 — Drag to move while floating
1. Float the panel and drag its themed title bar around the screen.

**Expected:** The floating window moves with the cursor.

**Result:**

### M13 — Title-bar menu vs track menu
1. Right-click a **track row** in the panel.

**Expected:** The normal track context menu appears (load/add/cortina/etc.), NOT the
Float/Dock menu (which is only on the title bar).

**Result:**

*Persistence*

### M14 — Visibility persists (open)
1. With Tango on, open the panel, quit, relaunch (Tango still on).

**Expected:** Panel is open on startup; menu item checked.

**Result:**

### M15 — Visibility persists (hidden)
1. Close the panel, quit, relaunch.

**Expected:** Panel stays hidden and does not flash on startup.

**Result:**

### M16 — Width / side persists (MUST PASS)
1. Open the panel, resize it wider, optionally move it to the **left** dock area,
   quit, relaunch.

**Expected:** Restored to the same width and side.

**Result:**

### M17 — Floating state persists
1. Float the panel, move it, quit, relaunch.

**Expected:** Comes back floating at the same place, still themed.

**Result:**

### M18 — Not restored visible outside Tango
1. Open the panel, quit, **disable Tango**, relaunch.

**Expected:** Even with a layout saved with the panel open, it does not appear while
Tango is off; the menu item is absent.

**Result:**

*Interactions / loading*

### M19 — Load to deck
1. Double-click a track in the panel (or drag it to a deck).

**Expected:** Loads to the deck exactly like the docked Auto DJ view.

**Result:**

### M20 — Selection duration / cortina toggle in the panel
1. Select multiple tracks in the panel; right-click.

**Expected:** The same Tango context-menu items (selection-duration line, Set as
Cortina/Track) behave as in the docked list.

**Result:**

*Styling / skin*

### M21 — Table matches skin
1. Open the panel under LateNight (classic and palemoon).

**Expected:** Header, row colours, alternating rows and selection colours match the
docked Auto DJ list (no default-gray Qt table).

**Result:**

### M22 — Themed title bar (docked and floating)
1. Inspect the title bar docked, then float and inspect again.

**Expected:** Same dark gradient bar in both states (not native when floating);
float/close buttons use skin icons with hover highlight.

**Result:**

### M23 — Themed context menus
1. Right-click the title bar (**Float / Dock to Side**), and the LIVE indicator
   (**Enter/Exit LIVE mode**).

**Expected:** Both menus are skinned like other app menus (dark background, theme
text/hover), not native; each item hugs its label with even padding.

**Result:**

### M24 — Skin change while open
1. With the panel open, switch skin (or variant) in Preferences → Interface, apply.

**Expected:** Panel survives the reload and restyles to the new skin; no crash.

**Result:**

*Edge cases*

### M25 — Minimize doesn't "close" it
1. Open the panel, minimize the main window, restore it.

**Expected:** Panel still open and still checked in the menu (minimize is not a
user close).

**Result:**

### M26 — Empty queue
1. Clear the Auto DJ queue with the panel open.

**Expected:** Panel shows empty, no crash; adding a track repopulates it.

**Result:**

### M27 — Fullscreen
1. Toggle fullscreen (F11) with the panel open.

**Expected:** Panel remains usable/visible.

**Result:**

*Shutdown*

### M28 — Clean shutdown (MUST PASS)
1. Open the panel, then quit Mixxx.

**Expected:** Exits cleanly, no crash/assert on shutdown (the panel/view is
destroyed before the Auto DJ processor and model).

**Result:**

---

## N. Skin / theming (LateNight)

### N1 — Couple + cortina styling
1. Under LateNight classic and palemoon, inspect the couple indicator (open/lit
   padlock→couple icon) and the blue cortina rows.

**Expected:** Icons and colours render correctly in both variants.

**Result:**

### N2 — Themed spin controls line up
1. Inspect the transition spinbox, target-end-time edit, and cortina **±N s** step
   spinner in the toolbar.

**Expected:** All three share the dark embedded-spinbox theme and are vertically
aligned with their neighbours (no widget sitting high/low, no native-style box).

**Result:**

### N3 — Other skins degrade gracefully
1. Switch to a non-fully-themed skin (Tango/Deere/Shade).

**Expected:** Tango widgets still function; some may fall back to default styling
(documented) but nothing is broken or invisible.

**Result:**

---

## R. Regression (stock Mixxx unaffected)

### R1 — Normal Auto DJ unchanged with Tango off (MUST PASS)
1. Tango off. Run ordinary Auto DJ with a mix of tracks, including a
   (still-session-tagged) cortina.

**Expected:** Completely stock behaviour — played tracks removed, requeue/shuffle/
random available, normal transitions, no gaps/envelope/colours. Tango features have
zero effect with the switch off.

**Result:**

### R2 — AutoDJ unit tests pass (MUST PASS)
1. Developer check: `mixxx-test --gtest_filter="AutoDJProcessorTest.*"`.

**Expected:** All pass (confirms the core transition engine is intact).

**Result:**
```
```

### R3 — Hard-cut cortina mode unchanged
1. Cortina transition = **Hard cut** (default), Tango on, play through a cortina.

**Expected:** Legacy behaviour — cortina starts at full, you fade it out manually;
no automated envelope or inserted gaps.

**Result:**

### R4 — Clean stop mid-cortina / mid-fade
1. During any cortina phase (gap or envelope), disable Auto DJ.

**Expected:** Clean stop; no deck resumes on its own afterward (pending gap timer
cancelled).

**Result:**
