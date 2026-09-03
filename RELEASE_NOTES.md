# TangoQ 1.0.2

TangoQ 1.0.2 is the first official TangoQ release. It is purpose-built for
Argentine tango DJing and is based on Mixxx 2.5.6. The earlier 1.0.1 builds were
limited early-access builds.

> **Upgrading from early-access TangoQ 1.0.1:** On Windows, uninstall TangoQ
> 1.0.1 before installing 1.0.2. On macOS, quit TangoQ and replace the existing
> TangoQ application when installing 1.0.2. Do not delete your TangoQ settings
> folder; your library, cues, playlists, and preferences will carry forward.
> Do not reinstall TangoQ 1.0.1 after upgrading.

## Tango workflow

- The TangoQ queue plays a prepared set from top to bottom, keeps played tracks
  visible, and stops at the end.
- Tracks can be grouped into Tango, Vals, Milonga, and Alt/Nuevo tandas. Tanda
  headers support custom names and retain progress pips when collapsed.
- Tanda Transition uses a real silent gap between tracks and respects a DJ-set
  start marker. Cortinas use their dedicated fade envelope.
- Cortinas, performance tracks, and pause-after-track markers support
  announcements and other changes in the flow of a milonga.
- The cockpit shows set timing, target-end comparison, the next transition, and
  tanda progress. When a pause follows the current track, the HUD describes the
  pause instead of promising that the next track will start automatically.
- LIVE mode and queue lockdown protect a running set from accidental stops,
  reordering, replacement, or randomization.

## Queue orientation and appearance

- Tanda headers use configurable fixed colors for Tango, Vals, Milonga, and
  Alt/Nuevo. Cortinas and performance tracks have their own colors.
- Color coding can be disabled without losing the selected colors. The layout
  remains stable when the color controls are disabled.
- A dedicated `▶` marker identifies the currently playing tanda header and
  track. The type markers (`T`, `V`, `M`, `N`, `c`, and `p`) remain visible.
- A Sunrise-derived **High Contrast** scheme provides a light work surface for
  daylight and other high-ambient-light environments.
- Fresh installs use a tango-focused column layout, a landscape window, and a
  centered initial position. Library text size can be adjusted from the UI or
  with Ctrl+mouse-wheel.

## Product and installer improvements

- The application, sidebar, preferences, menus, installer artwork, and package
  metadata use the TangoQ name and icon.
- TangoQ uses its own settings directory, database name, macOS bundle identity,
  and Windows installer UpgradeCode. It can be installed alongside stock Mixxx
  without taking over Mixxx's settings or Windows installer identity.
- The TangoQ queue ignores track double-clicks and removes the disruptive
  add-to-top and replace-queue actions from its menus.
- A startup-window close crash has been fixed.

## Configuration upgrades

- TangoQ configuration migration is independent of the Mixxx product version.
  `[Config] TangoQConfigVersion` starts at schema 1, while `[Config] Version`
  records the TangoQ product release.
- An existing 1.0.1 or other pre-schema TangoQ configuration is adopted without
  resetting customized waveform, frame-rate, or other settings.
- A configuration written by a newer unsupported TangoQ schema is rejected
  before normal startup and is not overwritten.
- Every launch records a compact migration result in `tangoq.log`. Debug logging
  can be enabled when a field problem needs more detail.
- The library database schema and the explicit Mixxx database-import behavior
  are unchanged by this configuration fix.

## Installation notes

- Windows and macOS installers are currently unsigned, so the operating system
  will display a first-launch warning. See [INSTALL.md](INSTALL.md) for the exact
  installation steps.
- **Windows users upgrading from early-access TangoQ 1.0.1 must uninstall
  1.0.1 before installing 1.0.2.** The installer identity changed so TangoQ can
  coexist with Mixxx, and Windows cannot upgrade that early build in place.
  Uninstalling the application preserves the TangoQ settings and library.
- On macOS, quit TangoQ and drag TangoQ 1.0.2 into **Applications**, choosing
  **Replace** when prompted. A separate uninstall is not required.
- After upgrading, do not reinstall or downgrade to the unsupported TangoQ
  1.0.1 build.
- Cortina tags, performance labels, pause markers, and LIVE mode are session
  annotations and are cleared when TangoQ restarts. The underlying queue and
  library database persist normally.
- External-display and OBS export is not included in this release.

Before publishing, complete the packaged-build checks in
[docs/release-1.0.2-checklist.md](docs/release-1.0.2-checklist.md).
