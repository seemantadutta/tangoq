# TangoQ

**Argentine Tango DJ Software, based on [Mixxx].**

TangoQ keeps Mixxx's familiar deck-based DJ interface and focuses it on milonga
workflow: ordered tanda queues, cortina handling, set timing, live-mode safety,
and waveform markers that help a tango DJ stay oriented while performing.

> **Not affiliated with the Mixxx project.** TangoQ is a community-maintained
> fork based on Mixxx, not an official Mixxx release. Please report TangoQ
> problems [here](https://github.com/seemantadutta/tangoq/issues), not to the
> upstream Mixxx project.

## Install

See **[INSTALL.md](INSTALL.md)** for step-by-step instructions, including how to
get past the Windows SmartScreen and macOS Gatekeeper warnings. Current builds
are unsigned, so both operating systems will warn the first time.

Downloads are attached to the [Releases](https://github.com/seemantadutta/tangoq/releases)
page. Windows packages are named `tangoq-*.msi`; macOS packages are named
`tangoq-*.dmg`.

## Tango Workflow

- The queue plays top to bottom and played tracks stay visible.
- Cortinas can be tagged and faded with a dedicated cortina envelope.
- Set length, end time, and remaining-time feedback stay visible.
- Pause-after-track, row display names, duplicate warnings, and live-mode
  safeguards support real-room DJing.
- FAS, S, LAS, and absolute-start waveform markers make start-position decisions
  visible.

For the current manual checklist, see
[tangomode_manual_test_cases.md](tangomode_manual_test_cases.md). The file name
will be cleaned up in a later pass with the rest of the behavior terminology.

## Building From Source

Same as Mixxx, from this fork:

```sh
git clone https://github.com/seemantadutta/tangoq.git
cd mixxx
mkdir build
cd build
cmake ..
cmake --build .
```

On Windows the executable is `tangoq.exe`. On Linux it is `tangoq`. On macOS the
bundle is `TangoQ.app` with bundle identifier `io.github.seemantadutta.tangoq`.
Bundled libraries and internal build targets may still use Mixxx names.

`tools/tangoq_build_macos.sh` wraps the macOS configure/build loop with local
defaults for this fork.

## Relationship To Mixxx

TangoQ is based on Mixxx and remains GPLv2 software. The Mixxx project deserves
clear attribution, and upstream Mixxx credits, copyright notices, dependency
names, internal APIs, and licenses are preserved.

The GPL covers the source code. It does not grant rights to the Mixxx name or
logo, which is why this fork carries its own product name and makes the
relationship explicit.

If you find Mixxx useful, [support the people who make it](https://mixxx.org/donate).

[mixxx]: https://mixxx.org
