# Decisions

Decisions about KeyboardEDO, made 2026-10-09 unless dated otherwise. The
roadmap is [roadmap.md](roadmap.md).

## What it is

A cross-platform MIDI keyboard controller for equal divisions of the octave,
after KeyEDO (`~/Dev-MIDI/KeyEDO`, Swift/SwiftUI, macOS and iPadOS). UMP is
its only internal format, and MIDI 2.0 its primary protocol: it uses MIDI
2.0-only features where they help, and makes MIDI 1.0 from them for MIDI 1.0
destinations. KeyEDO's `docs/controller-roadmap.md` lists those features;
this repo implements them.

KeyEDO is the guide for behaviour: what the layouts look like, how touches
play, what the settings are. Its code is not ported line by line.

## Language, build and shared code

- **C++20 and CMake**, as StageHand, Umpire and uw-core, so code can move
  between them.
- **uw-core** (`~/Dev-Common/uw-core`) is a git submodule at
  `external/uw-core`, pinned to a commit, as Umpire's decisions.md describes.
  The core uses only `uw::ump` (built with `UW_CORE_UMP_ONLY`): `MidiMessage`,
  the status and attribute enums, `scaleUp`, and `downconvert` for the MIDI
  1.0 fallback.
- **uw-core has no UI code.** It is UMP, the CLAP host, and the MIDI and audio
  backends. Umpire's UI is DGL and Dear ImGui from the DPF fork, a separate
  repo (Umpire's decisions.md, "DPF fork"). This app's UI is its own choice
  ("UI", below), and uw-core's part is the MIDI side.
- **uw-core needs an output side.** `uw::midi` has only `MidiInput`. This app
  needs a `MidiOutput` seam: send UMP to a chosen destination, and publish a
  UMP endpoint of its own with a function block (ALSA sequencer:
  `snd_seq_create_ump_endpoint` / `snd_seq_create_ump_block`; CoreMIDI:
  `MIDIUMPMutableEndpoint`, macOS 15). It belongs in uw-core, where StageHand
  and Umpire can use it too (roadmap, milestone 3).

## The core is toolkit-free

`kedo::core` (`src/kedo/`) holds the tuning, the layouts as geometry, the
playing logic and UMP output, with no toolkit types: Umpire's view-model rule.
The UI draws `Key`s and passes pointer events in layout coordinates; the
transport sends what `UmpOutput::take()` returns.

- `Tuning`: EDO, origin, frequencies, note numbers, labels, Pitch 7.9 and
  7.25 encodings.
- `Layout` and its three kinds (below), with `keyAt()` hit-testing.
- `Performer`: KeyEDO's touch handling, once for all layouts. Each pointer is
  tracked by id, so multitouch is the normal case, not an iOS special case.
- `NoteSink`: what is played, before it's MIDI (note on/off, glide,
  expression with or without a note). The arpeggiator will be a NoteSink
  between the Performer and the output.
- `UmpOutput`: a NoteSink that makes UMP.

## Layouts

The three KeyEDO keyboards stay:

- **Piano at 12-EDO**: a standard piano (`PianoLayout`), KeyEDO's key sizes.
- **Piano at any other EDO**: KeyEDO's diamond hybrid (`DiamondLayout`). One
  column per 12-EDO semitone holds the steps that fall in it, stacked; black
  columns sit half a diamond up so columns interlock like piano keys; E-F and
  B-C are a white key apart. Semitones with no step (EDOs under 12) keep their
  place. Steps are assigned to semitones arithmetically
  (`floor(12 * step / edo)`), which fixes KeyEDO's grouping under 12-EDO,
  where a skipped semitone shifted the columns after it.
- **Tiled at any EDO**: an isomorphic lattice of diamonds (`TiledLayout`).

### Tiled lattice

KeyEDO's tiling shifted odd rows half a tile right and even rows not at all,
with `step = col * g1 + row * g2`. Then up-right from an even row adds `g2`
but from an odd row `g1 + g2`, so a chord's shape depends on the row it
starts in: not isomorphic. (Visible in KeyEDO's 17-EDO Wicki-Hayden screen:
F2 is +10 up-right of A#1, but D#3 is +13 up-right of F2.)

Here each row is shifted half a tile further than the one below, so tile
`(col, row)` plays `col * right + row * upRight` and every direction has one
interval everywhere: right `right`, up-right `upRight`, up-left
`upRight - right`. The playable keys form a band leaning to one side, with
unplayable tiles drawn grey in the corners, as KeyEDO's.

The presets keep KeyEDO's names, re-expressed for the new lattice:
- Wicki-Hayden: right a whole tone, up-right a fifth, so up-left a fourth (the
  standard hex layout).
- Harmonic Table: up-right a major third, up-left a minor third, so two rows
  up is a fifth (the standard one). KeyEDO's `g1 = maj3, g2 = min3` gave
  up-left a descending semitone.
- Janko: whole tones along a row, each row a step higher.
- Bosanquet and Guitar (fourths): KeyEDO's numbers (right 1 / up-right a
  fifth; right a whole tone / up-right a fourth). **To check** against the
  layouts the author intends.

