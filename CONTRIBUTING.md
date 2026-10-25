# Contributing to KeyboardEDO

Bug reports and pull requests are welcome. Build steps are in the README; the reasons behind the
design are in [docs/decisions.md](docs/decisions.md) and what comes next in
[docs/roadmap.md](docs/roadmap.md).

## Before each commit

```sh
ctest --test-dir build --output-on-failure
```

The tests are silent (no audible output); keep new ones that way. Record a design decision in
docs/decisions.md, not only in a commit message.

## Code layout rule

`src/kedo/` (`kedo::core`) knows nothing of SDL3 or Dear ImGui: tunings, layouts, the performer, the
arpeggiator and the UMP output are plain C++ and tested without a window. Only `src/app/` uses the
toolkit.

## Checking UI changes without looking

`keyboardedo --snapshot FILE.png [STEP...]` renders a frame to a PNG after running the given steps in
order, so a UI change can be checked from a script:
`--click`, `--tap [shift+]X,Y`, `--double`, `--right` (a right-click), `--drag X1,Y1,X2,Y2`,
`--key [ctrl+][shift+]NAME`, `--press NAME` and `--release NAME` (a key held and let go, e.g. `z`,
`"left ctrl"`), `--move X,Y`. `keyboardedo --help` lists them all.

While the steps run, the real mouse is ignored. A snapshot run starts from the default preferences
(Edit mode, not publishing: add `--publish`) and saves nothing, so it never changes your own
settings. On Linux, `SDL_VIDEO_DRIVER=x11` or `wayland` chooses the video driver (SDL picks Wayland by
default).

If KeyboardEDO is already running and publishing under the same name, find a test instance's ALSA
client by its process id in `aconnect -l`, not by name.

## MIDI

KeyboardEDO sends MIDI 2.0 first, with a minimal MIDI 1.0 fallback, and never sends MPE
(docs/decisions.md, "No MPE").

## uw-core

`external/uw-core` is a pinned submodule. Don't commit inside it: make the change in a clone of
[uw-core](https://github.com/erickampman/uw-core), get it merged there, then move the pin here. With
`git config push.recurseSubmodules check` set once per clone, git refuses to push a pin whose commit
isn't on GitHub yet.

The README explains how to set up the json submodule so that check accepts it.

## The json submodule and the push check

Once per clone, so `git push --recurse-submodules=check` (the house rule) accepts the json
submodule: it's a shallow clone at upstream's `v3.12.0` tag, which no fetched branch contains, so
the check can't otherwise see it's on GitHub.

```bash
git config push.recurseSubmodules check
git -C external/json fetch --depth 1 origin +refs/tags/v3.12.0:refs/remotes/origin/v3.12.0
```

If the json pin ever moves to another tag, use that tag in the second line.

## Layout

| Path | What |
|---|---|
| `src/kedo/Note.h` | what a key plays: id, note number, group and channel, pitch |
| `src/kedo/Tuning.*` | EDO, origin, frequencies, note numbers, Pitch 7.9 / 7.25 |
| `src/kedo/Generators.*` | Tiled generator presets |
| `src/kedo/CustomLayout.*` | custom layouts: keys placed one by one, converting any layout |
| `src/kedo/ComputerKeys.*` | computer keys: their codes, and the generated layouts' default mappings |
| `src/kedo/Layout.*` | `PianoLayout`, `DiamondLayout`, `TiledLayout`: geometry and hit-testing |
| `src/kedo/Performer.*` | pointers and keys in, notes and expression out |
| `src/kedo/NoteSink.h` | what is played, before it's MIDI |
| `src/kedo/UmpOutput.*` | NoteSink calls to UMP, MIDI 2.0 or 1.0 |
| `src/kedo/Viewport.*`, `KeyStyle.*`, `UmpText.*` | the view transform, key colours and labels, UMP as text |
| `src/kedo/Document.*` | `.kedo` documents of several layouts; each layout a KeyEDO-compatible JSON file |
| `src/kedo/Synopsis.*`, `Preferences.*` | the one-line synopsis; app preferences |
| `src/app/` | the app: SDL and ImGui (`KeyboardApp`, `Icons`), MIDI targets (`MidiTargets`) |
| `external/uw-core` | shared UMP code (submodule) |
| `external/SDL`, `external/imgui` | SDL 3.4 and Dear ImGui 1.92 (submodules) |
| `tools/LayoutSvg.cpp` | draws layouts to SVG |
| `tools/MidiListen.cpp` | `kedo-midi-listen`: prints what a source sends |
| `external/json` | nlohmann/json 3.12.0 (shallow submodule) |
