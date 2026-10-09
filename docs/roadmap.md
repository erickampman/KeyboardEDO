# Roadmap

Milestones toward a first useful version: KeyEDO's three keyboards on Linux
and macOS, sending MIDI 2.0 to a chosen destination and from a UMP endpoint
of its own. Each milestone is about a day, with a fresh session that starts
by reading [decisions.md](decisions.md) and this file. Mark a milestone done
here when it finishes, and put what was learned in the docs. Later milestones
are rougher.

## 1. The core (KeyboardEDO): done 2026-10-09

`kedo::core`: tuning, generator presets, the three layouts with hit-testing,
the Performer (multitouch, Gliss and Fixed, glide, latch, strike velocity,
per-note or channel expression), and `UmpOutput` (MIDI 2.0, with the MIDI
1.0 fallback through uw-core's `downconvert`). Four silent test suites.
`kedo-layout-svg <dir>` draws each layout to SVG for checking by eye.

## 2. UI: SDL3 and Dear ImGui (KeyboardEDO): done on the Mac 2026-10-09

decisions.md, "UI". `keyboardedo` is one window: a settings bar (tuning,
layout, generator, tile colour, drag and expression, velocity, latch,
protocol, group and channel), the keyboard, and a MIDI monitor showing the
UMP that would be sent (no transport yet). It draws all three keyboards with
KeyEDO's colours and labels, scrolls and zooms, and plays by mouse and
touchscreen.

`keyboardedo --snapshot FILE.png [--click X,Y]` draws a few frames, saves the
window, and quits: a check without a person at the screen. Checked that way
on the Mac (Retina): the three keyboards, and a click on a tile and on a
piano key reaching the monitor as the right note.

Still to check by hand:
- the Linux build (X11 and Wayland) and two-finger playing on a touchscreen;
- trackpad pinch and scroll direction on the Mac;
- Shift-latch and Fixed-drag expression with a real mouse.

## 3. MIDI output in uw-core (uw-core)

A `MidiOutput` seam beside `MidiInput`: list destinations, send UMP to one,
and publish a named UMP endpoint with function blocks. ALSA sequencer UMP on
Linux, CoreMIDI on macOS. Report each destination's protocol, so
`OutputConfig::protocol` can follow it. Silent tests, as `BackendTests`.

## 4. The app plays (KeyboardEDO)

Wire the Performer to `MidiOutput`: a destination menu, KeyEDO's "Publish"
(our own endpoint, "KeyboardEDO", one output function block "Keyboard"),
group and channel. All notes off when the destination or tuning changes.
Load and save the configuration as JSON with KeyEDO's field names, so its
documents open here.

## 5. Configuration and toolbar (KeyboardEDO)

KeyEDO's Configure sheet (tuning, layout, generator, tile colour, MIDI, drag
and expression, velocity), the one-line synopsis across the top, the
protocol badge, and the Lock and Latch buttons.

## 6. Computer keyboard (KeyboardEDO)

KeyEDO's QWERTY mappings: the classic DAW rows on the piano, and on Tiled an
outlined region moved with the arrow keys. Shift latches. Hover expression
(Control held) on the desktop.

## 7. Arpeggiator (KeyboardEDO)

KeyEDO's `ArpeggiatorEngine` as a NoteSink between the Performer and the
output: patterns, divisions, gate, octave span, velocity source, hold.

## 8. Identity and discovery (uw-core, KeyboardEDO)

KeyEDO's controller roadmap tier 2: endpoint info and function blocks as
UMP stream messages, a Product Instance ID per installation or document,
Stream Configuration to agree the protocol, and timestamps (CoreMIDI host
time, ALSA real-time events).

## Later

- Network MIDI 2.0 through netmidi2 (C, `~/Dev-MIDI/netmidi2`).
- Tuning as a Property Exchange resource for Umpire (`X-` prefix), and
  arpeggiator tempo from Flex Data (controller roadmap 11, 12).
- MPE fallback for MIDI 1.0 synths: rotate notes across channels with
  per-channel bend, so microtonal playing works on them (controller
  roadmap 13).
- Glide that slides continuously between keys, not only from key to key.
- Pressure from Force Touch trackpads and pen tablets.
- iPad and Android, if the toolkit chosen in milestone 2 allows.
