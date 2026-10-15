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
- [ ] UMP endpoint and function blocks (CoreMIDI Objective-C API on Mac)
- [ ] Product Instance ID per installation / document
- [ ] Stream Configuration (protocol negotiation)
- [ ] Timestamps (CoreMIDI host time, ALSA real-time events)

## M6: Later
- [ ] Network MIDI 2.0 (netmidi2)
- [ ] Tuning as Property Exchange resource; arpeggiator tempo from Flex Data
- [ ] Continuous glide between keys
- [ ] Pressure from Force Touch and pen tablets
- [ ] iPad and Android
