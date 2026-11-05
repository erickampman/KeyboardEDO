<div style="display: flex; align-items: center; gap: 0.75rem; margin: 1.5rem 0 0.5rem;">
  <a href="https://www.unlikelyware.com" target="_blank" rel="noopener noreferrer">
    <img src="Unlikelyware.png" alt="Unlikelyware" style="width: 56px; height: auto; border-radius: 10px; margin: 0; display: block;">
  </a>
  <h1 style="margin: 0;">KeyboardEDO</h1>
</div>

<p>
  <em>KeyboardEDO runs on macOS and Linux. Shortcuts are written with Cmd, as on the Mac; on Linux, use Ctrl.</em>
</p>

::: {.callout .callout-note .card}
## On This Page
[Overview](#overview) · [Getting Started](#getting-started) · [The Window](#window) · [Edit and Play](#modes) · [Documents](#documents)

**Keyboards —** [Tuning](#tuning) · [Layouts](#layouts) · [Piano and Diamond Piano](#piano) · [Tiled](#tiled) · [Custom](#custom) · [Computer Keys](#computer-keys)

**Playing —** [Playing](#playing) · [Expression](#expression) · [Performance Bar](#performance) · [Arpeggiator](#arpeggiator) · [Scale](#scale) · [MIDI Output](#midi) · [Shortcuts](#shortcuts) · [Troubleshooting](#troubleshooting)
:::

::: {.callout .callout-note .card}
## Overview {#overview}
KeyboardEDO is an on-screen MIDI keyboard controller for **equal divisions of the octave** (EDOs): 12-EDO, 17, 19, 22, 31, 53, or any other from 5 to 72 steps. You play it with a mouse, a trackpad, a touchscreen (multitouch), or your computer's keyboard, and it sends MIDI to a synth, a DAW, or another app.

It's built on **MIDI 2.0** first. Each note carries its exact pitch (MIDI 2.0's Pitch 7.9), so a 31-EDO step sounds as a 31-EDO step on any receiver that honors it, with per-note pitch bend, per-note controllers and per-note pressure. For MIDI 1.0 receivers it falls back to MIDI 1.0 automatically.

Its keyboards are a standard piano at 12-EDO, a **diamond piano** at other EDOs, **tiled** layouts of diamond keys at any EDO, most of them isomorphic, and **custom** layouts you build key by key. A document holds several layouts, each with its own tuning, so you can switch between them while you play.
:::

::: {.callout .callout-note .card}
## Getting Started {#getting-started}
1. **Choose where notes go.** Click the antenna button at the right of the toolbar, then either pick a destination under **Send to** (a synth, a DAW's input, or a hardware port), or turn on **Publish as "KeyboardEDO"** so other apps can select KeyboardEDO as an input.
2. **Check the badge** in the middle of the toolbar. It shows **MIDI 2.0** or **MIDI 1.0** once notes are going somewhere, and **not sending** until then.
3. **Pick a tuning and layout.** Click **Edit** in the toolbar, open Settings (the sliders button at the far right), set **EDO steps** under Tuning, and choose a **Layout**.
4. **Play.** Click or touch the keys, or turn on **Computer keys** under View and play from your keyboard.

The line under the toolbar sums up what's playing: the EDO, the origin note, the layout, the drag mode and its expression, what's on (strike velocity, the arpeggiator, Latch, Lock), and where notes go. A file error shows at its end, in red.
:::

::: {.callout .callout-note .card}
## The Window {#window}
**The toolbar, left to right:**
- **Unlikelyware logo**: opens unlikelyware.com.
- **Performance bar**: shows or hides the bar on the left (wheels, knobs, arpeggiator, scale).
- **New**, **Open** and **Save**: documents (Cmd-N, Cmd-O, Cmd-S; Shift-Cmd-S saves as).
- **Layout menu**: the document's layouts. Switching releases any notes held.
- **Edit**: the mode; it reads **Editing** while it's on (see [Edit and Play](#modes)).
- **Badge**: the protocol notes are sent in.
- **Antenna**: where notes go ([MIDI Output](#midi)).
- **Lock**: turns off scrolling and zooming, so a stray swipe can't move the keyboard. Keys still play.
- **Latch**: released notes keep sounding until you turn Latch off or strike them again. Holding Shift while you play latches too.
- **Arpeggiator**: on or off ([Arpeggiator](#arpeggiator)).
- **Panic**: All Notes Off.
- **Monitor**: the MIDI monitor under the keyboard, a list of every MIDI message sent, as UMP. **Clear** empties it; **Follow** keeps the newest in view.
- **Help**: this page, in your browser (F1, or Cmd-?).
- **Settings**: the settings panel on the right.

**The settings panel** has sections that fold by their titles: **Layouts**, **Tuning**, **Layout**, **Keys** (custom layouts), **View**, **Playing**, **MIDI** and **File**. In Play mode it shows only Layouts, View and File.

**Moving around the keyboard:** the scroll wheel scrolls (sideways, when the whole keyboard fits top to bottom), Shift-wheel scrolls sideways, and Cmd- or Ctrl-wheel or a pinch zooms. Cmd-= and Cmd-- zoom too, and Cmd-0 goes back to 100%. Under **View** in Settings, **Reset View** puts it back, and **−** and **+** zoom. While the keyboard is locked, none of these move it.
:::

::: {.callout .callout-note .card}
## Edit and Play {#modes}
KeyboardEDO has two modes, switched with the button beside the layout menu. It reads **Edit** in Play mode and **Editing** in Edit mode, and KeyboardEDO remembers which you were in.
- **Edit** shows everything: Tuning, Layout, Playing and MIDI settings, managing the document's layouts, and the layout editor for custom layouts. A knob's name in the performance bar becomes a button that chooses what it sends. Use it to set up a document's layouts.
- **Play** hides the setup, leaving the layouts to choose from, the view, the file buttons, and the performance bar (the arpeggiator and scale included). Use it when performing, so nothing gets changed by accident.

In both modes you choose which layout is showing from the toolbar's layout menu, or the list under **Layouts** in Settings.
:::

::: {.callout .callout-note .card}
## Documents {#documents}
A KeyboardEDO document (`.kedo`) holds **several layouts**, each with its own tuning, layout, playing settings, MIDI settings and arpeggiator. It also remembers where notes go, the performance bar's knob assignments and scale, and the notes held when it was saved (latched, or kept by the arpeggiator's Hold), in the order played.
- **Layouts** in Settings (Edit mode): **Add** a new layout after the current one, **Duplicate** it, **Remove** it, rename it (**Name**), or move it **earlier** or **later** in the list with the arrows.
- **File** in Settings: **New**, **Open...**, **Save**, and **Save As...**. Below them is where the document is saved, or "Not saved yet."
- On the Mac a `.kedo` document is one file. On Linux it's a folder, and Open chooses it as a folder.
- Dropping a document on the window opens it.
- The window's title shows the document's name, and **(edited)** when it has unsaved changes.
- KeyboardEDO starts where it left off: the document you had when you quit, saved or not, with the notes you were holding. **New** starts an untitled one.
:::

::: {.callout .callout-note .card}
## Tuning {#tuning}
Under **Tuning** in Settings (Edit mode):
- **EDO steps**: how many equal steps divide the octave, from 5 to 72.
- **Origin note** and **Origin octave** (−1 to 7): the note the tuning is built from, A0 to start with. Its frequency is shown below, from A4 = 440 Hz: A0 is 27.50 Hz.
- **Columns**: for the diamond piano only, which piano key's column each step goes in: **Semitone below** (the 12-EDO note at or below it) or **Nearest semitone**.

Every key's pitch is a number of EDO steps from the origin. At 12-EDO every MIDI note plays. At any other EDO, keys play from the origin up to C8, at most 128 steps, each with its own MIDI 2.0 note number; keys outside that range are drawn grey and don't sound. A lower origin gives a wider range.
:::

::: {.callout .callout-note .card}
## Layouts {#layouts}
Under **Layout** in Settings (Edit mode), choose **Piano**, **Tiled** or **Custom**.
- **Piano** is a piano keyboard at 12-EDO, and the **diamond piano** at any other EDO.
- **Tiled** is a grid of diamond keys. With every generator but Octave columns it's isomorphic: every interval has the same shape wherever you play it.
- **Custom** is a layout of keys placed one by one, made by converting another layout or by adding keys yourself.

**Convert to custom** turns the current layout into a custom one, with every key keeping its place, look, pitch, note number and computer key, so it plays just as it did and you can start from a layout that's nearly right.
:::

::: {.callout .callout-note .card}
## Piano and Diamond Piano {#piano}
At **12-EDO**, the Piano layout is a standard piano keyboard.

At any **other EDO** it becomes the **diamond piano**: each piano key's steps are stacked as diamonds, so the white and black keys keep their familiar places, and the extra steps of the EDO sit between them. **Columns** under Tuning chooses which piano key each step goes with.

Both can show a scale on their keys ([Scale](#scale)).
:::

::: {.callout .callout-note .card}
## Tiled Layouts {#tiled}
A tiled layout is a grid of diamond keys. With every generator but Octave columns, moving one key **right**, **up-right**, or **up-left** always moves by the same number of EDO steps, so a chord or scale has the same shape everywhere. Its lowest key, the origin note in octave −1, is at the bottom left, and it has as many rows as fit the window.

**Generator** chooses those steps. Each is worked out for the current EDO, and only those that work there are offered:
- **Wicki-Hayden**: right is a whole tone, up-right a fifth.
- **Bosanquet**: right is one step, up-right a fifth.
- **Janko**: right is a whole tone, up-right one step.
- **Harmonic Table**: up-right is a major third and up-left a minor third, so two rows up is a fifth.
- **Guitar (fourths)**: right is a whole tone, up-right a fourth.
- **Default**: right is one step, up-right roughly a fifth. Offered only when none of the above work.
- **Octave columns**: each column holds one note in every octave, two octaves across, with a chromatic zigzag along the bottom. It isn't isomorphic: a chord's shape changes from one column to the next.

The steps for the chosen generator are shown under it. **Tile colour** colours the keys by **Note** (one colour per note name), by **Octave**, or not at all (**Off**).
:::

::: {.callout .callout-note .card}
## Custom Layouts {#custom}
A custom layout is a set of diamond keys, each with its own place, pitch, note number and look. Edit one in **Edit** mode, under **Keys** in Settings. While you edit, a click selects a key instead of playing it, and each change sounds the key briefly.
- **Select**: click a key; Shift-click to add or remove one; click empty space to select none; Cmd-A selects all.
- **Move**: drag the selected keys, or use the arrow keys (Shift moves four times as far). **Snap to grid** keeps keys aligned; **Half steps** snaps to half the grid, between keys.
- **Add key**: adds a key in the middle of the view, or double-click empty space. It's a step above the selected key, if there is one.
- **Duplicate** (Cmd-D) and **Delete** (Delete or Backspace) the selected keys.
- **Undo** (Cmd-Z) and **Redo** (Shift-Cmd-Z or Cmd-Y).

The inspector edits the selected keys:
- **Pitch**: one key's **EDO**, **Octave**, and **Degree** (steps above the origin note), shown as a note name, cents and Hz. With several keys selected, **− step**, **+ step**, **− octave** and **+ octave** transpose them all. Each key keeps its own EDO, so changing **EDO steps** under Tuning doesn't change the keys; it's the EDO new keys get.
- **Note number**: suggested (the nearest free number for the key's pitch) until you type one; **Suggest** goes back to that. A number shared with a key of another pitch is warned of, as they'd sound as one note.
- **Computer key**: click **Assign...**, then press a letter, digit or punctuation key (Escape cancels); **Clear** removes it. Each computer key plays one key.
- **Group** and **Channel**: the layout's (under MIDI), or the key's own.
- **Look**: **Shade** (white or black), **Own colour**, **Width** and **Height** (50% to 200%), and what its **Label** and the line **Under it** show: text you type, **Name and octave**, **Name**, **Degree**, or **Frequency**.
:::

::: {.callout .callout-note .card}
## Computer Keys {#computer-keys}
Turn on **Computer keys** under View to play from your computer's keyboard. Each key shows the computer key that plays it, named as your keyboard layout names it. Piano and tiled layouts come with a mapping, and converting to custom keeps it; in a custom layout you assign each key yourself.
- **Shift** latches the notes you play, as the Latch button does.
- **Ctrl** with the mouse sends expression for the notes your computer keys are holding: move the mouse as you would drag a key ([Expression](#expression)).
- **Escape** releases every note.
- With Cmd or Ctrl held, a key is a shortcut, not a note.
:::

::: {.callout .callout-note .card}
## Playing {#playing}
Under **Playing** in Settings (Edit mode):
- **Drag** sets what happens when you drag across keys:
  - **Gliss**: each key you pass over plays, as a glissando. The drag stops at the keyboard's edges, so keys hidden under the side bars don't sound.
  - **Fixed**: the note you struck stays; dragging sends **expression** instead.
- **Glide (Pitch 7.25)**, with Gliss: instead of striking each key, one note's pitch moves from key to key (MIDI 2.0). It lands on each key's exact pitch rather than sliding between them.
- **Horizontal** and **Vertical**: the expression a drag sends ([Expression](#expression)).
- **Strike velocity**: a key plays louder toward its top and softer toward its bottom, down to the **Minimum** you set (1 to 126). Off, every note plays at full velocity.

Two fingers on two keys that play the same pitch hold one note, which ends when the last finger lets go.
:::

::: {.callout .callout-note .card}
## Expression {#expression}
With **Fixed** drag, moving a finger or the mouse after striking a key sends expression for that note. In either drag mode, so does moving the mouse with Ctrl held while computer keys hold notes. **Horizontal** and **Vertical** choose what each direction sends:
- **Horizontal**: Off or **Pitch Bend**.
- **Vertical**: Off, **Controller** (choose its number), **Pressure**, or **Pitch Bend**.

Each has a **scope**:
- **Bend scope** and **Controller scope**: **Per note** bends or changes only the finger's own note (MIDI 2.0 per-note pitch bend and per-note controllers); **Channel** uses ordinary channel messages, driven by the first finger down.
- **Pressure scope**: **Poly** is each finger's own note (polyphonic key pressure); **Channel** is channel pressure from the first finger.

When **Expression as MIDI 1.0** is on (under MIDI), pitch bend and controllers are channel-wide, as MIDI 1.0 has no per-note form for them, and their scopes are set to Channel. Poly pressure is MIDI 1.0 too, so its scope still applies.
:::

::: {.callout .callout-note .card}
## Performance Bar {#performance}
The bar on the left, shown and hidden with the toolbar's Performance bar button, works in both modes. Its sections, **Wheels**, **Knobs**, **Arpeggiator** and **Scale**, fold by their titles, and stay as you leave them.

The wheels and knobs act on the whole channel, like a keyboard's, and are the same for every layout:
- **Bend**: a pitch-bend fader over the bend range set under MIDI. It springs back to the centre when you let go.
- **Mod**: the mod wheel (CC 1).
- **Four knobs**, by default **Timbre** (CC 74), **Pressure** (channel pressure), **CC 71** and **CC 7**. Drag up or down to turn one (Cmd or Ctrl for finer); double-click to set it back. To choose what one sends, right-click it, or in Edit mode click its name: **Sends** a controller (with its **CC** number) or channel pressure, with your own **Label**; **Default** puts it back.

Click the value under Mod or a knob to type one (0 to 127).

The knob assignments are saved with the document. Their positions aren't: nothing is sent until you move a knob, so opening a document never changes your synth's settings.
:::

::: {.callout .callout-note .card}
## Arpeggiator {#arpeggiator}
The arpeggiator steps through the notes you hold, one at a time, and puts a pink dot on the key it's playing. Its controls are in the performance bar, in both modes; **On**, or the toolbar's Arpeggiator button, turns it on and off.
- **Pattern**: Up, Down, Up-Down, Down-Up, As Played, or Random.
- **Division**: a step's length, from a whole note (1/1) to 1/32, with triplets (1/2T to 1/16T).
- **Tempo**: 1 to 999 beats per minute.
- **Gate**: how much of its step each note sounds, 1% to 100%.
- **Octaves**: plays the pattern, then again an octave higher for each octave more (1 to 4).
- **Swing**: every second step comes late; how much of a pair of steps the first takes, from 50% (straight) to 75%. 67% is a triplet feel.
- **Slop**: each step early or late at random, by up to 0 to 50 ms (never more than a quarter of a step).
- **Velocity**: **As played**, **Fixed** at a velocity you set with the knob beside it, or **Random**.
- **Hold**: released notes keep playing, and new ones add to them; click one again to let it go. Turning the arpeggiator off keeps them, silent, to play again when it's back on, and they're still there the next time KeyboardEDO starts. **Clear** lets go of them, and of notes latched with Shift or Latch; so do All Notes Off, Escape, and turning Hold off.

As with the knobs, double-click a knob to set it back, and click its value to type one. Each layout has its own arpeggiator settings.
:::

::: {.callout .callout-note .card}
## Scale {#scale}
The **Scale** section of the performance bar tints the keys of a familiar scale, its key note stronger. The scale's key is the tuning's origin note, so with an A origin, **Dorian** is A Dorian.
- **Scale**: **None**, or one of the modes of the major, melodic minor, harmonic minor and harmonic major scales, the major and minor pentatonics, the whole-tone scale, the three diminished scales (whole-half, half-whole, and from the 2nd, which leaves out the key note, so no key shows stronger), or the blues scale.
- **Map**: how the scale's notes find steps of the EDO: **By fifths** (stacking the EDO's own fifth) or **Nearest** (the step nearest each 12-EDO note). At 12-EDO both give the familiar scale.

It shows on the Piano layout (the standard piano and the diamond piano) only. The scale is saved with the document.
:::

::: {.callout .callout-note .card}
## MIDI Output {#midi}
**Where notes go** is set from the antenna button:
- **Send to**: **None**, or a destination (a synth, a DAW's input, or a hardware port). The menu shows whether each takes MIDI 2.0 or 1.0, and KeyboardEDO sends what it takes. The destination is saved with the document; one that isn't there now shows as **(not found)**, and is opened when it appears.
- **Publish as "KeyboardEDO"**: offers a MIDI source other apps can choose as an input. It keeps the same identity from one launch to the next, and the setting is remembered. On macOS 15 and later it's a MIDI 2.0 (UMP) endpoint that identifies itself to apps that ask; on Linux it's an ALSA sequencer port.

Send to and Publish can both be on: notes go to each.

Under **MIDI** in Settings (Edit mode):
- **Group** and **Channel** (1 to 16): where this layout's notes are sent. A custom layout's keys can have their own.
- **Bend range**: the channel pitch-bend range in semitones (0 to 96, 2 to start with), sent to the receiver when notes start going to it.
- **Per-note bend**: the range of per-note pitch bend (MIDI 2.0), likewise.
- **Expression as MIDI 1.0**: sends pitch bend, controllers and pressure as MIDI 1.0 messages, even to a MIDI 2.0 receiver; notes stay MIDI 2.0, with exact pitch. Logic Pro and some other DAWs mishandle MIDI 2.0 bend and controllers, so this is on by default. Turn it off for receivers that handle MIDI 2.0 fully, such as StageHand and Umpire, to get 32-bit and per-note expression.
- **Per-Note Management**: resets a note's per-note bend and controllers when it's struck again (MIDI 2.0). On by default.
:::

<details class="disclosure">
<summary class="disclosure-title subtle">MIDI 2.0, MIDI 1.0, and microtonal pitch</summary>

::: disclosure-content
KeyboardEDO sends each note's exact pitch with MIDI 2.0's **Pitch 7.9** note attribute, so a receiver that honours it plays every EDO step exactly, with nothing else to set up.

**MIDI 1.0 has no way to give one note its own exact pitch.** Sent to a MIDI 1.0 receiver, notes go to the nearest 12-EDO note, a glide strikes each key instead, and per-note expression becomes channel-wide. For microtonal playing, use a MIDI 2.0 receiver.

KeyboardEDO doesn't send **MPE**. MPE is a MIDI 1.0 workaround for per-note expression, and MIDI 2.0 does the same job directly. Synths that accept MPE, such as Unlikelyware's, also accept MIDI 2.0's per-note messages.
:::

</details>

::: {.callout .callout-note .card}
## Shortcuts {#shortcuts}

| Key | Action |
|---|---|
| Cmd-N | New document |
| Cmd-O | Open a document |
| Cmd-S | Save (Shift-Cmd-S: Save As) |
| Cmd-= and Cmd-- | Zoom in and out |
| Cmd-0 | Zoom back to 100% |
| F1 or Cmd-? | This help page |
| Escape | Release every note, and what the arpeggiator holds |
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
| Escape | Clear the selection (and release every note) |
:::

::: {.callout .callout-note .card}
## Troubleshooting {#troubleshooting}
- **No sound.** Check the badge: **not sending** means no destination is chosen and Publish is off (the antenna button). If you publish, make sure the receiving app has KeyboardEDO selected as an input.
- **The destination shows "(not found)".** Its device is unplugged or its app isn't running. KeyboardEDO opens it as soon as it appears.
- **An app doesn't see "KeyboardEDO".** An app that was already running when KeyboardEDO started publishing may need to rescan its MIDI devices. In REAPER: Preferences > MIDI Inputs > Reset all MIDI devices.
- **The badge reads "no MIDI output".** The system's MIDI couldn't be reached (on Linux: no `/dev/snd/seq`, or a kernel or alsa-lib without MIDI 2.0 support). The monitor still shows what would be sent.
- **Pitch bend or controllers misbehave in a DAW.** Turn on **Expression as MIDI 1.0** under MIDI.
- **Notes stuck on.** Press **Panic**, or Escape.
- **Microtones play as 12-EDO notes.** The receiver is getting MIDI 1.0; see the MIDI 2.0 note under [MIDI Output](#midi).
- **Keys are grey and don't sound.** They're outside the tuning's range: at EDOs other than 12, keys play from the origin up to C8 ([Tuning](#tuning)).
:::
