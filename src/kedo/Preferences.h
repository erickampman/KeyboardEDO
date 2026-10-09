#pragma once

#include <string>

// App-level settings, remembered across launches and not part of any
// document: the routing a person sets up once for their machine, as KeyEDO
// keeps "publish" in UserDefaults (decisions.md, "Sending MIDI").
namespace kedo {

struct Preferences {
    // Offer the published "KeyboardEDO" source. Off until turned on once.
    bool publish = false;

    // The window's panes: the settings panel (open at first, to find things)
    // and the MIDI monitor.
    bool panelOpen   = true;
    bool monitorOpen = false;

    bool operator==(const Preferences&) const = default;
};

// From and to a small JSON file. A missing or unreadable file gives the
// defaults; unknown keys are ignored.
Preferences loadPreferences(const std::string& path);
bool savePreferences(const std::string& path, const Preferences& preferences);

} // namespace kedo
