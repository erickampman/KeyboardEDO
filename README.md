# KeyboardEDO

An on-screen MIDI 2.0 keyboard from Unlikelyware for any equal division of the octave: 12-EDO, 17,
19, 22, 31, 53 or any other from 5 to 72 steps. Play it with a mouse, a trackpad, a touchscreen
(multitouch) or your computer's keyboard, and it sends MIDI to a synth, a host or another app.

![KeyboardEDO's window](docs/screenshot.png)

| | |
|---|---|
| Kind | App: a MIDI controller (CoreMIDI on macOS, the ALSA sequencer on Linux) |
| Platforms | Linux x86-64, macOS 26 or later on Apple Silicon. No Windows |
| Version | 0.9.0 (preview) |
| License | MIT |

**Downloads:** ready-built apps will be on itch.io with the 0.9 preview. This repo has the
source only.

## What makes it different

- **Exact pitch, not pitch-bend tricks.** Each note carries its pitch as MIDI 2.0 Pitch 7.9, so a
  31-EDO step sounds as a 31-EDO step on any receiver that honours it, with per-note pitch bend,
  per-note controllers and per-note pressure, and Pitch 7.25 glides. MIDI 1.0 receivers get MIDI 1.0
  automatically.
- **Keyboards made for EDOs.** A standard piano at 12-EDO, a diamond piano at other EDOs, tiled
  layouts of diamond keys at any EDO (most of them isomorphic), and custom layouts built key by key.
- **Several layouts in one document.** A `.kedo` document holds layouts, each with its own tuning,
  switched between while you play. An arpeggiator and a scale highlight are built in. Documents from
  KeyEDO, its macOS and iPadOS predecessor, import as one layout.

The Unlikelyware synths play its notes at their exact pitch, and through
[StageHand](https://github.com/erickampman/StageHand) any CLAP instrument that takes MIDI 2.0. The
help page (Help in the app, or [resources/help/help.md](resources/help/help.md)) covers it all.
[docs/decisions.md](docs/decisions.md) says why it's built this way, and
[docs/roadmap.md](docs/roadmap.md) what comes next.

## Installing

**macOS:** drag `KeyboardEDO.app` into Applications. The 0.9 previews aren't notarized, so the first
time macOS refuses to open it: open System Settings ▸ Privacy & Security and click Open Anyway. Or
clear the quarantine flag yourself:

```bash
xattr -dr com.apple.quarantine /Applications/KeyboardEDO.app
```

**Linux:** unpack the download anywhere you'll keep it, such as `~/.local/lib/keyboardedo`, and keep
`keyboardedo` and its `help` folder together (Help looks for `help/` beside the executable). To run
it as plain `keyboardedo`, link it into a directory on your `PATH`:

```bash
ln -sf ~/.local/lib/keyboardedo/keyboardedo ~/.local/bin/keyboardedo
```

## Building from source

```bash
git clone https://github.com/erickampman/KeyboardEDO.git
cd KeyboardEDO
tools/fetch_deps.sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

[BUILDING.md](BUILDING.md) has what to install first on Linux and macOS, the tests, and how to run
it and its tools.

## Help and bugs

Questions and bug reports go to [GitHub Issues](https://github.com/erickampman/KeyboardEDO/issues).

## License

MIT. See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