## Two layouts KeyEDO doesn't have (decided 2026-10-09)

### Octave columns

Each column holds one scale degree in every octave, stacked corner to corner
as the diamond piano's columns: above A-1 is A0. The column up and to the
right holds the next degree (at 12-EDO, A#), and so on.

The first plan (2026-10-09) made it a Tiled generator, up-right 1 and right
2 - EDO, which keeps it isomorphic. Built that way, each column started half
a tile above the one before, so the tile right of A6 was B5. **The author's
layout (2026-10-13) can't be isomorphic**, and is its own `OctaveLayout`:
- Two octaves of columns, one per degree: at 12-EDO A, A#, ... G#, A, ... G#.
  Every other column sits half a tile higher, as the diamond piano's black
  keys, so the zigzag along the bottom is chromatic: A-1, A#-1, ... G#0, A0,
  ... G#1. Column c at level o plays the step c + EDO * o above the lowest.
- So right of A6 is B6 (+2), up-right of A6 is A#6 (+1), straight up an
  octave, and after G#1 comes A1 (down and right of it, in the second octave
  of columns); but up-right of A#6 is B7 (+EDO + 1).
- Levels enough for the first column to reach the last note; tiles without
  a note number are grey.
- It's still the last preset in the Tiled generator list
  (`Generator::octaveColumns`), so documents store it as KeyEDO's do, by
  `selectedPresetIndex`, and KeyEDO's own presets keep their indexes.

### The lowest key, and the default origin (decided 2026-10-13)

- **Every tile layout (Tiled and Octave columns) starts at the origin note
  in octave -1** (`Tuning::lowestStep()`): A-1 with an A origin, C-1 with a C
  one, whatever the origin octave. It's the bottom left key, and the view
  opens on it. At 12-EDO the notes below it (C-1 to G#-1 for A) are no
  longer shown; at other EDOs the keys from it up to the origin are shown
  grey, as they have no note number.
- Not the piano: the standard keyboard keeps its own range, and the diamond
  piano starts at the origin.
- **Note numbers are as before:** at EDOs other than 12, the origin note and
  octave get note 0, and keys past note 127 are grey; 12-EDO keeps standard
  MIDI numbers.
- **The default origin is A0** (27.5 Hz), in the piano's lowest octave,
  instead of A1. A document that names its origin octave keeps it.

### Custom layout

Keys the player places and edits, in the app's Edit mode (not a separate
editor app: decided 2026-10-13, "Documents with several layouts"):
- **Editing:** drag keys to move them (snapping to a grid optional), add,
  delete and duplicate them, and an inspector (ImGui) for the selected key's
  label, colour, shape and size. Start blank or from any layout ("convert to
  custom"). Saved as one layout file in the document's bundle.
- **What a key sends:** a note number, group and channel, and for a Pitch
  7.9 key, its pitch as **EDO / octave / degree** (e.g. 17-EDO, octave 3,
  degree 5), independent of the current tuning, so one layout can mix EDOs.
  Degree 0 is the current tuning's origin note (decided 2026-10-09): with
  the origin at A, 17-EDO / octave 3 / degree 5 is five 17-EDO steps above
  A3. Since each layout now has its own tuning, "the current tuning" is the
  custom layout's own. Kept (the author, 2026-10-13): changing a custom
  layout's origin note or octave transposes every key, as is easy to undo.
- **Note number:** the editor suggests one and the player can override it.
  The suggestion (decided 2026-10-13): the **nearest free** number, the
  nearest 12-EDO note unless another key on the same group and channel has
  it, then the closest unused one. So a receiver that ignores Pitch 7.9
  still plays close to the right pitch, and keys finer than 12-EDO don't
  share a number (MIDI 2.0 would treat them as one note for per-note
  messages).
- **Duplicates** a player makes by overriding a number are left for later:
  the editor will need to warn.
- **Mixed EDOs** (confirmed 2026-10-13): each key keeps its own EDO, octave
  and degree, so changing the layout's EDO never invalidates the keys; the
  layout's EDO is only the default for keys added.
- **Shapes** (2026-10-13): diamond or rectangle, chosen per key; rectangles
  are what a converted 12-EDO piano is made of. No hexagons.
- **Selecting:** in Edit mode a click on a custom layout's key selects it
  (Shift adds to the selection) rather than playing it; auditioning is the
  inspector's, and playing is Play mode's. A generated layout's keys still
  play in Edit mode.
- **Snapping:** key centres to a square grid of half a diamond's width (the
  generated diamond layouts' centres lie on it, and their neighbours share
  edges), with a finer half-step grid. Space between keys comes from making
  them smaller, not from spacing the grid.
