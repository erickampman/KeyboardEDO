<div style="display: flex; align-items: center; gap: 0.75rem; margin: 1.5rem 0 0.5rem;">
  <a href="https://www.unlikelyware.com" target="_blank" rel="noopener noreferrer">
    <img src="Unlikelyware.png" alt="Unlikelyware" style="width: 56px; height: auto; border-radius: 10px; margin: 0; display: block;">
  </a>
  <h1 style="margin: 0;">KeyboardEDO</h1>
</div>

<p>
  <em>KeyboardEDO runs on macOS and Linux. Shortcuts are written with Cmd, as on the Mac; on Linux, use Ctrl.</em>
</p>

~~~note [card]
## On This Page
[Overview](#overview) · [Getting Started](#getting-started) · [The Window](#window) · [Edit and Play](#modes) · [Documents](#documents)

**Keyboards —** [Tuning](#tuning) · [Layouts](#layouts) · [Piano and Diamond Piano](#piano) · [Tiled](#tiled) · [Custom](#custom) · [Computer Keys](#computer-keys)

**Playing —** [Playing](#playing) · [Expression](#expression) · [Performance Bar](#performance) · [Arpeggiator](#arpeggiator) · [MIDI Output](#midi) · [Shortcuts](#shortcuts) · [Troubleshooting](#troubleshooting)
~~~
~~~note [card]
(#overview)
## Overview
KeyboardEDO is an on-screen MIDI keyboard controller for **equal divisions of the octave** (EDOs): 12-EDO, 17, 19, 22, 31, 53, or anything else from a handful of steps to hundreds. You play it with a mouse, a trackpad, a touchscreen (multitouch), or your computer's keyboard, and it sends MIDI to a synth, a DAW, or another app.

It's built on **MIDI 2.0** first. Each note carries its exact pitch (MIDI 2.0's Pitch 7.9), so a 31-EDO step sounds as a 31-EDO step on any receiver that honors it, with per-note pitch bend, per-note controllers and per-note pressure. For MIDI 1.0 receivers it falls back to MIDI 1.0 automatically.

Its keyboards come from KeyEDO: a standard piano at 12-EDO, a **diamond piano** at other EDOs, isomorphic **tiled** layouts at any EDO, and **custom** layouts you build key by key. A document holds several layouts, each with its own tuning, so you can switch between them while you play.
~~~
~~~note [card]
(#getting-started)
## Getting Started
1. **Choose where notes go.** Click the antenna button at the right of the toolbar, then either pick a destination under **Send to** (a synth, a DAW's input, or a hardware port), or turn on **Publish as "KeyboardEDO"** so other apps can select KeyboardEDO as an input.
2. **Check the badge** in the middle of the toolbar. It shows **MIDI 2.0** or **MIDI 1.0** once notes are going somewhere, and **not sending** until then.
3. **Pick a tuning and layout.** Open Settings (the sliders button at the far right), set **EDO steps** under Tuning, and choose a **Layout**.
4. **Play.** Click or touch the keys, or turn on **Computer keys** under View and play from your keyboard.

The line under the toolbar sums up what's playing: the EDO, the origin note, the layout, the drag mode, and where notes go.
~~~
~~~note [card]
(#window)
## The Window
**The toolbar, left to right:**
- **Unlikelyware logo**: opens unlikelyware.com.
- **Performance bar**: shows or hides the bar on the left (bend, mod wheel, knobs, arpeggiator).
- **New**, **Open** and **Save**: documents (Cmd-N, Cmd-O, Cmd-S; Shift-Cmd-S saves as).
- **Layout menu**: the document's layouts. Switching releases any notes held.
- **Edit / Play**: the mode (see [Edit and Play](#modes)).
- **Badge**: the protocol notes are sent in.
- **Antenna**: where notes go ([MIDI Output](#midi)).
- **Lock**: turns off scrolling and zooming, so a stray swipe can't move the keyboard. Keys still play.
- **Latch**: released notes keep sounding until you turn Latch off or strike them again. Holding Shift while you play latches too.
- **Arpeggiator**: on or off ([Arpeggiator](#arpeggiator)).
- **Panic**: All Notes Off.
- **Monitor**: a list of every MIDI message sent, as UMP.
- **Settings**: the settings panel on the right.

**Moving around the keyboard:** the scroll wheel scrolls, Shift-wheel scrolls sideways, and Ctrl-wheel or a pinch zooms. **Reset View** under View puts it back.
~~~
~~~note [card]
(#modes)
## Edit and Play
KeyboardEDO has two modes, switched with the button beside the layout menu.
- **Edit** shows everything: Tuning, Layout, Playing and MIDI settings, and the layout editor for custom layouts. Use it to set up a document's layouts.
- **Play** hides the setup, leaving the layout menu, the view, and the performance bar. Use it when performing, so nothing gets changed by accident.

In both modes you choose which layout is showing from the toolbar's layout menu.
~~~
~~~note [card]
(#documents)
## Documents
A KeyboardEDO document (`.kedo`) holds **several layouts**, each with its own tuning, layout, playing settings, MIDI settings and arpeggiator. It also remembers where notes go, the performance bar's knob assignments, and the notes held when it was saved (latched, or kept by the arpeggiator's Hold), in the order played.
- **Layouts** in Settings: **Add** a new layout after the current one, **Duplicate** it, **Remove** it, rename it, or move it **earlier** or **later** in the list.
- **Open** reads a `.kedo` document, or a KeyEDO document (`.json`), which comes in as a new document of one layout.
- The window's title shows the document's name, and **edited** when it has unsaved changes.
- KeyboardEDO starts where it left off: the document you had when you quit, saved or not, with the notes you were holding. **New** starts an untitled one.
~~~
~~~note [card]
(#tuning)
## Tuning
Under **Tuning** in Settings (Edit mode):
- **EDO steps**: how many equal steps divide the octave.
- **Origin note** and **Origin octave**: the note the tuning is built from. Its frequency is shown below, from A4 = 440 Hz: A0 is 27.50 Hz.

Every key's pitch is a number of EDO steps from the origin.
~~~
~~~note [card]
(#layouts)
## Layouts
Under **Layout** in Settings, choose **Piano**, **Tiled** or **Custom**.
- **Piano** is a piano keyboard at 12-EDO, and the **diamond piano** at any other EDO.
- **Tiled** is an isomorphic grid of diamond keys: every interval has the same shape wherever you play it.
- **Custom** is a layout of keys placed one by one, made by converting any other layout.

**Convert to custom** turns the current layout into a custom one, with every key in place, so you can start from a layout that's nearly right.
~~~
~~~note [card]
(#piano)
## Piano and Diamond Piano
At **12-EDO**, the Piano layout is a standard piano keyboard.

At any **other EDO** it becomes the **diamond piano**: each piano key's steps are stacked as diamonds, so the white and black keys keep their familiar places, and the extra steps of the EDO sit between them.
~~~
~~~note [card]
(#tiled)
## Tiled Layouts
A tiled layout is a grid of diamond keys. Moving one key **right**, **up-right**, or **up-left** always moves by the same number of EDO steps, so a chord or scale has the same shape everywhere.

**Generator** chooses those steps. Each is worked out for the current EDO:
- **Wicki-Hayden**: right is a whole tone, up-right a fifth.
- **Bosanquet**: right is one step, up-right a fifth.
- **Janko**: right is a whole tone, up-right one step.
- **Harmonic Table**: neighbouring keys form major and minor thirds and fifths.
- **Guitar (fourths)**: right is a whole tone, up-right a fourth.
- **Default**: right is one step, up-right roughly a fifth.
- **Octave columns**: each column holds one note in every octave, two octaves across, with a chromatic bottom row.

The steps for the chosen generator are shown under it. **Tile colour** colours the keys by **Note** (one colour per note name), by **Octave**, or not at all (**Off**).
~~~
~~~note [card]
(#custom)
## Custom Layouts
A custom layout is a set of keys, each with its own place, pitch, note number and look. Edit one in **Edit** mode.
- **Select**: click a key; Shift-click to add more; Cmd-A selects all.
- **Move**: drag the selected keys, or use the arrow keys (Shift moves four times as far). **Snap to grid** keeps keys aligned; snapping to **half** the grid places them between keys.
- **Add key**: adds a key in the middle of the view, or double-click empty space.
- **Duplicate** (Cmd-D) and **Delete** (Delete or Backspace) the selected keys.
- **Undo** (Cmd-Z) and **Redo** (Shift-Cmd-Z or Cmd-Y).

The inspector edits the selected key:
- **Pitch**: EDO, octave, and steps above the origin note.
- **Note number**: **Suggest** picks the nearest free number for the key's pitch.
- **Look**: shade (white or black), its own colour, its width and height, and what its label shows (text, name and octave, name, degree, or frequency).
- **Computer key**: click **Assign**, then press a letter, digit or punctuation key (Escape cancels). Each computer key plays one key.
~~~
~~~note [card]
(#computer-keys)
## Computer Keys
Turn on **Computer keys** under View to play from your computer's keyboard. Each key shows the computer key that plays it. Tiled and piano layouts come with a mapping; in a custom layout you assign each key yourself.
- **Shift** latches the notes you play, as the Latch button does.
- **Ctrl** with the mouse sends expression for the notes your computer keys are holding: move the mouse as you would drag a key ([Expression](#expression)).
- **Escape** releases every note.
~~~
~~~note [card]
(#playing)
## Playing
Under **Playing** in Settings (Edit mode):
- **Drag** sets what happens when you drag across keys:
  - **Gliss**: each key you pass over plays, as a glissando. The drag stops at the keyboard's edges, so keys hidden under the side bars don't sound.
  - **Fixed**: the note you struck stays; dragging sends **expression** instead.
- **Glide (Pitch 7.25)**, with Gliss: instead of striking each key, one note's pitch moves from key to key (MIDI 2.0). It lands on each key's exact pitch rather than sliding between them.
- **Strike velocity**: a key plays louder toward its top and softer toward its bottom, down to the **Minimum** you set. Off, every note plays at full velocity.

Two fingers on two keys that play the same pitch hold one note, which ends when the last finger lets go.
~~~
~~~note [card]
(#expression)
## Expression
With **Fixed** drag, moving a finger or the mouse after striking a key sends expression for that note. **Horizontal** and **Vertical** choose what each direction sends:
- **Horizontal**: Off or **Pitch Bend**.
- **Vertical**: Off, **Controller** (choose its number), **Pressure**, or **Pitch Bend**.

Each has a **scope**:
- **Bend scope** and **Controller scope**: **Per note** bends or changes only the finger's own note (MIDI 2.0 per-note pitch bend and per-note controllers); **Channel** uses ordinary channel messages, driven by the first finger down.
- **Pressure scope**: **Poly** is each finger's own note (polyphonic key pressure); **Channel** is channel pressure from the first finger.

When **Expression as MIDI 1.0** is on (under MIDI), MIDI 1.0 has no per-note form, so expression is channel-wide.
~~~
~~~note [card]
(#performance)
## Performance Bar
The bar on the left, shown and hidden with the toolbar's first button, has controls that act on the whole channel, like a keyboard's wheels and knobs. It's the same for every layout.
- **Bend**: a pitch-bend fader over the bend range set under MIDI. It springs back to the centre when you let go.
- **Mod**: the mod wheel (CC 1).
- **Four knobs**, by default **Timbre** (CC 74), **Pressure** (channel pressure), **CC 71** and **CC 7**. Drag up or down to turn one; double-click to set it back. Click its name to choose what it sends: any controller (with your own label) or channel pressure.

The knob assignments are saved with the document. Their positions aren't: nothing is sent until you move a knob, so opening a document never changes your synth's settings.
~~~
~~~note [card]
(#arpeggiator)
## Arpeggiator
The arpeggiator steps through the notes you hold, one at a time. Its controls are in the performance bar; the toolbar's Arpeggiator button turns it on and off too.
- **Pattern**: Up, Down, Up-Down, Down-Up, As Played, or Random.
- **Division**: a step's length, from a whole note to 1/32, with triplets (T).
- **Tempo**: in beats per minute.
- **Gate**: how much of its step each note sounds.
- **Octaves**: plays the pattern, then again an octave higher for each octave more (1 to 4).
- **Velocity**: as played, a fixed velocity you set, or random.
- **Hold**: released notes keep playing, and new ones add to them; click one again to let it go. Turning the arpeggiator off keeps them, silent, to play again when it's back on, and they're still there the next time KeyboardEDO starts. **Clear** lets go of them, and of notes latched with Shift or Latch; so do All Notes Off, Escape, and turning Hold off.

Each layout has its own arpeggiator settings.
~~~
~~~note [card]
(#midi)
## MIDI Output
**Where notes go** is set from the antenna button:
- **Send to**: a destination (a synth, a DAW's input, or a hardware port). The menu shows whether each takes MIDI 2.0 or 1.0, and KeyboardEDO sends what it takes.
- **Publish as "KeyboardEDO"**: offers a MIDI source other apps can choose as an input. It keeps the same identity from one launch to the next, and the setting is remembered. On macOS 15 and later it's a MIDI 2.0 (UMP) endpoint that identifies itself to apps that ask.

Under **MIDI** in Settings (Edit mode):
- **Group** and **Channel**: where this layout's notes are sent.
- **Bend range**: the channel pitch-bend range in semitones, sent to the receiver when notes start going to it.
- **Per-note bend**: the range of per-note pitch bend (MIDI 2.0).
- **Expression as MIDI 1.0**: sends pitch bend, controllers and pressure as MIDI 1.0 messages, even to a MIDI 2.0 receiver; notes stay MIDI 2.0, with exact pitch. Logic Pro and some other DAWs mishandle MIDI 2.0 bend and controllers, so this is on by default. Turn it off for receivers that handle MIDI 2.0 fully, to get 32-bit and per-note expression.
- **Per-Note Management**: resets a note's per-note bend and controllers when it's struck again (MIDI 2.0).
~~~
~~~disclosure MIDI 2.0, MIDI 1.0, and microtonal pitch [disclosure-title subtle]
KeyboardEDO sends each note's exact pitch with MIDI 2.0's **Pitch 7.9** note attribute, so a receiver that honours it plays every EDO step exactly, with nothing else to set up.

**MIDI 1.0 has no way to give one note its own exact pitch.** Sent to a MIDI 1.0 receiver, notes go to the nearest 12-EDO note, and per-note expression becomes channel-wide. For microtonal playing, use a MIDI 2.0 receiver.

KeyboardEDO doesn't send **MPE**. MPE is a MIDI 1.0 workaround for per-note expression, and MIDI 2.0 does the same job directly. Synths that accept MPE, such as Unlikelyware's, also accept MIDI 2.0's per-note messages.
~~~
~~~note [card]
(#shortcuts)
## Shortcuts

| Key | Action |
|---|---|
| Cmd-N | New document |
| Cmd-O | Open a document |
| Cmd-S | Save (Shift-Cmd-S: Save As) |
| Cmd-= and Cmd-- | Zoom in and out |
| Cmd-0 | Zoom back to 100% |
| Escape | Release every note |
| Shift (while playing) | Latch the notes played |
| Ctrl (with the mouse) | Expression for notes held by computer keys |

**Editing a custom layout:**

| Key | Action |
|---|---|
| Cmd-A | Select all keys |
| Cmd-D | Duplicate the selected keys |
| Delete or Backspace | Delete the selected keys |
| Arrow keys | Move the selected keys (Shift: four times as far) |
| Cmd-Z | Undo |
| Shift-Cmd-Z or Cmd-Y | Redo |
| Escape | Clear the selection |
~~~
~~~note [card]
(#troubleshooting)
## Troubleshooting
- **No sound.** Check the badge: **not sending** means no destination is chosen and Publish is off (the antenna button). If you publish, make sure the receiving app has KeyboardEDO selected as an input.
- **An app doesn't see "KeyboardEDO".** An app that was already running when KeyboardEDO started publishing may need to rescan its MIDI devices. In REAPER: Preferences > MIDI Inputs > Reset all MIDI devices.
- **"No MIDI output".** The system's MIDI couldn't be reached. The monitor still shows what would be sent.
- **Pitch bend or controllers misbehave in a DAW.** Turn on **Expression as MIDI 1.0** under MIDI.
- **Notes stuck on.** Press **Panic**, or Escape.
- **Microtones play as 12-EDO notes.** The receiver is getting MIDI 1.0; see the MIDI 2.0 note under [MIDI Output](#midi).
~~~
