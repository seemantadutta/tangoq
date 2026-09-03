# TangoQ 1.0.2 release checklist

This is the release gate for the first official TangoQ build. Run it against the
packaged Windows and macOS installers, not only an in-tree development build.
The comprehensive historical Tango Mode catalog remains available in
[`../tangomode_manual_test_cases.md`](../tangomode_manual_test_cases.md) when a
change needs deeper subsystem coverage.

Record the tested commit, platform, artifact filename, and result for each run.
A failure in a **release blocker** must be fixed or explicitly accepted before
publishing.

## 1. Source and CI gate

- [ ] The release commit is on `main` with no uncommitted release changes.
- [ ] `TANGOQ_VERSION` is `1.0.2`; the Mixxx base remains `2.5.6` and the TangoQ
      configuration schema remains `1`.
- [ ] Pull-request CI passes formatting, static analysis, tests, and all package
      builds.
- [ ] The Windows MSI, macOS Intel DMG, and macOS Apple Silicon DMG are present
      and named `tangoq-*`, not `mixxx-*`.
- [ ] `RELEASE_NOTES.md`, `INSTALL.md`, and the GitHub release text agree about
      supported packages, unsigned installers, and known limitations.

## 2. Fresh-install smoke test — release blocker

Use a machine or temporary account with no TangoQ settings. A stock Mixxx
installation may remain present and must not be changed.

- [ ] Install and launch TangoQ using the steps in `INSTALL.md`.
- [ ] The application, executable/bundle, menus, About dialog, and installer say
      **TangoQ 1.0.2**.
- [ ] TangoQ creates its own `tangoq.cfg`, `tangoq.db`, and `tangoq.log` in the
      documented TangoQ settings directory. The Mixxx settings directory and
      files remain untouched.
- [ ] The new config records `[Config] Version` as `1.0.2` and
      `TangoQConfigVersion` as `1`.
- [ ] The first window is centered with a landscape layout and the tango-focused
      queue columns.
- [ ] Select an audio output, load a track on each deck, play audio, stop, and
      quit normally without a crash.

## 3. Early-access upgrade smoke test — release blocker

Back up the early-access settings directory before this test. Use a 1.0.1 config
with deliberately non-default waveform type and frame-rate values so accidental
resetting is visible.

- [ ] On Windows, uninstall 1.0.1 before installing 1.0.2. Confirm uninstalling
      the application leaves the early-access settings directory intact.
- [ ] On macOS, replace the 1.0.1 application bundle with 1.0.2.
- [ ] Launch 1.0.2 twice.
- [ ] The customized settings remain unchanged; the config gains product version
      `1.0.2` and config schema `1`.
- [ ] The first launch records `outcome=adopted` in `tangoq.log`; the second does
      not repeat a legacy Mixxx migration.
- [ ] The existing TangoQ library database opens and stored tracks, playlists,
      and cues remain available.
- [ ] Stock Mixxx still starts with its own settings and database.

## 4. Core milonga workflow — release blocker

Prepare a short set containing two tandas, a cortina, a performance track, and a
pause-after-track marker. Use short audio files or start near their ends.

- [ ] Group and label Tango, Vals, Milonga, and Alt/Nuevo tandas. Rename one
      tanda and collapse it.
- [ ] Tanda headers show `T`, `V`, `M`, or `N`; cortina and performance rows show
      `c` and `p`. The playing item adds `▶` without displacing its type marker.
- [ ] The playing marker appears on both the playing track and its tanda header.
      A collapsed header retains its progress pips.
- [ ] Disable queue color coding. All queue text returns to normal styling, the
      playing marker remains visible, saved color selections are retained, and
      the preferences layout does not resize or flicker.
- [ ] Re-enable colors and confirm each configured tanda/special-track color is
      applied only where expected.
- [ ] Tanda Transition honors the Set Start marker and inserts the configured
      silent gap without replaying skipped lead-in audio.
- [ ] A cortina follows its configured fade envelope and hands off cleanly.
- [ ] When a pause follows the current track, the HUD describes the pause rather
      than showing `Next track in MM:SS`. Resume and confirm the next track starts
      normally.
- [ ] Auto DJ keeps played rows, follows live edits without losing its place, and
      stops only after the final track finishes.
- [ ] LIVE mode blocks the protected stop/reorder actions and exits cleanly.
- [ ] Quit with tracks loaded on both decks, relaunch, and confirm there was no
      shutdown crash.

## 5. Appearance and platform packaging

- [ ] Switch between the default and High Contrast schemes. Text, controls,
      waveforms, queue markers, and selected rows remain readable.
- [ ] Windows: TangoQ installs beside stock Mixxx, uses its own Start menu entry,
      and upgrading TangoQ does not modify the Mixxx installation.
- [ ] macOS Intel: mount the DMG, drag `TangoQ.app` to Applications, complete the
      documented Gatekeeper flow, and launch successfully.
- [ ] macOS Apple Silicon: repeat the native ARM installation and launch test.
- [ ] On macOS, Finder, Dock, About, microphone permission text, and the mounted
      DMG identify TangoQ.

## 6. Publish

- [ ] Merge the release-preparation PR and tag the exact release commit as
      `1.0.2` (without a `v`; that is the tag pattern used by the workflow).
- [ ] Confirm the tag workflow succeeds and download its packaged artifacts.
- [ ] Calculate SHA-256 hashes for every published MSI/DMG and include them on
      the GitHub release page.
- [ ] Create the GitHub release for tag `1.0.2`, paste the release notes, attach
      the three installers, and verify each attachment downloads successfully.
- [ ] Install one downloaded release attachment and confirm its About dialog says
      `1.0.2`; this catches uploading an artifact from the wrong workflow.
- [ ] Keep a copy of the final artifact names, hashes, and tested commit in the
      release record.
