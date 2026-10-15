#pragma once

// Toolbar icons, drawn as vector shapes with ImGui's draw list, so they need
// no icon font and stay sharp at any scale. Modelled on the SF Symbols
// KeyEDO's toolbar uses.
namespace kedo::app {

enum class Icon {
    Open,        // folder
    Save,        // tray with a down arrow
    Routing,     // antenna: where the notes go
    Locked,      // padlock, closed
    Unlocked,    // padlock, open
    Latch,       // pin
    Arpeggiator, // notes climbing a stair
    Panic,       // stop square: All Notes Off
    Monitor,     // lines of text: the MIDI monitor
    Configure,   // sliders: the settings panel
};

// A square button showing `icon`, `size` points across, with `tooltip` on
// hover. `on` draws it highlighted (a toggle that's on). Returns true when
// clicked.
bool iconButton(const char* id, Icon icon, float size, const char* tooltip, bool on = false);

} // namespace kedo::app
