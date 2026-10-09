#pragma once

#include <cstdint>
#include <optional>

// What the keyboard plays, before it becomes MIDI. The Performer calls a
// NoteSink; UmpOutput turns the calls into UMP, and the arpeggiator (later)
// will sit between them as a NoteSink that calls another.
namespace kedo {

// A note is named by the step it was struck on (Tuning). With a Pitch 7.25
// glide it can move to another step's pitch and keep its name.
class NoteSink {
public:
    virtual ~NoteSink() = default;

    virtual void noteOn(int note, std::uint16_t velocity) = 0;
    virtual void noteOff(int note) = 0;

    // The sounding note `note` now has the pitch of step `step`, without
    // being struck again.
    virtual void glide(int note, int step) = 0;

    // Expression. `note` set: that note only (MIDI 2.0 per-note messages).
    // std::nullopt: the whole channel.
    virtual void pitchBend(std::optional<int> note, double value) = 0;              // -1..1
    virtual void controller(std::optional<int> note, int number, double value) = 0; // 0..1
    virtual void pressure(std::optional<int> note, double value) = 0;               // 0..1
};

} // namespace kedo
