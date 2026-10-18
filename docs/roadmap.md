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

Built on Linux 2026-10-12 (Ubuntu, GCC 15, a Wayland session) with no code
changes: the core and app compile without warnings, the tests pass, and
`--snapshot` draws the three keyboards, with `--click` pressing the right
tile, under both `SDL_VIDEO_DRIVER=wayland` and `x11` (XWayland).

Still to check by hand:
- on Linux: two-finger playing on a touchscreen (Wayland and X11), a real
  mouse, resizing and libdecor's decorations, HiDPI scaling, and a native
  X11 session rather than XWayland;
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

### 3b. The ALSA sequencer's (uw-core, on the Linux PC): done 2026-10-12 (uw-core `63c85d9`)

`AlsaSeqOutput`: two UMP sequencer clients, one per protocol, as
`AlsaSeqInput`'s (an application's sequencer client has one protocol), so a
published port shows subscribers the protocol it sends. Destinations are
sent to directly, without subscribing; published ports send to their
subscribers. `AlsaSeqInput` now takes the kernel's largest input pool, as a
300-message burst overran ALSA's default. `test-alsa-output` loops back
through `AlsaSeqInput` in both protocols, follows a destination leaving and
coming back, and checks a legacy client gets ordinary events.

KeyboardEDO builds `uw::midi` on Linux too (JACK's headers now needed).
Checked with `kedo-midi-listen`, `aseqdump` and `keyboardedo --snapshot
--click`: the published source reaches a MIDI 2.0 listener with Pitch 7.9,
and a legacy subscriber as the bare note number; a MIDI 2.0 destination gets
MIDI 2.0, and a MIDI 1.0 UMP destination and `aseqdump` the nearest note
with the bend range as CC 101/100/6/38; quitting sends the note-off and CC
123.

Checked by the author 2026-10-13: KeyboardEDO plays StageHand on Linux.
Its MIDI 2.0 behaviour there was confusing, most likely in the plugins,
which need work (decisions.md, "What the synths do with it"); not yet
pinned down.

Still to check by hand: a USB MIDI 2.0 device's UMP endpoint ports.

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

## 7. Octave columns (KeyboardEDO): done 2026-10-13

decisions.md, "Octave columns" and "The lowest key, and the default origin".
Built first as an isomorphic Tiled generator (right 2 - EDO), which put B5
right of A6; the author's layout isn't isomorphic, so it became
`OctaveLayout`: two octaves of degree columns, every other one half a tile
up, the bottom zigzag chromatic. Every tile layout now starts at the origin
note in octave -1, and the default origin is A0.

Tests: the author's 12-EDO examples (A-1 at the bottom left, A0 above it,
A#6 up-right of A6, B6 right of it, A1 after G#1); at every EDO from 5 to 72
and four origin octaves, straight up an octave, right +2, up-right +1 or
EDO + 1, every playable step present, nothing overlapping; Tiled's lowest
key at its bottom left, nothing below it playable. The existing tests that
assumed the A1 default now set it.

The reset view keeps the bottom row in view for two frames, because ImGui
sizes the toolbar a frame late. Scrolling and zooming needed more than the
wheel: Shift-wheel scrolls sideways, there are zoom buttons and Cmd/Ctrl
= - 0, and the panel names them (decisions.md, "UI").

Still to check by hand: playing it, Shift-wheel and the zoom keys with a
real mouse and keyboard; and the Tiled presets against KeyEDO on the Mac
(the author recalls them differently; KeyEDO's bottom row isn't chromatic
either, but steps by the generator's "right").

## 8. Layouts in a document (KeyboardEDO)

Decided 2026-10-13 (decisions.md, "Documents with several layouts"): a
document is a set of layouts chosen in advance, played in Play mode and set
up in Edit mode, in one app.

### 8a. `.kedo` bundles, several layouts, Play and Edit modes: done 2026-10-13

A `.kedo` folder: `document.json` (format, version, layout order, the one
showing, the destination) and `layouts/<name>.json`, each with its own
tuning, layout and playing settings in KeyEDO's field names. Safe save
(write `.kedo.tmp`, swap). KeyEDO `.json` files open as a one-layout untitled
document. Play mode: a layout selector, switching releases held notes, the
settings hidden. Edit mode: add, remove, rename, duplicate and reorder
layouts, and their settings. Tests: a bundle round-trips; a newer version
is refused; a KeyEDO file imports as one layout; a crash mid-save (the swap
not done) leaves the old bundle; switching releases notes. File dialogs:
SDL3's folder dialog to open a bundle on Linux.

As built: `Document` is the bundle (its layouts, the one showing, the
destination); what was `Document`, one KeyEDO-shaped setup, is
`LayoutSetup`, now with a `name`. The toolbar has a layout menu and an
Edit/Play toggle (remembered as a preference, Edit at first); Play mode's
panel shows only Layouts, View and File. Open is a folder dialog (a bundle
is a folder on Linux), Import KeyEDO... a file dialog, Save As adds `.kedo`.
Tests (`DocumentTests`): a three-layout bundle round-trips, two layouts of
one name getting their own files; a layout file opens alone as a KeyEDO
document; a stale `.tmp` from an unfinished save doesn't stop the next; a
layout file copied into `layouts/` appears after the listed ones; a file or
plain folder isn't overwritten; a newer version and another format are
refused; a KeyEDO file imports. Checked with `--snapshot --tap`: Edit and
Play modes, switching to a layout with its own tuning and colouring, and
Save rewriting the bundle.

Not tested automatically: that switching releases held notes (it calls the
Performer's `releaseAll`, as changing the destination does). Still to check
by hand: the file dialogs on Linux (the folder dialog choosing a `.kedo`),
and on the Mac, where `.kedo` isn't a package until the app has a bundle.

### 8b. Custom layouts

decisions.md, "Custom layout". Two sessions:
1. The layout and its file: `CustomLayout` (keys with place, shape, size,
   look, EDO / octave / degree, note number, group and channel), its JSON
   in the layout file, Convert to custom, drawing and playing it. Tests: a
   converted layout plays exactly what the original did; the file round-trips;
   nearest-free note numbers. **Done 2026-10-13:** tests convert every
   generated layout (the piano, diamonds at 5, 17 and 31, every Tiled preset
   at 12, 19 and 31) and find each playable key in the same place with the
   same note number, pitch, group and channel, notes shared as before;
   `customKeys` round-trips, bad values clamped; pitch from EDO / octave /
   degree and the origin; nearest-free numbering (a same-pitch key's
   number, other channels not competing, all 128 taken). Checked with
   `--snapshot --tap`: converting Octave columns keeps its look and view.
2. The editor, in Edit mode: select (Shift adds), drag, snap, add, delete,
   duplicate, undo; the inspector, with pitch as EDO / octave / degree, the
   suggested note number and its override; auditioning edits through the
   output. **Done 2026-10-13** (decisions.md, "Custom layout", the editor as
   built). Also: custom keys are diamonds only, and the 12-EDO piano
   converts to the diamond piano's arrangement. `--snapshot` takes steps in
   order (`--tap [shift+]`, `--double`, `--drag`, `--key`), ignoring the
   real mouse meanwhile. Checked with them: selecting, Shift-selecting,
   dragging and nudging with snapping, adding by double-click, duplicating,
   deleting and undoing, the inspector's degree changing a key's pitch and
   number, and Cmd/Ctrl-S saving the edits, read back from the bundle;
   auditioning heard with `aseqdump` (note on, off 400 ms later).

   The author found no way to add a layout from a custom one: in Play mode
   the Layouts section only lists them (its hint now says to click Edit),
   and in Edit mode the Keys section's Add and Duplicate had the Layouts
   section's labels, which ImGui takes as one widget; they're now "Add key",
   and so on. Snapshot runs use default preferences and save none (the
   author's running KeyboardEDO and the tests had been switching each
   other's mode through the shared file).

   Not tested automatically: the editor is app code (the core's custom
   layout, numbering and file are tested). Still to check by hand: dragging
   with a real mouse and a touchscreen, the colour picker, and whether a
   converted diamond piano's labels ("A", "110.0") should follow a pitch
   change (only a label that's the note's name with its octave does now).

## 9. Computer keyboard (KeyboardEDO): done 2026-10-13

Re-planned 2026-10-13 (decisions.md, "Computer keyboard"): not KeyEDO's
QWERTY region moved with the arrow keys, which the author found too
confusing, but a computer key the player assigns to each note key; no
region or border on a custom layout. Shift latches. Hover expression
(Control held) on the desktop. Settled at the start of the session: keys
assigned in the inspector by pressing them; default mappings (tracker rows
on the piano, a zigzag of steps on the diamond piano and Octave columns,
the staggered rows as a lattice on Tiled); computer keys play in Edit mode
too; octave shifting later.

As built: a computer key is its HID usage (SDL's scancode), named in files
as the W3C `code` (`src/kedo/ComputerKeys.*`). Every `Key` and `CustomKey`
carries one; the generated layouts fill theirs in, Convert to custom copies
them (the 12-EDO piano's onto its diamonds), and a custom layout's file
stores them (`computerKey`), a duplicate kept on the first key only. The
app plays them through the Performer's `keyDown` / `keyUp` (not while a text
field has focus, nor with Cmd/Ctrl), draws each key's name small at the
bottom of its key in the system's keyboard layout (View: "Computer keys", a
preference, on), and the inspector assigns them ("Assign...", "Clear";
duplicating a key doesn't copy its computer key). Hover expression is the
Performer's `hoverBegin` / `hoverMove` / `hoverEnd`; the Playing section now
shows Horizontal and Vertical in Gliss mode too, as hover uses them.

Also, left from 8b: a converted key's labels follow a pitch change. Each of
a custom key's two labels has a style (text, name and octave, name, degree,
frequency), worked out from the pitch when drawn (decisions.md, "Custom
layout"); the inspector chooses it, and 8b's files get the style whose text
a label has.

Tests: the four rows and their codes; every generated layout's mapping (at
5, 12, 17, 31 and 72-EDO and three origin octaves) only on playable keys,
each computer key once, Z on the origin note in octave 3; the piano's,
diamond piano's, Tiled's and Octave columns' shapes; converting keeps every
computer key; the file round-trips and drops duplicates; hover expression
(per note and channel, reset by a key's release and by Ctrl's, a pointer
keeping the channel); converted labels equal the original's and follow a
pitch change; label styles round-trip, and 8b's labels are recognised.
Checked with `--snapshot`, which gained `--press`, `--release` and `--move`
steps (and now moves the mouse a frame before pressing, so a tap on the
keyboard just after one on the panel isn't the panel's): playing Z and S on
the piano, the diamond piano and Tiled mappings drawn, assigning X to a
converted piano's C3 (moving it from D3), a converted "A / 110.0" key
reading "B / 246.9" after two steps up, hover bend and CC 74 in the monitor,
and Shift-latching a computer key.

Still to check by hand: a real keyboard on X11 and Wayland, and a non-QWERTY
layout's names; hover expression with a real mouse; on the Mac, Control
(not Cmd) for hover.

## 10. Arpeggiator (KeyboardEDO): done 2026-10-15

decisions.md, "Arpeggiator". KeyEDO's `ArpeggiatorEngine` as `Arpeggiator`, a
NoteSink between the Performer and the outputs: its six patterns (by pitch),
ten divisions, gate, octave span, velocity source (and a fixed velocity),
tempo and hold, KeyEDO's Hold and octave span as the author chose. Per
layout, in KeyEDO's fields; an Arp toolbar button, an Arpeggiator panel
section in both modes, and the synopsis's "Arp Up 1/16 120 bpm". It has no
timer: the app runs it on a thread of its own, woken at its next event, so
vsync doesn't jitter the steps.

Tests (`ArpeggiatorTests`, with a hand-stepped clock): passing through when
off; step and gate times at 120 bpm, triplets and whole notes; a 100% gate
ending each note as the next starts; every pattern, carrying on after the
chord's first note; KeyEDO's octave span, with and without the layout's
octave notes; releasing, the last key silencing at once; Hold adding and
clearing; the three velocity sources; a tempo change keeping phase, and a
stall playing one step; on/off emptying it; per-note expression re-sent on
each step and channel expression passing through; under a Performer, with
latch. `DocumentTests`: KeyEDO's arp fields read, round-tripped and clamped.
`ViewTests`: the synopsis. Checked with `--snapshot`: Z, C and B held
arpeggiate C3 E3 G3 in the monitor; Play mode's panel; Hold keeping released
notes cycling, drawn latched; at 31-EDO, two octaves give n93 then n124.

Still to check by hand: hearing it (steadiness of the steps on a synth, on
the Mac and Linux), and changing tempo and pattern while it plays.

## 11. Identity and discovery (uw-core, KeyboardEDO)

KeyEDO's controller roadmap tier 2: endpoint info and function blocks as
UMP stream messages, a Product Instance ID, Stream Configuration to agree
the protocol, and timestamps (CoreMIDI host time, ALSA real-time events).

Decided 2026-10-15 (decisions.md, "Identity and discovery"): the Product
Instance ID is per installation; one function block, spanning the groups
the document's layouts use; CoreMIDI's endpoint API in a uw-core `.mm`.
Likely more than a day, so in two parts:
- 11a: the endpoint, function block and Product Instance ID (uw-core: the
  ALSA sequencer's `snd_seq_create_ump_endpoint` / `_block`, CoreMIDI's
  `MIDIUMPMutableEndpoint`; KeyboardEDO: what it declares). It fixes the
  known gap of a late subscriber missing the bend-range setup.
- 11b: Stream Configuration and timestamps, which would also let the
  arpeggiator's steps be scheduled ahead.

11a done on the Mac 2026-10-15 (uw-core `5cf9aad`; decisions.md, "Identity
and discovery"): the published source is a CoreMIDI UMP endpoint with the
installation's Product Instance ID and a "Keyboard" block over the
document's groups, keeping "KBED"; it answers discovery itself, and sends
the bend-range setup again to a peer that asks. Checked with
kedo-midi-listen, a MIDI 1.0 receiver and a discovery sent by hand. Still
to do:
- On the Linux PC: `AlsaSeqOutput::openEndpoint` and `setFunctionBlocks`
  (ALSA's UMP endpoint and block), discovery answered, and the setup
  resent when a receiver subscribes.
- With real hosts (Logic, Umpire, StageHand): that they show the endpoint
  and block, find it again by Product Instance ID, and take a block change
  (on CoreMIDI, a remade endpoint).

## 12. Performance bar (KeyboardEDO)

decisions.md, "Performance bar". A bar docked on the left of the window: a
pitch-bend fader and mod wheel, four assignable knobs (CC 74, channel
pressure, CC 71, CC 7 by default; assignments per document, positions not
restored), and the arpeggiator's controls moved there from Settings. A
toolbar button shows and hides it. Check each layout with `--snapshot`:
the canvas keeps working at the narrower width (hit-testing, Gliss drags
stopping at the bar's edge).

Done on the Mac 2026-10-17 (decisions.md, "Performance bar", for what was
settled while building it): `kedo::PerformanceControls` (ControlsTests)
keeps the controls' places and sends what moves; knob assignments are
saved in `document.json`. Checked with `--snapshot`: the bar, the
assignment popup, the bar hidden, the bend fader springing back to centre,
and a Gliss drag stopping at the keyboard's left edge, on the piano. Still
to do:
- Try it by hand with a real synth (IFSynth or PlainOldSynth: Timbre and
  Pressure), and with two fingers on a touchscreen (bend and keys at once).
- On the Linux PC: build, and check the bar under Wayland and X11.

## 13. Identity and help (KeyboardEDO)

decisions.md, "Identity and help". An app icon drawn from the diamond tiles,
a macOS `.app` bundle and a Linux `.desktop` entry, an About box, a bundled
`help.html` opened in the browser, and `.kedo` files opening from the
Finder. Signing and notarization wait for the release work, shared with
StageHand and the synths.

Mac bundle and icon done 2026-10-18: the build makes `KeyboardEDO.app`
(Info.plist from `cmake/Info.plist.in`, ad-hoc signed), its icon compiled from
`resources/KeyboardEDO.icon` by `actool` into `Assets.car` (macOS 26) and an
`.icns` (older macOS); `build/keyboardedo` links to the executable. Checked:
the Finder shows the icon, `--snapshot` runs through the link, tests pass.
Still to do: the Linux icon and `.desktop` entry, the window icon, the About
box, help, and the `.kedo` document type.

Toolbar branding done 2026-10-18: the Unlikelyware logo and a "Keyboard" +
"EDO" wordmark in Karla (two blues, the app's, in the place of the plugins'
tan and terracotta "IFS" + "ynth") at the toolbar's left, sized to its icon row, and a
faint band setting the synopsis row apart. Clicking the logo opens
www.unlikelyware.com in the default browser, as the plugins' logo does. Logo and
font are embedded (`resources/README.md`); the font's OFL text ships in
`resources/fonts/OFL.txt`. Checked with `--snapshot`.

Help done 2026-10-18: `resources/help/help.md`, in Markdown Web Writer's
dialect, which the author exports to `help.html` (CSS embedded). The build
copies it and the logo (as `Unlikelyware.png`) to `help/` in the bundle's
Resources and beside the executable; a toolbar button, F1 and Cmd-? open it
in the default browser. `SDL_GetBasePath()` finds it: the bundle's Resources
when run as an app, `build/` through the `build/keyboardedo` link. After
editing `help.md`, export `help.html` again: the build doesn't.

Linux icon done 2026-10-18 (on the Linux PC): flat rounded PNGs, 16 to 512 px,
in `resources/linux/icons` (`tools/make-linux-icons.sh` makes them from the
Mac icon's art), a `.desktop` entry, and the window icon (X11; Wayland takes
the entry's). `cmake --install build` puts the executable in `bin`, the help
page in `share/keyboardedo/help` (where `openHelp` looks from `bin`), and the
entry and icons in `share/applications` and `share/icons/hicolor`. Checked:
the X11 window's `_NET_WM_ICON` is 256×256; the toolbar's logo, wordmark and
Help button draw under Wayland; F1 opens the help page through `xdg-open`;
an install to a scratch prefix lays the files out as above; the build is
free of GCC's warnings again (`KnobAssignment::label{}`). The author checked
by hand: with `cmake --install build --prefix ~/.local`, the help page opens
in the browser and GNOME's dock shows the icon.

## Later

- Network MIDI 2.0 through netmidi2 (C, `~/Dev-MIDI/netmidi2`).
- Tuning as a Property Exchange resource for Umpire (`X-` prefix), and
  arpeggiator tempo from Flex Data (controller roadmap 11, 12).
- Not planned: an MPE fallback or MIDI Tuning Standard SysEx for MIDI 1.0
  synths (decisions.md, "No MPE").
- Glide that slides continuously between keys, not only from key to key.
- Pressure from Force Touch trackpads and pen tablets.
- iPad and Android, if the toolkit chosen in milestone 2 allows.
