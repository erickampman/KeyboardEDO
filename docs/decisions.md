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
as the diamond piano's columns: above A0 is A1. The column up and to the right
holds the next degree (at 12-EDO, A#), and so on.

It's a Tiled generator, not a new layout: up-right = 1 step and right =
2 - EDO, so two rows straight up adds 2 x 1 - (2 - EDO) = EDO steps, an
octave, and every direction keeps one interval (still isomorphic). It needs:
- `TiledLayout` to accept a negative `right` (it clamps to 1 now, and its
  range and `keyForStep` arithmetic assume positive);
- rows enough for every octave (about 20 at 12-EDO), with vertical
  scrolling, instead of rows fitted to the window's height.

### Custom layout

Keys the player places and edits:
- **Edit mode:** drag keys to move them (snapping to a grid optional), add,
  delete and duplicate them, and an inspector (ImGui) for the selected key's
  label, colour, shape and size. Start blank or from any layout ("convert to
  custom"). Saved as JSON.
- **What a key sends:** a note number, group and channel, and for a Pitch
  7.9 key, its pitch as **EDO / octave / degree** (e.g. 17-EDO, octave 3,
  degree 5), independent of the current tuning, so one layout can mix EDOs.
  Degree 0 is the current tuning's origin note (decided 2026-10-09): with
  the origin at A, 17-EDO / octave 3 / degree 5 is five 17-EDO steps above
  A3.
- **Note number:** the editor suggests one and the player can override it.
  The suggestion: the nearest 12-EDO note, so a receiver that ignores Pitch
  7.9 still plays close to the right pitch.
- **Duplicates** (two keys, one note number, different pitches, on one
  channel) are left for later. MIDI 2.0 treats them as one note for per-note
  messages, so the editor will need to warn or allocate.
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
  trackpad pinch zooms (SDL 3.4's pinch events). Lock stops both.
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
