#pragma once

#include <map>
#include <string>
#include <vector>

#include "kedo/Layout.h"

// Computer keys (decisions.md, "Computer keyboard"): the player assigns one to
// a note key, and generated layouts have a mapping of their own. A computer
// key is its physical place, its USB HID usage (which SDL's scancodes are),
// so a mapping keeps its shape on QWERTY, AZERTY or Dvorak; 0 is none.
namespace kedo {

// The four rows of keys that can be assigned, from the bottom (0: Z X C ...
// /) to the top (3: 1 2 3 ... =), left to right. Each row starts a little
// left of the one below, so (row, i) has (row + 1, i) up-left of it and
// (row + 1, i + 1) up-right.
const std::vector<int>& computerKeyRow(int row);

bool isAssignableComputerKey(int key);

// The W3C `code` name ("KeyQ", "Digit2", "BracketLeft"), as files store a
// computer key; "" for one that can't be assigned. And back: 0 if unknown.
std::string computerKeyCode(int key);
int computerKeyFromCode(const std::string& code);

// What a US keyboard has printed on it ("Q", "2", "["). The app shows what the
// system's own keyboard layout gives instead.
std::string computerKeyName(int key);

// The default mappings, from the origin note in octave 3:
//
// The 12-EDO piano's, by MIDI note: the trackers' two rows, Z X C ... white
// from C3 and S D G H J black, Q W E ... ] white from C4 and 2 3 5 ... =
// black. 0 for notes it doesn't reach.
int pianoComputerKey(int midiNote);

// The diamond piano's and Octave columns': consecutive steps zigzag across a
// pair of rows, Z S X D ... / ' (20), then on across Q 2 W 3 ... = ] (23).
const std::vector<int>& computerKeyZigzag();

// The playable keys of `layout` that have a computer key, by computer key.
std::map<int, Key> computerKeyMap(const Layout& layout);

} // namespace kedo
