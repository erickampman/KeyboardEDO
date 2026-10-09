# KeyboardEDO

A cross-platform MIDI keyboard controller for equal divisions of the octave,
after KeyEDO (`~/Dev-MIDI/KeyEDO`). UMP throughout, MIDI 2.0 first: Pitch 7.9
notes, per-note pitch bend and controllers, Pitch 7.25 glides, with a MIDI 1.0
fallback. Three keyboards, as KeyEDO's: a standard piano at 12-EDO, the
diamond piano at other EDOs, and an isomorphic tiled layout at any EDO.

So far it plays on the Mac: to a chosen destination or from its own
published source, in MIDI 2.0 or 1.0 as the receiver needs, and opens and
saves KeyEDO-compatible documents. Linux builds without MIDI output until
uw-core has its ALSA side.
[docs/decisions.md](docs/decisions.md) says why it's built this way, and
[docs/roadmap.md](docs/roadmap.md) what comes next.

## Building and testing

```bash
git submodule update --init --recursive
cmake -S . -B build -G Ninja
ninja -C build
ctest --test-dir build --output-on-failure
```

Once per clone, so `git push --recurse-submodules=check` (the house rule)
accepts the json submodule: it's a shallow clone at upstream's `v3.12.0` tag,
which no fetched branch contains, so the check can't otherwise see it's on
GitHub.

```bash
git config push.recurseSubmodules check
git -C external/json fetch --depth 1 origin +refs/tags/v3.12.0:refs/remotes/origin/v3.12.0
```

If the json pin ever moves to another tag, use that tag in the second line.

Run it:

```bash
./build/keyboardedo
./build/keyboardedo --edo 17 --layout tiled --publish
./build/keyboardedo ~/"Library/Mobile Documents/com~apple~CloudDocs/KeyEDO/Untitled.json"
```

`keyboardedo --help` lists the options. To see what it sends without a DAW,
run this alongside it with Publish on:

```bash
./build/kedo-midi-listen KeyboardEDO 60
```

Linux needs SDL3's build dependencies (X11 and/or Wayland development
packages; see `external/SDL/docs/README-linux.md`). To build only the core and
its tests, configure with `-DKEDO_APP=OFF`.

To see the layouts without a UI:

```bash
./build/kedo-layout-svg build/svg
```

## Layout

| Path | What |
|---|---|
| `src/kedo/Note.h` | what a key plays: id, note number, group and channel, pitch |
| `src/kedo/Tuning.*` | EDO, origin, frequencies, note numbers, Pitch 7.9 / 7.25 |
| `src/kedo/Generators.*` | Tiled generator presets |
| `src/kedo/Layout.*` | `PianoLayout`, `DiamondLayout`, `TiledLayout`: geometry and hit-testing |
| `src/kedo/Performer.*` | pointers and keys in, notes and expression out |
| `src/kedo/NoteSink.h` | what is played, before it's MIDI |
| `src/kedo/UmpOutput.*` | NoteSink calls to UMP, MIDI 2.0 or 1.0 |
| `src/kedo/Viewport.*`, `KeyStyle.*`, `UmpText.*` | the view transform, key colours and labels, UMP as text |
| `src/kedo/Document.*` | KeyEDO-compatible JSON documents |
| `src/kedo/Synopsis.*`, `Preferences.*` | the one-line synopsis; app preferences |
| `src/app/` | the app: SDL and ImGui (`KeyboardApp`, `Icons`), MIDI targets (`MidiTargets`) |
| `external/uw-core` | shared UMP code (submodule) |
| `external/SDL`, `external/imgui` | SDL 3.4 and Dear ImGui 1.92 (submodules) |
| `tools/LayoutSvg.cpp` | draws layouts to SVG |
| `tools/MidiListen.cpp` | `kedo-midi-listen`: prints what a source sends (macOS) |
| `external/json` | nlohmann/json 3.12.0 (shallow submodule) |

MIT licence, as uw-core.
