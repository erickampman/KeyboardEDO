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
- [x] UMP endpoint and function blocks on Linux (ALSA sequencer)
- [x] Product Instance ID per installation

## M6: Performance bar
- [x] Left-docked bar with a show/hide toolbar button; the canvas takes the rest
- [x] Pitch-bend fader (spring return) and mod wheel (CC 1)
- [x] Four assignable knobs (CC 74, pressure, CC 71, CC 7), saved per document
- [x] Arpeggiator controls moved from Settings into the bar

## M7: Identity and help
- [x] macOS app bundle with its icon (Icon Composer source, compiled by actool)
- [x] Linux icon PNGs and .desktop entry; window icon; `cmake --install`
- [x] Unlikelyware logo and two-colour wordmark (Karla) at the toolbar's left, as the plugins have
- [x] About box: version, Unlikelyware, MIT license, Ko-fi link, Product Instance ID
- [x] Bundled help.html, opened in the default browser (toolbar button, F1, Cmd-?); shortcuts in it
- [x] .kedo documents open from the Finder (document type and SDL's open-file event)

## M8: Look and feel
- [x] The plugins' glowing knobs, group boxes and value boxes, in KeyboardEDO's colours
- [x] A linear slider in that look (bend, mod, Settings' sliders)
- [x] Arpeggiator: knobs for tempo, gate, octaves; captioned dropdowns
- [x] DejaVu Sans in the bar and Settings
- [x] Checked by hand on the Mac, and typing a value (2026-11-07; Esc doesn't close the typing field, clicking away does)

## M9: Before 1.0 (after the 0.9 previews; left from M5)
- [ ] Several fingers at once on a Mac touchscreen (ReleaseUMP R2-29, a known gap in 0.9): SDL 3.4.18's Cocoa backend skips `NSTouchTypeDirect`, and screen touches reach only an `NSGestureRecognizer` (`NSEventTypeDirectTouch`), so patch SDL to add a recognizer to its view. macOS also cancels three fingers landing together after about 260 ms (probed 2026-11-10)
- [ ] Stream Configuration (protocol negotiation)
- [ ] Timestamps (CoreMIDI host time, ALSA real-time events)

## M10: Later
- [ ] Network MIDI 2.0 (netmidi2)
- [ ] Tuning as Property Exchange resource; arpeggiator tempo from Flex Data
- [ ] Continuous glide between keys
- [ ] Pressure from Force Touch and pen tablets
- [ ] iPad and Android
