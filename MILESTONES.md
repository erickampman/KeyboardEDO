# KeyboardEDO milestones

## Tests
- core: ctest --test-dir build-core --output-on-failure
- all: ninja -C build && ctest --test-dir build --output-on-failure

## M1: Core library
- [x] Tuning, generator presets, three layouts with hit-testing
- [x] Performer (multitouch, gliss/fixed, glide, latch, velocity, expression)
- [x] UmpOutput (MIDI 2.0 and 1.0 fallback via uw-core downconvert)
- [x] Keys carry the note they play (Note on every Key, not inferred at send time)
- [x] Four test suites passing

## M2: App and MIDI output
- [x] SDL3 + Dear ImGui window: scrolls, zooms, plays by mouse and touchscreen
- [x] CoreMIDI output (Mac): send to destination, publish a virtual source
- [x] ALSA sequencer output (Linux): both MIDI 2.0 and 1.0 clients
- [x] App plays: KeyEDO-compatible JSON documents, group and channel routing
- [x] Toolbar, settings side panel, synopsis, MIDI monitor
- [x] Builds on Linux with no code changes (Ubuntu, GCC 15, Wayland and X11)

## M3: Richer layouts and documents
- [x] Octave columns layout (two octaves of degree columns, chromatic bottom row)
- [x] `.kedo` document bundles: several layouts, Play and Edit modes, safe save
- [x] Custom layout editor: convert any layout, drag/snap/add/delete/undo, inspector
- [x] Computer keyboard: per-key HID assignment, default mappings, hover expression

## M4: Arpeggiator
- [x] ArpeggiatorEngine as a NoteSink between Performer and output
- [x] Patterns, divisions, gate, octave span, velocity source, hold

## M5: MIDI identity and discovery
- [x] UMP endpoint and function blocks (CoreMIDI Objective-C API on Mac), discovery answered
- [ ] UMP endpoint and function blocks on Linux (ALSA sequencer)
- [x] Product Instance ID per installation
- [ ] Stream Configuration (protocol negotiation)
- [ ] Timestamps (CoreMIDI host time, ALSA real-time events)

## M6: Performance bar
- [x] Left-docked bar with a show/hide toolbar button; the canvas takes the rest
- [x] Pitch-bend fader (spring return) and mod wheel (CC 1)
- [x] Four assignable knobs (CC 74, pressure, CC 71, CC 7), saved per document
- [x] Arpeggiator controls moved from Settings into the bar

## M7: Identity and help
- [ ] App icon (SVG source; .icns in a macOS .app bundle; PNG and .desktop on Linux; window icon)
- [ ] About box: version, Unlikelyware, MIT license, Ko-fi link, Product Instance ID
- [ ] Bundled help.html, opened in the default browser; a keyboard-shortcuts page
- [ ] .kedo documents open from the Finder (document type and SDL's open-file event)

## M8: Later
- [ ] Network MIDI 2.0 (netmidi2)
- [ ] Tuning as Property Exchange resource; arpeggiator tempo from Flex Data
- [ ] Continuous glide between keys
- [ ] Pressure from Force Touch and pen tablets
- [ ] iPad and Android
