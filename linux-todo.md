# Linux packaging TODO: remaining Mixxx branding

Branding cleanup for the Linux packages (`.deb` and Flatpak), deferred while the
focus is the Windows early-access bundle. The Windows MSI is already clean
(TangoQ dialog/banner bitmaps, TangoQ license header, TangoQ version resource and
icons). macOS was already TangoQ-branded (`TangoQ.app`,
`io.github.seemantadutta.tangoq`).

Scope tags below: **[deb]** Debian-only, **[shared]** affects both the `.deb` and
the Flatpak (common Linux assets), **[flatpak]** Flatpak-only.

## High priority (user-visible)

- [ ] **[deb] Man page is Mixxx, and misnamed.** The `.deb` ships `mixxx.1`,
  generated from `packaging/debian/mixxx.sgml`, which is pure Mixxx (title
  "MIXXX", refpurpose "A Digital Disc Jockey Interface", `mixxx.org` links,
  maintainer `s.brandt@mixxx.org`). Because it installs as `mixxx.1`,
  `man tangoq` finds nothing while `man mixxx` shows a Mixxx page.
  - Rename the man page to `tangoq.1` and update the references in
    `packaging/CPackDebInstall.cmake` (the `mixxx.sgml` -> `mixxx.1` build step,
    the `file(REMOVE ... mixxx.1)`, and `dh_installman`).
  - Rebrand the SGML content (title, refpurpose, description, URLs, maintainer).

- [ ] **[shared] Software-center screenshots show stock Mixxx.** In
  `res/linux/io.github.seemantadutta.tangoq.metainfo.xml`, the `<screenshots>`
  link to `https://mixxx.org/theme/images/...` (stock Mixxx skins), each
  captioned "TangoQ with the ... skin". GNOME Software / KDE Discover would show
  Mixxx's UI labeled as TangoQ.
  - Replace with real TangoQ screenshots on a stable URL, or remove the
    `<screenshots>` block until we have them.

- [ ] **[shared] Metainfo links route users to Mixxx.** In the same metainfo
  file: `donation` -> `mixxx.org/donate`, `help` -> `mixxx.org/support`,
  `translate` -> Mixxx Transifex, `contact` -> `mixxx.zulipchat.com`.
  - Fix the **donation** link first (donations currently go to Mixxx, not us):
    repoint or remove.
  - Decide whether help / translate / contact should stay pointed at Mixxx (the
    underlying app is Mixxx) or be removed.

## Low priority (cosmetic / not user-visible)

- [ ] **[shared] Orphaned Mixxx icons installed into hicolor.** The directory
  install in `CMakeLists.txt` (around line 2066) copies all of
  `res/images/icons/` except `ic_mixxx.ico`, so `mixxx.png`, `mixxx.svg`,
  `mixxx_macos.svg`, and `mixxx_ios.svg` land in the user's icon theme unused
  (only the TangoQ `.desktop` referencing `Icon=tangoq` is installed). Add
  `PATTERN ... EXCLUDE` lines to drop them, or leave (harmless).

- [ ] **[shared] `.desktop` GenericName.** In
  `io.github.seemantadutta.tangoq.desktop`, the default `GenericName` holds the
  full tagline while the localized `GenericName[..]` entries are the generic
  "Digital DJ system". Optional: make the default a short generic name too.

- [ ] **[flatpak] `packaging/flatpak/repo.flatpakrepo` is fully Mixxx.**
  Title=Mixxx, Url=`downloads.mixxx.org/flatpak/`, Homepage=`mixxx.org`,
  Icon=mixxx-logo. If we ever publish a TangoQ Flatpak repo, replace it; if not,
  remove the file.

- [ ] **[deb] udev rule name.** The rule is named `mixxx-usb-uaccess`
  (`res/linux/mixxx-usb-uaccess.rules`, installed via `dh_installudev`). Internal
  system file, and the metainfo cross-references it, so rename only with care.

- [ ] **[deb] `CPACK_DEBIAN_PACKAGE_REPLACES "mixxx-data"`** (CMakeLists.txt).
  Makes the TangoQ `.deb` replace a stock `mixxx-data` package, which affects
  users who also have stock Mixxx installed. Confirm this is intended.
