# Installing TangoQ

**TangoQ** is Argentine Tango DJ Software based on **Mixxx 2.5.6**.

> **Heads up: these installers are not code-signed.** This is a free,
> personally maintained build, so Apple and Microsoft will warn the first time
> you open it. The steps below explain how to proceed.

## 1. Download

Go to the **[Releases page](https://github.com/seemantadutta/tangoq/releases)**
and download the file for your system:

| Your computer | Download this file |
|---|---|
| **Windows** (10 or 11) | `tangoq-*.msi` |
| **Mac - Apple Silicon** (M1/M2/M3/M4) | `tangoq-*-arm64.dmg` |
| **Mac - Intel** | `tangoq-*-x86_64.dmg` |

## 2. Upgrading From Early-Access TangoQ 1.0.1

First quit TangoQ. Your library, cues, playlists, and preferences are stored
separately from the application and are preserved by the steps below.

**Windows:** uninstall TangoQ 1.0.1 before installing 1.0.2. Open **Settings ->
Apps -> Installed apps**, find TangoQ, and choose **Uninstall**. Then install the
1.0.2 `.msi` normally. Do not delete `%LOCALAPPDATA%\TangoQ`; TangoQ 1.0.2 will
adopt the existing settings and database there.

**macOS:** no separate uninstall is required. Drag TangoQ 1.0.2 into
**Applications** and choose **Replace** when Finder asks. Alternatively, move
the old `TangoQ.app` to the Trash before copying the new one. Do not delete the
TangoQ folder under `~/Library/Containers`; it contains your settings and
database.

After opening 1.0.2, do not reinstall or downgrade to the unsupported 1.0.1
build.

## 3. Install On Windows

1. Double-click the downloaded **`.msi`** file.
2. If Windows SmartScreen appears, click **More info**, then **Run anyway**.
3. Follow the installer prompts. TangoQ then appears in your Start menu.

## 4. Install On Mac

1. Double-click the downloaded **`.dmg`**.
2. Drag the **TangoQ** icon onto the **Applications** folder.
3. Open **Applications** and launch **TangoQ**. If macOS blocks it, use one of
   the approval flows below.

### macOS Sequoia (15) And Newer

1. Try to open TangoQ. When it is blocked, click **Done**.
2. Open Apple menu -> **System Settings** -> **Privacy & Security**.
3. In **Security**, click **Open Anyway** for TangoQ.
4. Confirm with your password or Touch ID, then click **Open Anyway** again.

### macOS Sonoma (14) And Earlier

1. In **Applications**, right-click or Control-click **TangoQ** -> **Open**.
2. In the dialog, click **Open**.

### If It Still Will Not Open

Open **Terminal**, paste this line, and press Return:

```sh
xattr -dr com.apple.quarantine /Applications/TangoQ.app
```

Then open TangoQ normally.

## 5. First Launch

TangoQ asks you to choose your music folder the first time it opens. Pick the
folder your music lives in and click **Open**. You can add more folders later in
**Preferences -> Library**.

TangoQ keeps its own settings and library separate from standard Mixxx:

- Windows: `%LOCALAPPDATA%\TangoQ`
- macOS: `~/Library/Containers/io.github.seemantadutta.tangoq/Data/Library/Application Support/TangoQ`
- Linux: `~/.tangoq`

The new database is `tangoq.db`, the config file is `tangoq.cfg`, and the log is
`tangoq.log`.

## 6. Optional Checksum Verification

**Windows**:

```powershell
Get-FileHash .\tangoq-*.msi -Algorithm SHA256
```

**Mac**:

```sh
shasum -a 256 ~/Downloads/tangoq-*.dmg
```

Compare the printed value to the checksum published on the release page.

## 7. What This Is

TangoQ is a community-maintained fork based on
[Mixxx](https://mixxx.org). It is not produced, supported, or endorsed by the
Mixxx project. Please report TangoQ problems at
<https://github.com/seemantadutta/tangoq/issues>.

Mixxx is free software under the GPL. TangoQ keeps the license and copyright
notices intact, uses its own name and branding, and links back to the original
project.