- **Converting** keeps every playable key's place, shape, colour, pitch and
  note number, so a converted layout plays exactly what it did. In the file
  the layout says it's Tiled (`keyLayoutViewType` 1), with its keys in
  `customKeys`, so KeyEDO opens it as something rather than failing.
- **As built** (session 1, 2026-10-13): `CustomKey` in `LayoutConfig`, so
  editing keys rebuilds the keyboard as any setting does; `CustomLayout`
  (`src/kedo/CustomLayout.*`). Keys of one note number, group and channel
  are one note to the Performer. A key carries its look: a colour (a
  converted Tiled layout's tile colours) or a shade (a converted piano's
  black and white), and two labels, the converted layout's name and
  degree or frequency. Its label ink is dark or light by the fill. A
  converted layout's grey, unplayable tiles aren't kept. Converting keeps
  the view; the Layout menu's "Custom" starts from whatever keys the layout
  had (none at first).
- **No rectangles** (the author, 2026-10-13): custom keys are all diamonds;
  converting the 12-EDO piano gives the diamond piano's arrangement of its
  keys (one each, black half a diamond up), keeping its range and its black
  and white.
- **Labels follow pitch** (the author, 2026-10-13; built in milestone 9):
  a key's label and the line under it each have a *style*: typed text, the
  note's name and octave, the name alone, the degree (1-based) or the
  frequency. All but text are worked out from the key's pitch when it's
  drawn, so they follow any change of it. Converting gives each key the
  styles of the layout it came from ("A" over "110.0" is name over
  frequency); a new key's label is its name and octave; typing a label makes
  it text. The file stores the style (`labelStyle`, `sublabelStyle`) and the
  text only for text.
- **The editor, as built** (session 2, 2026-10-13), in the panel's Keys
  section and on the keyboard:
  - Click selects, Shift-click adds or removes, drag moves (the grabbed key
    snaps, the rest keep their places relative to it), double-click on
    empty space adds a key, a click there clears the selection. Selected
    keys draw on top, and a click finds them first.
  - Keys: Del or Backspace deletes, Cmd/Ctrl-D duplicates (a key to the
    right, numbers suggested afresh), Cmd/Ctrl-A selects all, arrows nudge a
    grid step (Shift: four), Escape clears the selection, Cmd/Ctrl-Z undoes
    and Cmd/Ctrl-Shift-Z (or -Y) redoes. Undo covers keys and selection,
    up to 200 steps, per layout shown (switching layouts clears it).
  - A new key: the selected key's pitch a step up (or the origin note in
    octave 4), in the layout's EDO; its note number suggested; its label its
    note's name.
  - The inspector: one key's EDO / octave / degree (the degree wraps into
    the octave), with its note name, cents and frequency; several keys'
    pitches move together by "- step / + step / - octave / + octave", each in
    its own EDO. Note number (one key's): suggested until typed, "Suggest"
    to go back, and a warning when it's shared with a key of another pitch.
    Group and channel: the layout's or a key's own. Shade, own colour,
    width and height (menus of 50%, 100%, 150% and 200% of the standard
    diamond, 100% the default: the author, 2026-10-13), label and the line
    under it, "Label from pitch". A suggested number and a label that was
    the old note's name follow a pitch change.
  - Auditioning: a pitch, number or route change sounds the key's note for
    400 ms through the outputs.
  - While editing keys the view isn't clamped to them (it would slide keys
    from under the pointer, and leave no room to add); Reset View still
    finds them. Snapping is to the grid of half a diamond's width, or half
    that ("Half steps"); off, keys go where they're put.
- **Auditioning** (decided 2026-10-09): changing a key's values in the
  inspector sends that key's note through the configured output, as playing
  it would, so a DAW or synth listening there hears the change and can use
  it. With no output, nothing is sent.

### Keys carry the note they play (done 2026-10-09, roadmap milestone 4)

