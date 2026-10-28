#pragma once

#include <cstdint>
#include <optional>

#include "kedo/Note.h"

// What the keyboard plays, before it becomes MIDI. The Performer calls a
// NoteSink; UmpOutput turns the calls into UMP, and the Arpeggiator sits
// between them as a NoteSink that calls another.
namespace kedo {

// noteOn() gives the whole Note; later calls name it by Note::id.
class NoteSink {
public:
    virtual ~NoteSink() = default;

    virtual void noteOn(const Note& note, std::uint16_t velocity) = 0;
    virtual void noteOff(int id) = 0;
    // A note let go on purpose (a latched note struck again): ended even
    // where a release would be kept, as the arpeggiator's Hold keeps one.
    virtual void letGo(int id) { noteOff(id); }

    // The sounding note `id` now has pitch `pitch` (semitones above MIDI
    // note 0), without being struck again.
    virtual void glide(int id, double pitch) = 0;

    // Expression. `id` set: that note only (MIDI 2.0 per-note messages).
    // std::nullopt: the whole channel.
    virtual void pitchBend(std::optional<int> id, double value) = 0;              // -1..1
    virtual void controller(std::optional<int> id, int number, double value) = 0; // 0..1
    virtual void pressure(std::optional<int> id, double value) = 0;               // 0..1
};

} // namespace kedo
