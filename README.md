# TangoMode

**An unofficial build of [Mixxx] with features for Argentine tango DJing.**

TangoMode is [Mixxx] — the free, open-source DJ software — with one extra switch.
Turn **Tango DJ Mode** on and Auto DJ stops behaving like a club mixing list and
starts behaving like a milonga set: the queue plays **in order**, played tracks
**stay in the list**, cortinas are handled properly, and the toolbar tells you
whether you will finish on time.

Turn it off and you have ordinary Mixxx, unchanged.

> **Not affiliated with the Mixxx project.** This is a community build, not
> produced or endorsed by [the Mixxx team](https://mixxx.org). Please report
> problems with it [here](https://github.com/seemantadutta/mixxx/issues), not to
> them. If you want official, signed builds for club style DJing functions, get Mixxx from
> [mixxx.org](https://mixxx.org).

---

## Install

See **[INSTALL.md](INSTALL.md)** for step-by-step instructions, including how to
get past the Windows SmartScreen and macOS Gatekeeper warnings — the builds are
unsigned, so both operating systems will complain the first time.

Downloads are attached to the [Releases](https://github.com/seemantadutta/mixxx/releases)
page. Windows `.msi` and macOS `.dmg` (Intel and Apple Silicon).

---

## What Tango mode adds

Everything below is behind the single **Tango DJ Mode** switch in
*Preferences → Auto DJ* (or `Ctrl+Alt+Shift+T`, `⌘⌥⇧T` on macOS).

**The set plays as you arranged it**
- The queue plays top to bottom and nothing is deleted by playing it, so you can
  see what you have played and what is coming
- The cursor stays anchored to the playing track while you edit the rest of the
  queue — you build the set during the milonga, not before it
- Auto DJ stops at the end of the queue rather than picking tracks at random
- The reordering actions that could wipe a queue by accident are locked out

**Cortinas**
- Tag any track as a cortina; it shows blue in the queue and on the deck
- **Cortina Fade** plays a cortina as a proper envelope — silence, fade in, hold,
  fade out, silence — instead of a hard cut you have to ride manually
- Cortina length is budgeted separately for timing, and can be nudged live

**Will I finish on time?**
- **Set Length**, **Ends** and **Left** for everything queued
- Set a target end time and see how far over or under you are running

**Playing to a room**
- **Pause after this track** — mark any row and the set stops there for an
  announcement, then carries on when you press play. A red line in the queue shows
  where it will stop
- **Display names** — rename a row for the night, e.g. `[PERF] …`, without
  touching the file's tags
- Duplicate tracks are flagged in amber (repeated cortinas are not, because that
  is normal)
- **LIVE mode** — while performing, stopping Auto DJ takes two presses and the
  deck play/pause keys are disabled, so one stray keystroke cannot kill the floor
- A dockable Auto DJ queue panel that stays visible while you browse

For the full behaviour, see [tangomode_manual_test_cases.md](tangomode_manual_test_cases.md).

---

## Bugs and requests

| What | Where |
|---|---|
| Anything Tango-specific | [This tracker](https://github.com/seemantadutta/mixxx/issues) |
| A bug in Mixxx itself, present with Tango mode **off** | [Upstream][issues] — please reproduce on an official build first |

Mixxx's [Zulip][zulip] and [forums][discourse] are the Mixxx community's, not a
support channel for this build. Please don't take TangoMode problems there.

---

## Building from source

Same as Mixxx, from this fork:

    $ git clone https://github.com/seemantadutta/mixxx.git
    $ cd mixxx

Set up the build environment for your platform:

| Platform | Command | Requirements |
| -- | ------- | ------------ |
| Windows | `tools\windows_buildenv.bat` | ~2.5 GB download, ~9 GB disk space |
| macOS | `source tools/macos_buildenv.sh setup` | ~1.5 GB download, ~3 GB disk space |
| Debian/Ubuntu | `tools/debian_buildenv.sh setup` | ~200 MB download, ~1 GB disk space |
| Fedora | `tools/rpm_buildenv.sh setup` | ~200 MB download, ~1 GB disk space |
| Flatpak | `tools/flatpak_buildenv.sh setup` | ~2.6 GB download, ~5 GB disk space |
| Other Linux distros | See the [wiki article](https://github.com/mixxxdj/mixxx/wiki/Compiling%20on%20Linux) | |

Then:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ cmake --build .

**On macOS, build with `tools/tangomode_build_macos.sh` instead.** It passes the
fork's bundle name and identifier, producing `TangoMode.app` and keeping its
settings separate from any Mixxx you already have installed. A plain `cmake ..`
leaves both at their upstream defaults and gives you `Mixxx.app` sharing Mixxx's
settings container.

The Windows executable is `tangomode`; on Linux it is `mixxx`, as upstream.
Bundled libraries keep their original names on every platform.

Upstream's [detailed build instructions](https://github.com/mixxxdj/mixxx/wiki#compile-mixxx-from-source-code)
apply unchanged, as does [packaging/flatpak/README.md](packaging/flatpak/README.md).

---

## Documentation

TangoMode is Mixxx, so the Mixxx documentation covers nearly all of it:

- [Mixxx manual][manual] — everything except the Tango features
- [Mixxx wiki][wiki]
- [Hardware compatibility]
- [Creating skins]

Tango-specific behaviour is documented in this repository.

---

## Relationship to Mixxx

TangoMode tracks a Mixxx release rather than its development branch, so it
inherits a stable base. The Tango work is kept in its own commits with the aim of
proposing the generally useful parts upstream — which would be the better outcome
for everyone, since official builds are signed and supported.

If you find Mixxx useful, [support the people who make it](https://mixxx.org/donate).

## License

Mixxx is released under the **GPLv2**, and so is this build. See the
[LICENSE](LICENSE) file for a full copy of the license.

The GPL covers the source code. It does not grant rights to the Mixxx name or
logo, which is why this build carries its own name, states plainly that it is
unofficial, and links back to the original project.

Original Mixxx copyright is held by the Mixxx developers; see the source headers
and [CONTRIBUTING](CONTRIBUTING.md).

[mixxx]: https://mixxx.org
[issues]: https://github.com/mixxxdj/mixxx/issues
[manual]: https://manual.mixxx.org/
[wiki]: https://github.com/mixxxdj/mixxx/wiki
[creating skins]: https://mixxx.org/wiki/doku.php/Creating-Skins
[hardware compatibility]: https://manual.mixxx.org/2.3/en/hardware/manuals.html
[zulip]: https://mixxx.zulipchat.com/
[discourse]: https://mixxx.discourse.group/
