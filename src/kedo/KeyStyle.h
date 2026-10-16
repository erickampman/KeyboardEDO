#pragma once

#include <string>

#include "kedo/Layout.h"

// How a key looks: colours and labels, as KeyEDO's views. Plain values, so the
// drawing layer only converts them to its own types.
namespace kedo {

// How Tiled colours its tiles (KeyEDO's TileColorMode).
enum class TileColor { Off, Note, Octave };

struct Rgb {
    float r = 0, g = 0, b = 0;
    bool operator==(const Rgb&) const = default;
};

struct KeyLook {
    Rgb fill;
    Rgb outline;
    Rgb ink;              // label colour
    std::string title;    // e.g. "C2", "A", "F#3"; empty for none
    std::string detail;   // smaller, under the title: frequency or scale degree
};

// The look of `key` from `kind`'s layout (Piano covers both the piano and the
// diamonds; the key's shape tells them apart).
KeyLook keyLook(const Key& key, const Tuning& tuning, LayoutKind kind, TileColor tileColor,
                bool sounding, bool latched);

// The background behind each keyboard: slate blue for the diamonds (between
// their black and white), a darker slate for the piano, dark grey for the
// tiles.
Rgb keyboardBackground(LayoutKind kind, const Tuning& tuning);

// The nearest 12-EDO note's name and octave, e.g. "F#3": a custom key's
// label from its pitch.
std::string noteNameOf(double pitch);

// HSB to RGB, h in 0..1.
Rgb hsb(double h, double s, double b);

} // namespace kedo
