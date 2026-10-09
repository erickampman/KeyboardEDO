#pragma once

#include <string>

#include "kedo/Document.h"

// KeyEDO's one-line synopsis of the setup, shown across the top so it can be
// read without opening the settings panel, e.g.
//   17-EDO · A1 · Tiled: Wicki-Hayden · Gliss · Strike Vel · Latch · to IAC Bus 1 · Published
namespace kedo {

// What isn't in the document: the moment's routing and modes.
struct LiveState {
    bool        latch = false;          // the Latch toggle
    std::string destination;            // the destination's display name, "" for none
    bool        destinationFound = true;
    bool        publishing = false;
};

std::string synopsis(const Document& document, const LiveState& live);

} // namespace kedo
