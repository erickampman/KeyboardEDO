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

### 3a. The seam and CoreMIDI: done 2026-10-09 (uw-core `ee1dd11`)

`uw::MidiOutput` beside `MidiInput`: list destinations with their protocol,
`open()` one, `openVirtual()` a source other apps receive from (KeyEDO's
"Publish"), `send()` UMP, and hotplug with `changes()` / `sync()`. It never
translates: the caller sends what `protocol(handle)` asks for, which is
`UmpOutput`'s job here. `CoreMidiOutput` implements it, and
`test-coremidi-output` checks it silently on the Mac by looping back through
`CoreMidiInput` (a virtual source, and MIDI 2.0 and 1.0 destinations).
`PlatformMidiOutput.h` sets `UW_HAVE_MIDI_OUTPUT` to 0 where there's none.

### 3b. The ALSA sequencer's (uw-core, on the Linux PC)

`AlsaSeqOutput`: a UMP-enabled sequencer client with an output port per
protocol, subscribing to a destination port, or published as a port others
subscribe to. Extend `test-backends`. Needs a Linux machine; nothing here
could compile it.

Declaring a UMP endpoint and function blocks moved to milestone 11, with the
rest of identity and discovery: on the Mac it needs CoreMIDI's Objective-C
`MIDIUMPMutableEndpoint`.

## 4. Keys carry the note they play (KeyboardEDO): done 2026-10-09

decisions.md, "Keys carry the note they play". A `Note` (id, note number,
group and channel, pitch) on every `Key`; the Performer, `NoteSink` and
`UmpOutput` work from it, and `UmpOutput` no longer takes a `Tuning`. The
three layouts behave as before: the existing tests pass with only the types
changed. New tests: each layout's keys carry their tuning's note; any note
number and pitch is sent as given (Pitch 7.9 only when they differ), on its
own group and channel; MIDI 1.0 plays the nearest note to the pitch; the
Performer passes Notes through untouched; glides go to the exact pitch.

## 5. The app plays (KeyboardEDO): done on the Mac 2026-10-09

decisions.md, "Sending MIDI" and "Documents". "Send to" a destination and
"Publish" a KeyboardEDO source through uw-core's `CoreMidiOutput`, each in
its own protocol; group and channel; notes off when the targets or tuning
change, and on quit. KeyEDO-compatible JSON documents, opened from the
command line or with native dialogs. The UMP endpoint and function block
are milestone 11.

Checked on the Mac with `kedo-midi-listen` (a new tool that prints what a
source sends, or acts as a destination of either protocol) and
`keyboardedo --snapshot --click --wait`: a click reaches the published
source as MIDI 2.0 with Pitch 7.9, a MIDI 2.0 destination the same, and a
MIDI 1.0 destination as the nearest 12-EDO note with the bend range as CC
101/100/6/38; quitting sends the note-off and CC 123. All 25 of the author's
KeyEDO documents open and re-save. New `DocumentTests`.

Still to check by hand: a real synth or DAW on each target; a device
unplugged and plugged back in; the file dialogs.

## 6. Toolbar, settings panel and synopsis (KeyboardEDO): done 2026-10-09

decisions.md, "The window". The three rows of settings became KeyEDO's
arrangement, with the author's choices: a toolbar of icon buttons (drawn,
not a font), a **side panel** for the settings that can stay open while
playing, and a **toggleable** MIDI monitor; both remembered as preferences.
KeyEDO's synopsis line under the toolbar (`Synopsis.*`, tested), the
protocol badge centred, the routing menu on the antenna button. The panel
shows only what the layout uses. Two-finger scrolling now follows the
fingers. `keyboardedo --tap X,Y` presses and releases, for buttons.

## 7. Octave columns (KeyboardEDO)

decisions.md, "Octave columns": a Tiled generator with up-right 1 and right
2 - EDO. `TiledLayout` accepts a negative `right`; rows cover every octave,
and the view scrolls vertically. Tests: straight up is an octave, up-right a
step, from every tile.

## 8. Custom layout (KeyboardEDO)

decisions.md, "Custom layout". A `CustomLayout` of placed keys; edit mode
(drag, add, delete, duplicate, snap); the inspector, with pitch as EDO /
octave / degree and a suggested, overridable note number; convert any layout
to custom; save and load as JSON; auditioning edits through the output.
Maybe two sessions: the layout and its file, then the editor.

## 9. Computer keyboard (KeyboardEDO)

KeyEDO's QWERTY mappings: the classic DAW rows on the piano, and on Tiled an
outlined region moved with the arrow keys. Shift latches. Hover expression
(Control held) on the desktop.

## 10. Arpeggiator (KeyboardEDO)

KeyEDO's `ArpeggiatorEngine` as a NoteSink between the Performer and the
output: patterns, divisions, gate, octave span, velocity source, hold.

## 11. Identity and discovery (uw-core, KeyboardEDO)

KeyEDO's controller roadmap tier 2: endpoint info and function blocks as
UMP stream messages, a Product Instance ID per installation or document,
Stream Configuration to agree the protocol, and timestamps (CoreMIDI host
time, ALSA real-time events).

## Later

- Network MIDI 2.0 through netmidi2 (C, `~/Dev-MIDI/netmidi2`).
- Tuning as a Property Exchange resource for Umpire (`X-` prefix), and
  arpeggiator tempo from Flex Data (controller roadmap 11, 12).
- Not planned: an MPE fallback or MIDI Tuning Standard SysEx for MIDI 1.0
  synths (decisions.md, "No MPE").
- Glide that slides continuously between keys, not only from key to key.
- Pressure from Force Touch trackpads and pen tablets.
- iPad and Android, if the toolkit chosen in milestone 2 allows.
