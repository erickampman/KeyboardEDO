#pragma once

#include <string>

#include "kedo/Layout.h"
#include "kedo/Scales.h"

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
    float opacity = 1;    // under 1 for keys with no note, so the background shows through
};

// The look of `key` from `kind`'s layout (Piano covers both the piano and the
// diamonds; the key's shape tells them apart). `scale`: its place in the
// performance bar's scale, tinting the piano's keys (only those, for now:
// decisions.md, "Scale highlight").
KeyLook keyLook(const Key& key, const Tuning& tuning, LayoutKind kind, TileColor tileColor,
                bool sounding, bool latched, ScaleMark scale = ScaleMark::Off);

// The background behind each keyboard: slate blue for the diamonds and the
// tiles (between black and white keys), a darker slate for the piano, darker
// still for Custom.
Rgb keyboardBackground(LayoutKind kind, const Tuning& tuning);

// The nearest 12-EDO note's name and octave, e.g. "F#3": a custom key's
// label from its pitch.
std::string noteNameOf(double pitch);

// HSB to RGB, h in 0..1.
Rgb hsb(double h, double s, double b);

} // namespace kedo
