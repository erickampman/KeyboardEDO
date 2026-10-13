#pragma once

#include <string>
#include <vector>

// Generator presets for the Tiled layout (KeyEDO's EDOKeyboardTypes.swift).
namespace kedo {

// Two step intervals that define an isomorphic lattice.
//   right    : the tile to the right, sharing a corner (KeyEDO's g1)
//   upRight  : the tile up and to the right, sharing an edge (KeyEDO's g2)
// So the tile up and to the left is upRight - right. With gcd(right, upRight)
// == 1 every step is reachable.
//
// octaveColumns: not a lattice at all, but OctaveLayout (docs/decisions.md,
// "Octave columns"); it's listed with the presets so documents store it as
// KeyEDO's do, by index. Its right and upRight are what they are from the
// lowest columns: 2 and 1.
struct Generator {
    std::string name;
    int right   = 1;
    int upRight = 7;
    bool octaveColumns = false;

    int upLeft() const { return upRight - right; }
    bool operator==(const Generator&) const = default;
};

int gcd(int a, int b);

// The presets for `edo`, most idiomatic first. Never empty. New presets go at
// the end: documents store the index (KeyEDO's selectedPresetIndex).
std::vector<Generator> generatorPresets(int edo);

} // namespace kedo
