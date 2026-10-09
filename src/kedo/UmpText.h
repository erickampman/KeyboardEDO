#pragma once

#include <string>

#include "uw/ump/MidiMessage.h"

// One UMP as a line of text, for the MIDI monitor: protocol, group and
// channel, then the message, e.g.
//   2.0 g1 ch1  Note On      n17  v 50.0%  pitch 45.000
namespace kedo {

std::string describeUmp(const uw::MidiMessage& m);

} // namespace kedo
