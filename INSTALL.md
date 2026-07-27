# Installing TangoMode — Tango DJ mode for Mixxx

**TangoMode** is a community build of Mixxx with **Tango DJ mode** (see
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
| **Windows** (10 or 11) | `tangomode-*.msi` |
| **Mac — Apple Silicon** (M1/M2/M3/M4) | `tangomode-*-arm64.dmg` |
| **Mac — Intel** | `tangomode-*-x86_64.dmg` |

**Not sure which Mac you have?** Click the  (Apple) menu in the top-left →
**About This Mac**. If it says **Chip: Apple M…**, you have Apple Silicon. If it
says **Processor: Intel**, you have an Intel Mac.

---

## 2. Install on Windows

1. Double-click the downloaded **`.msi`** file.
2. You'll likely see a blue **"Windows protected your PC"** box.
   - Click **More info**.
   - Click **Run anyway**.
3. Follow the installer prompts. TangoMode then appears in your Start menu.

That blue box appears because the installer isn't signed — it is **not** a sign
that anything is wrong.

---

## 3. Install on Mac

1. Double-click the downloaded **`.dmg`**.
2. Drag the **TangoMode** icon onto the **Applications** folder.
3. Open **Applications** and try to launch **TangoMode**. macOS will block it the
   first time with a message like *"TangoMode can't be opened because Apple cannot
   check it for malicious software."* Use the steps for your macOS version:

### macOS Sequoia (15) and newer
1. Try to open TangoMode (it gets blocked — that's fine, click **Done**).
2. Open  (Apple menu) → **System Settings** → **Privacy & Security**.
3. Scroll down to the **Security** section. You'll see a line about *"TangoMode was
   blocked…"* — click **Open Anyway**.
4. Confirm with your password / Touch ID, then click **Open Anyway** again.

### macOS Sonoma (14) and earlier
1. In **Applications**, **right-click** (or Control-click) **TangoMode** → **Open**.
2. In the dialog, click **Open**.

You only need to do this **once**; afterward TangoMode opens normally.

### If it still won't open (reliable fallback)
Open **Terminal** (Applications → Utilities → Terminal), paste this line, and
press Return:

```sh
xattr -dr com.apple.quarantine /Applications/TangoMode.app
```

Then open TangoMode normally. This removes the "downloaded from the internet" flag
that triggers the block.

### First launch

TangoMode asks you to **choose your music folder** the first time it opens. Pick
the folder your music lives in and click **Open** — it then scans that folder and
builds its library. You can add more folders later in **Preferences → Library**.

macOS may also ask whether TangoMode can **access data from other apps**. That
permission is only used to import an existing collection from Rekordbox, Serato,
Traktor or Music.app. Declining it is fine if you don't need that; TangoMode keeps
its own settings in its own place either way.

If TangoMode ever reports that it *could not create the folder it keeps your
settings in*, that is a permission problem rather than a damaged download. Open
 → **System Settings** → **Privacy & Security** → **Files and Folders**, find
**TangoMode**, and switch it on. If it isn't listed, open **Terminal**
(Applications → Utilities → Terminal), run the line below, then open TangoMode
again and choose **Allow**:

```sh
tccutil reset SystemPolicyAppData io.github.seemantadutta.tangomode
```

### Already using standard Mixxx? Bring your library across

TangoMode keeps its settings completely separate from a standard Mixxx install,
so the two never interfere with each other — but that also means it starts with an
empty library.

To begin with a copy of your existing crates, playlists, history and cue points
instead: quit both apps, **open TangoMode once and close it again** (so it creates
its folder), then run this in **Terminal**:

```sh
cp ~/Library/Containers/org.mixxx.mixxx/Data/Library/Application\ Support/Mixxx/mixxxdb.sqlite \
   ~/Library/Containers/io.github.seemantadutta.tangomode/Data/Library/Application\ Support/TangoMode/
```

Those long paths are not a mistake: macOS stores each app's settings inside its own
private container folder.

This **copies** your library — your standard Mixxx install keeps working exactly
as before, and nothing you do in TangoMode can affect it.

---

## 4. (Optional) Verify your download

If you'd like to confirm the file downloaded correctly and wasn't tampered with,
compare its **SHA-256 checksum** to the values published on the Releases page.

**Windows** (PowerShell):
```powershell
Get-FileHash .\tangomode-*.msi -Algorithm SHA256
```

**Mac** (Terminal):
```sh
shasum -a 256 ~/Downloads/tangomode-*.dmg
```

The printed value should match the checksum listed for that file in the release
notes. (This is an integrity check only — it's optional and doesn't change the
install steps above.)

<!-- Maintainer: paste the actual checksums here when cutting the release, e.g.
SHA-256 checksums:
  tangomode-<ver>.msi           <hash>
  tangomode-<ver>-x86_64.dmg    <hash>
  tangomode-<ver>-arm64.dmg     <hash>
-->

---

## 5. "Is this safe?"

These warnings mean *"this app wasn't signed with a paid Apple/Microsoft
certificate,"* **not** *"this app is dangerous."* The full source code is public
in this repository, and the installers are built automatically by GitHub from
that code. If you're ever unsure, you can build it yourself from source.

### What this is, and what it isn't

TangoMode is an **unofficial community build** — a modified version of
[Mixxx](https://mixxx.org) with features added for Argentine tango DJing. It is
**not** produced or endorsed by the Mixxx project, and problems with it should be
reported here rather than to them.

Mixxx is free software under the **GPL**, which is what makes this build legal to
share: the modified source is public, and the licence and copyright notices are
kept intact. The GPL covers the *code* — it does not grant rights to the Mixxx
name or logo, which is why this build is named and presented as its own thing and
links back to the original.

If you want official, signed builds with vendor support, get Mixxx from
[mixxx.org](https://mixxx.org).

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
