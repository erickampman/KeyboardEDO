#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

// What a key plays: everything the output needs to send it, so nothing past
// the layout depends on how the key got its note (a Tuning step, or the
// custom layout's own settings; decisions.md, "Keys carry the note they play").
namespace kedo {

struct Note {
    // Names the note while it sounds: the Performer holds notes by it, and
    // NoteSink calls after noteOn() refer to it. Unique within a layout for
    // each distinct note; keys that play the same note share it (Tiled's
    // repeated steps). Tuning-based layouts use the step.
    int id = 0;

    std::uint8_t number = 60;            // the MIDI 2.0 note number
    std::optional<std::uint8_t> group;   // std::nullopt: the output's
    std::optional<std::uint8_t> channel;

    // Pitch in 12-EDO semitones above MIDI note 0 (8.18 Hz), as Pitch 7.9 and
    // 7.25 encode it. A MIDI 2.0 note-on carries it as a Pitch 7.9 attribute
    // unless it equals `number` (a plain note: 12-EDO, standard numbering).
    double pitch = 60.0;

    bool needsPitchAttribute() const { return std::abs(pitch - number) > 1e-9; }

    // The MIDI 1.0 fallback's note number: the nearest 12-EDO note.
    std::uint8_t nearestMidi1Note() const {
        const long n = std::lround(pitch);
        return std::uint8_t(n < 0 ? 0 : n > 127 ? 127 : n);
    }

    bool operator==(const Note&) const = default;
};

} // namespace kedo
