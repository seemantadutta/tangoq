# TangoQ 1.0.3

TangoQ 1.0.3 is the first update to the early-access release of TangoQ, the
dedicated Argentine tango DJ application based on Mixxx 2.5.6. It records each
milonga in History, remembers your cortinas, checks that your set reads like a
milonga, and makes changing a set on the fly safer.

**New to TangoQ?** Watch the
[video tutorials](https://www.youtube.com/playlist?list=PLD1MAXCCQrTw).

> **Upgrading from TangoQ 1.0.2:** Install 1.0.3 over 1.0.2; you do not need to
> uninstall first. Your library, cues, playlists, queue and preferences carry
> forward. The first time 1.0.3 starts, it upgrades the library database and
> keeps a copy of the old one next to it, named
> `tangoq.db.pre-tangoq-schema-2.bak`, in the TangoQ settings folder (see
> [INSTALL.md](INSTALL.md)). TangoQ 1.0.2 can still open the upgraded database.

## History: one session per milonga

- Each time you run TangoQ, History starts one session when the first track
  plays, named by its date and time. It stays open through stops and pauses
  until you reset the queue or quit TangoQ.
- Every track TangoQ plays is logged, including a cortina you use after every
  tanda. Tracks you play by hand on a deck are not logged, and no empty session
  is left behind when you only prepare a set.
- History shows cortinas and performance tracks as they were when they played,
  even if you change their marks later.
- Deleting several History sessions at once now updates play counts, as
  deleting one did.

## Cortinas are remembered

- A track marked as a cortina stays a cortina after you restart TangoQ, until
  you unmark it. Performance marks, pause marks and display names are still
  cleared when TangoQ restarts.

## The queue checks your set

- A red **!** appears in place of the T, V, M or c mark where the queue does not
  read like a milonga: a cortina inside a tanda, a cortina whose genre is Tango,
  Vals or Milonga, two cortinas in a row, or two tandas with no cortina between
  them. Hover over the **!** to see why.
- "Make … tanda" on a selection of cortinas explains why it cannot, and offers
  to unmark them and make the tanda.

## Changing the set while it plays

- The playing track, and its tanda as a whole, stay where they are. Everything
  else can move freely, above or below, by dragging, with "Move tanda up/down",
  or with Alt+Up/Down (Option+Up/Down on a Mac). One message explains a refused
  move.
- Reordering the tracks inside a tanda keeps it a tanda, even the playing one.
  Moving a track out of a tanda, or into the middle of one, still ungroups it.
- Dropping a track between the playing track and the next one makes it play
  next.
- Using the same cortina in several places is safe: moving or adding a copy no
  longer makes the set repeat a track or move a pause to the wrong place.

## Decks and the cockpit

- A deck shows `[CORTINA]`, `[PERFORMANCE]` and `[PAUSE AFTER]` in a colour of
  their own (red, or dark red in High Contrast), and the song name keeps its
  normal colour. A cued performance shows `[PERFORMANCE, PAUSE AFTER]` right
  away.
- The set length, the projected end time and how far you are over or under your
  target time now sit in the HUD around the countdown, in full words.
- The "Fade Now" button is easier to find, and the target input reads "Target
  Time".
- Clocks follow your computer's 12-hour or 24-hour setting everywhere, and switch
  as soon as you change it.

## Library and settings

- The sidebar now reads Tracks, TangoQ, Playlists, Crates, History, Computer,
  your other DJ libraries, then Analyze. Recordings is no longer in the sidebar;
  recording still works, and your recordings are under Computer.
- Other DJ libraries (iTunes or Music, Traktor, Rekordbox, Serato and others) are
  shown as before. To hide the ones you do not use, open **Preferences ->
  Library -> External Libraries** and restart TangoQ.
- Preferences no longer shows pages tango DJs do not need: Effects, Live
  Broadcasting, Beat Detection, Vinyl Control and Modplug Decoder. The Library
  page no longer shows the History settings that TangoQ no longer uses. Your
  saved settings are unchanged.
- The default tanda colours are softer and easier on the eye. Existing installs
  get the new colours; colours you chose yourself are kept.
- The side panel shows in bold which playlists a selected track came from, and
  no longer scrolls sideways when you select a row.

## Fixes

- macOS, full screen: the track menu no longer opens under the hidden menu bar,
  where clicks did not reach it.

## Installation notes

- Windows and macOS installers are still unsigned, so the operating system shows
  a warning the first time. See [INSTALL.md](INSTALL.md) for the exact steps.
- Windows: run the 1.0.3 `.msi`; it replaces 1.0.2. macOS: quit TangoQ, drag
  TangoQ 1.0.3 into **Applications** and choose **Replace**.
- Keep `tangoq.db.pre-tangoq-schema-2.bak` until you are happy with 1.0.3. If you
  ever need to go back to 1.0.2: on Windows, uninstall 1.0.3 and then install
  1.0.2; on macOS, replace the app with 1.0.2. It opens the upgraded database.
