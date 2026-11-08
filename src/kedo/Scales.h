#pragma once

#include <string>
#include <vector>

#include "kedo/Tuning.h"

// Familiar scales, and which steps of an EDO they fall on: the performance
// bar's scale, shown on the piano's keys, rectangles or diamonds (decisions.md,
// "Scale highlight"). The scales and the mapping are NoteGran Chords' (its
// core/Scales), so a scale looks the same in both.
//
// A familiar scale is stored spelled: each degree as its place on the chain of
// fifths from the key (C 0, G 1, D 2, ... F -1, Bb -2, ...), so F# (6) and Gb
// (-6) are different degrees. A place p is (7p mod 12) semitones above the key
// in 12-EDO, and in an N-EDO either:
//   By fifths: p * F mod N steps, F the EDO's own fifth (its nearest to 702
//              cents). Never ties.
//   Nearest:   the step nearest its 12-EDO cents; a tie goes up for a
//              sharp-side place (p > 0), down for a flat-side one (p < 0).
// The scale's key is the tuning's origin note, step 0. At 12-EDO both
// mappings give the familiar scale.
namespace kedo {

struct FamiliarScale {
    const char* name;
    int count;
    int fifths[8];   // ascending in 12-EDO pitch from the key (or, if the key
                     // isn't in it, from the step above); `count` used
};

// The menu, in order.
const std::vector<FamiliarScale>& familiarScales();

enum class ScaleMap { Fifths, Nearest };

// The EDO's own fifth: its step count nearest 701.955 cents.
int edoFifth(int edo);

// What the performance bar shows: a scale by name ("" for none) and how it
// maps into the EDO; its key is the origin note. Saved with the document.
struct ScaleChoice {
    std::string scale;
    ScaleMap    map = ScaleMap::Nearest;

    bool operator==(const ScaleChoice&) const = default;
};

enum class ScaleMark { Off, Degree, Tonic };

// Each step's mark, by its step above the origin mod the EDO (so index
// mod(step, edo)); the origin is the Tonic when the scale holds its key, as
// all but Diminished (from the 2nd) do. Empty when no scale, or one not in
// familiarScales(), is chosen.
std::vector<ScaleMark> scaleMarks(const ScaleChoice& choice, const Tuning& tuning);

} // namespace kedo
