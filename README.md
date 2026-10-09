# KeyboardEDO

A cross-platform MIDI keyboard controller for equal divisions of the octave,
after KeyEDO (`~/Dev-MIDI/KeyEDO`). UMP throughout, MIDI 2.0 first: Pitch 7.9
notes, per-note pitch bend and controllers, Pitch 7.25 glides, with a MIDI 1.0
fallback. Three keyboards, as KeyEDO's: a standard piano at 12-EDO, the
diamond piano at other EDOs, and an isomorphic tiled layout at any EDO.

So far: the core library with its tests, and the app's window (SDL3 and
Dear ImGui) showing what it would send. MIDI output comes next.
[docs/decisions.md](docs/decisions.md) says why it's built this way, and
[docs/roadmap.md](docs/roadmap.md) what comes next.

## Building and testing

```bash
git submodule update --init
cmake -S . -B build -G Ninja
ninja -C build
ctest --test-dir build --output-on-failure
```

Run it:

```bash
./build/keyboardedo
./build/keyboardedo --edo 17 --layout tiled
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
| `src/app/` | the app: the only code that knows SDL and ImGui |
| `external/uw-core` | shared UMP code (submodule) |
| `external/SDL`, `external/imgui` | SDL 3.4 and Dear ImGui 1.92 (submodules) |
| `tools/LayoutSvg.cpp` | draws layouts to SVG |

MIT licence, as uw-core.