Both, and the custom layout above all, need keys that aren't steps of one
Tuning. So every `Key` carries a `Note` (`src/kedo/Note.h`):
- `id`: names the note while it sounds. The Performer holds notes by it, and
  NoteSink calls after `noteOn` refer to it. Keys that play the same note
  share it (Tiled's repeated steps); tuning-based layouts use the step.
- `number`, and `group` and `channel` (unset: the output's).
- `pitch`: 12-EDO semitones above MIDI note 0, for Pitch 7.9 and 7.25.

Tuning-based layouts fill it in with `Tuning::note(step)`; the custom layout
will store what the player set. `Key::step` stays, for labels and colours.
`UmpOutput` no longer knows the tuning: everything it sends comes from the
Note. The 12-EDO rule became general: **a note-on carries Pitch 7.9 unless
its pitch equals its number**, so at 12-EDO, and for any custom key set that
way, it's a plain note-on. A MIDI 1.0 destination gets the nearest 12-EDO
note to the pitch. All Notes Off now reaches every group and channel a note
used, as well as the output's.

### The settings follow the layout

Some settings don't apply to every layout: a custom layout can mix EDOs, so
the global EDO, Generator and Tile Color don't apply to it, though the
origin does (it's degree 0). The settings panel shows only what the current
layout uses (done in milestone 6; the custom layout's in milestone 8).

## Note numbers and pitch

As KeyEDO: at 12-EDO, standard MIDI note numbers; at other EDOs, steps are
numbered from 0 at the origin (at most 128), and each note-on carries a Pitch
7.9 attribute with the real pitch. A MIDI 1.0 destination gets the nearest
12-EDO note instead.

## MIDI 2.0 use

What `UmpOutput` sends to a MIDI 2.0 destination (KeyEDO's controller roadmap
items in brackets):
- Note-on with 16-bit velocity and Pitch 7.9 (none at 12-EDO, where the note
  number is the pitch). Velocity is at least 1: a MIDI 2.0 note-on with
  velocity 0 isn't a note-off (KeyEDO's "To check").
- Per-Note Management with Reset before each note-on, on by default, so a
  retriggered note has no per-note bend or controllers left over [5].
- Fixed drag: **per-note** pitch bend, registered per-note controllers (the
  vertical controller number, e.g. 74 Brightness), and polyphonic pressure,
  each finger on its own note [1, 4]. **Each message's scope is chosen
  separately** (decided 2026-10-09): pitch bend per note or channel,
  controller per note (registered per-note controller) or channel (CC),
  pressure poly or channel. Channel-scope messages come from the first
  finger down, as KeyEDO; per-note ones from each finger. Per-note bend and
  controllers need "Expression as MIDI 1.0" off (below); poly pressure
  doesn't, since MIDI 1.0 has it. Documents store `bendScope`,
  `controllerScope` and `pressureScope` ("note" / "channel"), and read the
  earlier single `perNoteExpression` switch. Channel-wide expression, as KeyEDO,
  remains a setting (`perNoteExpression = false`): then the first pointer
  down drives the channel.
- 32-bit channel pitch bend and controllers [6].
- Bend ranges with `setup()`: registered controller 0 and, for per-note bend,
  7 [2].
- Gliss with **glide** (a setting): the struck note keeps its number and its
  pitch moves to each key passed over, with registered per-note controller 3
  (Pitch 7.25), instead of a new note per key [3]. Stepwise for now: it lands
  on each key's exact pitch, without sliding between them.
- All Notes Off: a note-off for each sounding note, then CC 123, as KeyEDO.

For a MIDI 1.0 destination it makes MT 0x2 messages: notes at the nearest
12-EDO note with 7-bit velocity, per-note expression moved to the channel
(polyphonic pressure stays per note), registered controllers as CC
101/100/6/38 (uw-core's `downconvert`), and a glide restruck as note-off and
note-on at the nearest note.

The rest of KeyEDO's controller roadmap (endpoint and function blocks,
Product Instance ID, stream configuration, timestamps, Property Exchange,
Flex Data tempo) needs the transport, so it's in the roadmap. Its MPE fallback
(controller roadmap 13) is rejected: see "No MPE".

## The window (decided 2026-10-09, roadmap milestone 6)

- **Toolbar** across the top, as KeyEDO's: Open and Save on the left; the
  protocol badge in the middle (what the destination gets, or MIDI 2.0 when
  only publishing); on the right, routing (a popup: Send to, Publish),
  Lock, Latch, All Notes Off, the monitor and the settings panel.
- **Icon buttons**, drawn with ImGui's draw list (`src/app/Icons.*`) after
  KeyEDO's SF Symbols, rather than an icon font: no new dependency, sharp at
  any scale. Each has a tooltip.
- **Synopsis** under the toolbar: KeyEDO's one line (`Synopsis.*`), with the
  routing added.
- **Settings in a side panel** on the right (the author's choice over a
  modal sheet), so changes can be heard while it's open: Tuning, Layout,
  Playing, MIDI, File, each section showing only what the current layout
  uses ("The settings follow the layout", above).
- **The MIDI monitor is toggleable**, under the keyboard. Whether the panel
  and the monitor are open are preferences, remembered.
- **Scrolling follows the fingers:** SDL's wheel values already carry the
  system's direction ("natural" on the Mac), so the view moves by them, not
  against them.

## Sending MIDI (decided 2026-10-09, roadmap milestone 5)

As KeyEDO's routing menu, through uw-core's `MidiOutput` (`src/app/MidiTargets.*`):
- **Send to** one destination. Its own protocol decides what it gets: a MIDI
  2.0 destination gets MIDI 2.0, a MIDI 1.0 one the fallback. The protocol
  is no longer a setting; the bar shows it as KeyEDO's badge does.
- **Publish** a "KeyboardEDO" source other apps receive from, always MIDI
  2.0 (CoreMIDI converts for MIDI 1.0 receivers such as REAPER). As KeyEDO
  (decided 2026-10-09, after it worked only sometimes with REAPER):
  - it has a fixed CoreMIDI unique ID, "KBED" (KeyEDO's is "KEDO"), through
    uw-core's `openVirtual(name, protocol, persistentId)`, so a host sees the
    same device on every launch instead of a new one;
  - whether to publish is an **app preference**, remembered across launches
    in SDL's per-user preferences folder (`Preferences.h`), not part of the
    document; so once on, the source is there whenever the app runs;
  - an app already running when the source appears may still need to rescan
    (REAPER: Reset all MIDI devices); the Publish tooltip says so.
- Each has its own `UmpOutput`, fed by one Performer through a tee; with
  neither, a preview output keeps the monitor working.
- A destination is remembered by id or name (KeyEDO documents name it):
  if it's missing, it stays chosen, shown "(not found)", and opens when it
  appears. One that disappears is reopened when it comes back.
- Notes go out right after each batch of input events, before drawing.
- Changing the destination, publishing, or the MIDI settings releases what's
  held first; quitting sends note-offs and All Notes Off.
- Known gap: the bend-range setup goes out when a target opens, so a
  receiver that connects to the published source later misses it (as with
  KeyEDO). Milestone 11's stream messages are the place to fix it.
- **On Linux** (2026-10-12, milestone 3b) it's uw-core's `AlsaSeqOutput`, on
  two UMP sequencer clients: "KeyboardEDO" (MIDI 1.0) and "KeyboardEDO MIDI
  2.0". The published source is the port "KeyboardEDO" on the MIDI 2.0
  client, so its id is "KeyboardEDO MIDI 2.0:KeyboardEDO"; ALSA has no
  persistent id, so hosts find it again by that name. Destinations include
  legacy (pre-UMP) clients such as FluidSynth's and `aseqdump`, which count
  as MIDI 1.0 and get ordinary sequencer events from the kernel.
- A MIDI 1.0 receiver of the **published** source, on either platform, gets
  the system's conversion: the note number as sent, without Pitch 7.9 (on
  Linux, `aseqdump` subscribed to it showed note 69 for a 31-EDO C4).
  "Send to" a MIDI 1.0 synth instead, for the nearest note and pitch bend.
- Where the system's MIDI can't be reached (on Linux, no `/dev/snd/seq` or no
  UMP support), the bar says "no MIDI output" and the monitor still shows
  what would be sent.

## Expression as MIDI 1.0 (decided 2026-10-09)

The author had DAWs, Logic Pro first, mishandle MIDI 2.0 pitch bend and
controllers from KeyEDO. KeyEDO's code shows the workaround it settled on
(`sendPitchBend`, `sendCC`, `sendChannelPressure` in KeyboardMIDIManager.swift,
commits "Vertical and horizontal MIDI messages" and "Updates from Polygraph
Modular"): bend and CC always went out as MIDI 1.0 (MT 0x2 in a MIDI 1.0 event
list), even to MIDI 2.0 destinations, "matching what Logic Pro does when
forwarding to an AU plugin"; channel pressure even as raw MIDI 1.0 bytes,
because Polygraph Modular's dispatcher dropped the UMP form. Notes stayed
MIDI 2.0 with Pitch 7.9.

So `OutputConfig::midi1Expression` (the panel's "Expression as MIDI 1.0"):
- **On by default**, so Logic and REAPER work as with KeyEDO. **Off** for
  receivers that handle MIDI 2.0 fully: StageHand and Umpire, where the
  author means to make it work properly. Saved in the document.
- On: pitch bend, controllers, pressure, the bend-range setup (CC
  101/100/6/38) and All Notes Off's CC 123 go out as MIDI 1.0; pitch bend and
  controllers are channel scope (their scope controls are greyed out), driven
  by the first finger. Pressure keeps its own scope: poly or channel. Notes, Pitch 7.25 glides and Per-Note
  Management stay MIDI 2.0.
- CoreMIDI converts the MIDI 1.0 messages for a MIDI 2.0 receiver, so such a
  receiver still sees MT 0x4, converted as from a hardware keyboard (checked:
  the bend-range CCs arrive as a MIDI 2.0 RPN). A MIDI 1.0 receiver (REAPER)
  gets the bytes.
- Not copied: KeyEDO's raw-bytes channel pressure (MIDISend), which existed
  for Polygraph Modular. uw-core's output is UMP only; revisit if a receiver
  drops MIDI 1.0 UMP pressure.

## What the synths do with it (2026-10-12)

The author's CLAP synths (PlainOldSynth, StruckSynth, IFSynth, in `~/Dev-Audio`) were brought up to
date with what `UmpOutput` sends, so they are the receivers to test against:
- Pitch 7.9 notes; notes matched on note number, channel and group.
- Registered controllers 0 and 7 set the channel and per-note bend ranges, per channel, and survive a
  host's `reset()`. With no registered controller 7 the per-note range is the channel's (M2-104
  §4.2.12), so at KeyboardEDO's defaults (±2 and ±2) the "known gap" above is harmless: a synth that
  connects after the setup still bends the right amount.
- Per-note pitch bend, after the synth's glide, and Pitch 7.25 (registered per-note controller 3) as
  an absolute pitch for the sounding note, which both bends offset. Per-Note Management Reset clears
  both, as the reset before each note-on intends.
- A MIDI 2.0 note-on with velocity 0 is a note-on in all three now. Keep the velocity floor of 1
  anyway: with velocity → loudness on, a velocity of 0 is silent.
- Polyphonic pressure and per-note controller 74: PlainOldSynth routes them as modulation sources and
  StruckSynth to its Expression panel; IFSynth doesn't use them yet.

## No MPE (decided 2026-10-11)

KeyboardEDO doesn't send MPE, in any mode. It mainly drives the author's own
MIDI 2.0 stack (PlainOldSynth and the other synths, StageHand, Umpire), and
two aims set the line: push toward MIDI 2.0, and keep support for transitional
MIDI 1.0 schemes small. The policy across the projects is to support no MPE
feature that MIDI 2.0 duplicates: the synths accept MPE as a compatibility
input, StageHand passes it through, Umpire converts it to MIDI 2.0 on input,
and nothing originates it.

- **The MIDI 1.0 fallback stays minimal**, as described under "MIDI 2.0 use":
  notes at the nearest 12-EDO note, expression on the channel. A MIDI-1.0-only
  synth therefore plays EDO layouts rounded to 12-EDO. That's accepted.
- **Rejected: the MPE fallback** (KeyEDO's controller roadmap 13): rotating
  notes across channels with a bend each, so microtonal playing works on
  MIDI 1.0 synths. It's the scheme MIDI 2.0's Pitch 7.9 and per-note pitch bend
  replace.
- **Rejected: MIDI Tuning Standard SysEx** (per-key retuning for MIDI 1.0
  synths). Not MPE, but another MIDI 1.0 workaround for what Pitch 7.9 does,
  with the same support cost.
- **Unaffected: "Expression as MIDI 1.0"** (above). It works around DAWs that
  mishandle MIDI 2.0 bend and controllers; it doesn't spread notes across
  channels.

## Documents (decided 2026-10-09, roadmap milestone 5)

(Superseded in part by "Documents with several layouts", 2026-10-13: a
document becomes a `.kedo` bundle of layouts. What follows is the single-file
KeyEDO format, which becomes one layout's file and what KeyEDO imports read.)

- **KeyEDO's JSON, KeyEDO's field names** (`src/kedo/Document.*`, with
  nlohmann/json as a shallow submodule pinned to v3.12.0). All 25 of the
  author's KeyEDO documents open.
- Missing fields keep defaults and bad ones are clamped, so any KeyEDO file
  opens. Fields this version doesn't use (the arpeggiator's, hover
  expression) are kept and written back unchanged.
- Every key KeyEDO reads is written, with KeyEDO's defaults where we have no
  value, so KeyEDO can open our files. Our own fields (`glide`,
  `perNoteExpression`, `perNoteManagement`, `bendRange`, `perNoteBendRange`,
  `locked`, `pianoLowest`, `pianoHighest`) are extra keys, which KeyEDO
  ignores.
- Open, Save and Save As use SDL3's native file dialogs, also on Cmd/Ctrl-O,
  S and Shift-S. The window title shows the file and "(edited)". There's no
  "save changes?" on quit yet.

## Documents with several layouts (decided 2026-10-13, roadmap milestone 8a)

A document is a **set of layouts chosen in advance**, switched between while
playing, rather than one layout reconfigured on the fly. The author's
reasons: a player wants several keyboards in one document, and a document
mixing "fixed" and custom layouts is only odd if they're different kinds of
thing. They aren't: a fixed layout's keys are generated from a few settings
(EDO, generator, origin), a custom one's are stored, and both are a set of
keys that carry their notes (milestone 4).

- **A `.kedo` document is a folder** (a bundle), as StageHand's `.rack`:

  ```
  Set A.kedo/
    document.json      format, version, the layouts in order, which one is
                       showing, the MIDI destination
    layouts/
      wicki-31.json    one file per layout
      my-hex.json
  ```

  `document.json` has `"format": "keyboardedo-document"` and a `version`; a
  newer version is refused, not guessed at. Each layout is its own file, so
  a layout can be copied between documents by copying the file, and diffs
  stay readable. On macOS the app's `Info.plist` will declare `.kedo` a
  package, so Finder and the file dialogs treat it as one file (once the Mac
  app has a bundle); on Linux it's a folder.
- **Each layout has its own tuning**, layout settings (kind and generator,
  or a custom layout's keys) and playing and MIDI settings (drag,
  expression, velocity, group, channel, bend ranges): a document can hold a
  12-EDO piano and a 31-EDO Wicki-Hayden. A layout file uses KeyEDO's field
  names, so it is shaped like a KeyEDO document.
- **Per document:** the MIDI destination. Publish stays an app preference.
- **Saving is safe**, as StageHand's: a complete new bundle is written next
  to the old (`X.kedo.tmp`), then swapped into place.
- **KeyEDO documents still open:** a `.json` file opens as an untitled
  document with one layout. Saving it writes a `.kedo` bundle; the `.json`
  is never written back. (Today's single-file documents, milestone 5, are
  KeyEDO's format, so they open the same way.)
- **Play and Edit modes.**
  - Play: the keyboard and a selector for the document's layouts. The
    tuning and layout settings are hidden, so nothing changes by accident
    mid-performance. Switching layouts releases held notes (the author's
    choice), then sends the new layout's setup (bend ranges) as a target
    change does.
  - Edit: add, remove, rename, duplicate and reorder layouts; change a
    layout's settings; "convert to custom"; and (milestone 8b) edit a custom
    layout's keys.
  - The Lock button stays what it is: no scrolling or zooming, in either mode.
- **Not a separate editor app** (the author, 2026-10-13): editing needs the
  same drawing, Performer and output as playing, and belongs in the same
  window as a mode.
- Later: switching layouts by computer key or MIDI program change.
- As built (2026-10-13): Edit or Play is an app preference, remembered,
  Edit at first. A layout's name is its file's `name` key (KeyEDO ignores
  it); a file without one is named after the file. Layout files are named
  from their layouts' names ("Wicki 31" -> `wicki-31.json`, a second of the
  same name `-2`), rewritten in full on every save. Which layout shows is
  saved, but switching isn't an edit (no "(edited)" in the title).

## Computer keyboard (decided 2026-10-13, roadmap milestone 9)

- **No QWERTY region.** KeyEDO's outlined region moved with the arrow keys
  is too confusing (the author). Instead the player **assigns a computer
  key to a note key**. On a custom layout there's no region and no border.
- The arrow keys stay the custom layout editor's (nudging keys).
- **Converting keeps them** (the author, 2026-10-13): a note key's computer
  key before Convert to custom is still assigned after it, and can be
  changed. So generated layouts have assignments of their own (a default
  mapping), which converting copies onto the custom keys.

Settled at the start of milestone 9 (the author, 2026-10-13, taking each
recommendation):
1. **Assigning:** in the inspector, "Computer key: [Q] Assign... Clear".
   Assign waits for the next key pressed; Escape cancels. A computer key
   belongs to one note key, so assigning one that's taken moves it. Only a
   custom layout's keys are assigned; a generated layout's mapping is fixed,
   and changing it means converting to custom.
2. **Default mappings**, from the origin note in octave 3 unless said:
   - 12-EDO piano: the trackers' two rows. Z X C V B N M are white keys from
     C3, S D G H J the black; Q W E R T Y U I O P [ ] white from C4, 2 3 5 6
     7 9 0 = the black. A note key holds one computer key, so the rows can't
     overlap as FL Studio's do: the Z row stops at B3, and , . / L ; are
     unused.
   - Diamond piano and Octave columns: consecutive steps zigzag across a
     pair of rows, Z S X D C F ... / ' (20 steps), then on across Q 2 W 3
     ... ] (23 steps): any EDO, and Octave columns' bottom zigzag is already
     in step order (each pair is one level of its columns, the next pair
     two levels up).
   - Tiled: the computer keyboard's staggered rows as the lattice. Z is on
     the tile of the origin note in octave 3 (the leftmost one with the
     other three rows above it, so the mapping is in view when the layout
     opens), A up-left of Z, S up-right, and so on up the Q and digit rows,
     so a chord has one shape anywhere.
3. **Octave shifting** the mapping: later.
4. **Hover expression:** while computer keys hold notes, holding Ctrl
   (Control on the Mac) and moving the mouse drives their expression as
   Fixed drag would (horizontal and vertical controls, per note or channel
   by their scopes), measured from where the mouse was when Ctrl went
   down. Releasing Ctrl or the key resets it, as lifting a finger does.
5. **Edit mode:** computer keys play there too, also on a custom layout
   whose keys a click selects, to hear a layout while editing it.

How it's built: a computer key is its **physical position** (the USB HID
usage, which SDL's scancodes are), so a mapping is the same shape on QWERTY,
AZERTY or Dvorak; the key shows the character the system's keyboard layout
gives it. Files name it as the W3C `code` does ("KeyQ", "Digit2",
"BracketLeft"). Letters, digits and the punctuation in the four rows can be
assigned. Nothing plays while a text field has focus, and Cmd/Ctrl
shortcuts aren't notes.

As built (2026-10-13):
- Each key shows its computer key, small, at the bottom of the key (on the
  piano above the C's name), as the system's keyboard layout names it. A
  View checkbox, "Computer keys", hides them; it's an app preference.
- Duplicating a custom key doesn't copy its computer key (one note key
  each). A file that puts one computer key on two keys keeps it on the
  first.
- On the diamond piano the zigzag plays consecutive steps, but as steps
  stack in columns there, it doesn't look like a zigzag on the screen.
- Hover expression uses the Playing section's Horizontal and Vertical
  controls, now shown in Gliss mode too. Releasing a key resets its own
  note's expression, and the channel's if it was the last one held.
## KeyEDO compatibility is not a goal (decided 2026-10-13)

The author: there's no need to support KeyEDO in any way. Matching KeyEDO's
Tiled presets, layouts or behaviour isn't a reason for a choice any more,
and later work needn't keep KeyEDO able to read KeyboardEDO's files. What
was built for it (KeyEDO's field names in layout files, importing its
`.json` documents, the presets' order) stays until there's a reason to
change it; whether to drop any of it is the author's call.

## Behaviour kept from KeyEDO

Gliss and Fixed drag; latch with Shift, and the Latch toggle (turning it off
releases the latched notes); striking a latched note releases it; strike
velocity (top 12% of a key loudest, falling to a minimum at the bottom);
expression over 150 points of drag; gliss holds the note across the diamond
layout's gaps (KeyEDO on the Mac). Two pointers on two tiles that play the
same step hold one note, which ends when the last lets go.

## UI: SDL3 and Dear ImGui (decided 2026-10-09)

The author chose SDL3 with Dear ImGui over DGL, NanoVG and ImGui (Umpire's
stack):
- SDL3 gives multitouch on Linux (X11 and Wayland), Windows and iOS/Android
  touchscreens, trackpad pinch on the Mac, and pen pressure, all of which a
  keyboard controller needs. pugl has no touch events and no iOS.
- This app hosts no plugins, so it doesn't need DGL's plugin-editor
  embedding, the reason Umpire uses it.

How it's set up:
- **SDL 3.4.18 and ImGui 1.92.9b are git submodules** (`external/SDL`,
  `external/imgui`), pinned to releases like uw-core, and built by our CMake
  (SDL statically, without audio or camera). `KEDO_SYSTEM_SDL=ON` uses an
  installed SDL3 instead. `KEDO_APP=OFF` builds only the core and its tests.
- **Rendering: SDL_Renderer** through ImGui's `sdlrenderer3` backend. The
  keyboard is drawn into ImGui's background draw list, so there is one
  renderer and one draw pass.
- **Only `src/app/` knows SDL and ImGui.** The core supplies the geometry
  (`Layout`), the view transform (`Viewport`), colours and labels
  (`KeyStyle`), and monitor text (`UmpText`).
- **Input:** touchscreen fingers go to the Performer by finger id; the mouse
  is pointer 0. SDL's mouse copies of touches (`SDL_TOUCH_MOUSEID`) are left
  to ImGui, so panels still work by touch and no note is played twice.
  Trackpad fingers (indirect touch devices) are ignored: they have no place
  on the screen. Wheel scrolls, Ctrl or Cmd with the wheel zooms, and a
  trackpad pinch zooms (SDL 3.4's pinch events). Lock stops both. A
  keyboard that fits the window's height scrolls sideways with a plain
  wheel; a taller one (Octave columns) scrolls vertically, and sideways with
  Shift held. Zoom buttons by Reset View, Cmd/Ctrl = and -, and Cmd/Ctrl 0
  for zoom 1 zoom about the keyboard's centre (2026-10-13: the author found
  no way to scroll Octave columns sideways or zoom with a plain mouse; the
  panel now says how).
- **Fonts:** ImGui's built-in vector font, and Cousine (from ImGui's
  `misc/fonts`, embedded at configure time) for the MIDI monitor.
- **Redraw on demand:** the loop waits for events, draws a few frames after
  each, and draws every frame while a pointer is down.
- **Linux** (first built 2026-10-12, Ubuntu with GCC 15): SDL is built with
  both X11 (XInput2 multitouch) and Wayland (libdecor for window
  decorations), and picks Wayland when the session has it;
  `SDL_VIDEO_DRIVER=x11` forces X11. The app code needed no changes. The
  packages SDL needs beyond a desktop's usual X11 ones are in the README.

## Working method

As uw-core and Umpire: one milestone per session, about a day each. Each
starts by reading this file and the roadmap. Tests are silent. The author
commits to `main`; ask before pushing, and push uw-core before a pin that
points at it (`git push --recurse-submodules=check`).

**uw-core changes are made in `~/Dev-Common/uw-core`, never in
`external/uw-core`** (decided 2026-10-09). Each clone is its own repository,
so a commit made in the submodule doesn't show in the other clone until it
goes through GitHub; one working clone avoids that. Commit, test and push
there, then move the pin here:

```bash
git -C external/uw-core pull
git add external/uw-core
git commit
```

The submodule is only ever a checkout of a pushed commit.
