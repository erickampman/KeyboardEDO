# KeyboardEDO

A cross-platform MIDI keyboard controller for equal divisions of the octave,
after KeyEDO, Unlikelyware's earlier macOS and iPadOS app. UMP throughout, MIDI 2.0 first: Pitch 7.9
notes, per-note pitch bend and controllers, Pitch 7.25 glides, with a MIDI 1.0
fallback. Three keyboards, as KeyEDO's: a standard piano at 12-EDO, the
diamond piano at other EDOs, and an isomorphic tiled layout at any EDO.

It plays on the Mac (CoreMIDI) and Linux (the ALSA sequencer): to a chosen
destination or from its own published source, in MIDI 2.0 or 1.0 as the
receiver needs. A document (`.kedo`, a folder) holds several layouts, each
with its own tuning, switched between in Play mode and set up in Edit mode;
KeyEDO's documents import as a document of one layout.
[docs/decisions.md](docs/decisions.md) says why it's built this way, and
[docs/roadmap.md](docs/roadmap.md) what comes next.

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

## License

MIT, as uw-core. See [LICENSE](LICENSE).
