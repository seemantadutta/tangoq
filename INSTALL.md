# Installing Mixxx — Tango DJ Mode build

This is a community build of Mixxx with **Tango DJ mode** (see
[`RELEASE_NOTES.md`](RELEASE_NOTES.md)), based on **Mixxx 2.5.6**.

> **Heads up — these installers are not code-signed.** This is a free,
> personally-maintained build, so Apple and Microsoft haven't been paid to
> "certify" it. Your computer will show a scary-looking warning the first time
> you open it. That's expected — the steps below tell you how to get past it.
> If you'd rather wait, this feature is also being proposed for official Mixxx.

---

## 1. Download

Go to the **[Releases page](https://github.com/seemantadutta/mixxx/releases)**
and download the file for your system:

| Your computer | Download this file |
|---|---|
| **Windows** (10 or 11) | `mixxx-*-x64.msi` |
| **Mac — Apple Silicon** (M1/M2/M3/M4) | `mixxx-*-macosarm.dmg` |
| **Mac — Intel** | `mixxx-*-macosintel.dmg` |

**Not sure which Mac you have?** Click the  (Apple) menu in the top-left →
**About This Mac**. If it says **Chip: Apple M…**, you have Apple Silicon. If it
says **Processor: Intel**, you have an Intel Mac.

---

## 2. Install on Windows

1. Double-click the downloaded **`.msi`** file.
2. You'll likely see a blue **"Windows protected your PC"** box.
   - Click **More info**.
   - Click **Run anyway**.
3. Follow the installer prompts. Mixxx then appears in your Start menu.

That blue box appears because the installer isn't signed — it is **not** a sign
that anything is wrong.

---

## 3. Install on Mac

1. Double-click the downloaded **`.dmg`**.
2. Drag the **Mixxx** icon onto the **Applications** folder.
3. Open **Applications** and try to launch **Mixxx**. macOS will block it the
   first time with a message like *"Mixxx can't be opened because Apple cannot
   check it for malicious software."* Use the steps for your macOS version:

### macOS Sequoia (15) and newer
1. Try to open Mixxx (it gets blocked — that's fine, click **Done**).
2. Open  (Apple menu) → **System Settings** → **Privacy & Security**.
3. Scroll down to the **Security** section. You'll see a line about *"Mixxx was
   blocked…"* — click **Open Anyway**.
4. Confirm with your password / Touch ID, then click **Open Anyway** again.

### macOS Sonoma (14) and earlier
1. In **Applications**, **right-click** (or Control-click) **Mixxx** → **Open**.
2. In the dialog, click **Open**.

You only need to do this **once**; afterward Mixxx opens normally.

### If it still won't open (reliable fallback)
Open **Terminal** (Applications → Utilities → Terminal), paste this line, and
press Return:

```sh
xattr -dr com.apple.quarantine /Applications/Mixxx.app
```

Then open Mixxx normally. This removes the "downloaded from the internet" flag
that triggers the block.

### First launch: say "Allow" to the permission box

The first time Mixxx opens, macOS asks whether it may **access data from other
apps**. Click **Allow**.

Mixxx needs this for two things:

- to create the folder where it keeps your settings and music library
- to import your existing collection from Rekordbox, Serato, Traktor or Music.app

**If you click "Don't Allow"**, Mixxx shows a confusing message mentioning
*SQLite* and a single **OK** button, and then quits. Nothing is broken and your
download is fine — Mixxx simply wasn't allowed to create its library file.

To fix it, open  → **System Settings** → **Privacy & Security** → **Files and
Folders**, find **Mixxx** in the list, and switch it on. Then open Mixxx again.

If Mixxx isn't listed there, open **Terminal** (Applications → Utilities →
Terminal), paste this line and press Return:

```sh
tccutil reset SystemPolicyAppData org.mixxx.mixxx
```

That makes macOS ask again the next time you open Mixxx — this time choose
**Allow**.

### Already using standard Mixxx? Bring your library across

This build keeps its settings separate from a standard Mixxx install, so the two
never interfere with each other. To start with a copy of your existing crates,
playlists, history and cue points, close both apps and run this in **Terminal**:

```sh
cp -R ~/Library/Application\ Support/Mixxx ~/Library/Application\ Support/TangoMode
```

This **copies** your library — your standard Mixxx install keeps working exactly
as before, and anything you do in this build cannot affect it.

---

## 4. (Optional) Verify your download

If you'd like to confirm the file downloaded correctly and wasn't tampered with,
compare its **SHA-256 checksum** to the values published on the Releases page.

**Windows** (PowerShell):
```powershell
Get-FileHash .\mixxx-*-x64.msi -Algorithm SHA256
```

**Mac** (Terminal):
```sh
shasum -a 256 ~/Downloads/mixxx-*.dmg
```

The printed value should match the checksum listed for that file in the release
notes. (This is an integrity check only — it's optional and doesn't change the
install steps above.)

<!-- Maintainer: paste the actual checksums here when cutting the release, e.g.
SHA-256 checksums:
  mixxx-<ver>-x64.msi          <hash>
  mixxx-<ver>-macosintel.dmg   <hash>
  mixxx-<ver>-macosarm.dmg     <hash>
-->

---

## 5. "Is this safe?"

These warnings mean *"this app wasn't signed with a paid Apple/Microsoft
certificate,"* **not** *"this app is dangerous."* The full source code is public
in this repository, and the installers are built automatically by GitHub from
that code. If you're ever unsure, you can build it yourself from source.

---

## 6. Getting help / reporting problems

If something doesn't work or a track behaves oddly in Tango mode, please open an
issue: **https://github.com/seemantadutta/mixxx/issues** — include your OS, what
you did, and what happened. Screenshots help.

---

## Requirements
- **Windows:** Windows 10 or 11 (64-bit).
- **Mac:** a recent macOS (Apple Silicon or Intel; pick the matching download).
- A sound card/output, and ideally headphones + a controller or mixer for
  cueing — same as standard Mixxx.
