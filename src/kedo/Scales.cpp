#include "kedo/Scales.h"

#include <cstdint>

namespace kedo {

namespace {

int mod(std::int64_t a, std::int64_t n) { return int(((a % n) + n) % n); }

int semitonesOf(int fifths) { return mod(std::int64_t{fifths} * 7, 12); }

// The step nearest `cents` (0..1199) of an N-EDO; a tie up if `up`.
int nearestStep(std::int64_t cents, int edo, bool up) {
    const std::int64_t twice = 2 * cents * edo;   // 2 x the exact step, over 1200
    const std::int64_t whole = twice / 2400;
    const std::int64_t rest  = twice % 2400;
    return int(rest > 1200 || (rest == 1200 && up) ? whole + 1 : whole);
}

// A place on the chain of fifths as steps above the key, 0..edo - 1.
int stepsOf(int fifths, int edo, ScaleMap map) {
    if (map == ScaleMap::Fifths) return mod(std::int64_t{fifths} * edoFifth(edo), edo);
    return mod(nearestStep(std::int64_t{semitonesOf(fifths)} * 100, edo, fifths > 0), edo);
}

} // namespace

const std::vector<FamiliarScale>& familiarScales() {
    // C 0, D 2, E 4, F -1, G 1, A 3, B 5; a sharp adds 7, a flat takes 7 away.
    static const std::vector<FamiliarScale> scales = {
        {"Major (Ionian)", 7, {0, 2, 4, -1, 1, 3, 5}},
        {"Dorian", 7, {0, 2, -3, -1, 1, 3, -2}},
        {"Phrygian", 7, {0, -5, -3, -1, 1, -4, -2}},
        {"Lydian", 7, {0, 2, 4, 6, 1, 3, 5}},
        {"Mixolydian", 7, {0, 2, 4, -1, 1, 3, -2}},
        {"Minor (Aeolian)", 7, {0, 2, -3, -1, 1, -4, -2}},
        {"Locrian", 7, {0, -5, -3, -1, -6, -4, -2}},
        {"Melodic minor", 7, {0, 2, -3, -1, 1, 3, 5}},
        {"Dorian b2", 7, {0, -5, -3, -1, 1, 3, -2}},
        {"Lydian augmented", 7, {0, 2, 4, 6, 8, 3, 5}},
        {"Lydian dominant", 7, {0, 2, 4, 6, 1, 3, -2}},
        {"Mixolydian b6", 7, {0, 2, 4, -1, 1, -4, -2}},
        {"Locrian #2", 7, {0, 2, -3, -1, -6, -4, -2}},
        {"Altered", 7, {0, -5, -3, -8, -6, -4, -2}},
        {"Harmonic minor", 7, {0, 2, -3, -1, 1, -4, 5}},
        {"Locrian #6", 7, {0, -5, -3, -1, -6, 3, -2}},
        {"Ionian #5", 7, {0, 2, 4, -1, 8, 3, 5}},
        {"Dorian #4", 7, {0, 2, -3, 6, 1, 3, -2}},
        {"Phrygian dominant", 7, {0, -5, 4, -1, 1, -4, -2}},
        {"Lydian #2", 7, {0, 9, 4, 6, 1, 3, 5}},
        {"Ultralocrian", 7, {0, -5, -3, -8, -6, -4, -9}},
        {"Harmonic major", 7, {0, 2, 4, -1, 1, -4, 5}},
        {"Major pentatonic", 5, {0, 2, 4, 1, 3}},
        {"Minor pentatonic", 5, {0, -3, -1, 1, -2}},
        {"Whole tone", 6, {0, 2, 4, 6, 8, 10}},
        // The diminished scale's three sets, a half step apart; the third
        // leaves out the key.
        {"Diminished (whole-half)", 8, {0, 2, -3, -1, -6, -4, 3, 5}},
        {"Diminished (half-whole)", 8, {0, -5, -3, 4, 6, 1, 3, -2}},
        {"Diminished (from the 2nd)", 8, {-5, 2, 4, -1, 1, -4, -2, 5}},
        {"Blues", 6, {0, -3, -1, -6, 1, -2}},
    };
    return scales;
}

int edoFifth(int edo) {
    // round(edo x 701.955 / 1200), in integers.
    return int((std::int64_t{edo} * 701955 + 600000) / 1200000);
}

std::vector<ScaleMark> scaleMarks(const ScaleChoice& choice, const Tuning& tuning) {
    const FamiliarScale* scale = nullptr;
    for (const auto& s : familiarScales())
        if (choice.scale == s.name) scale = &s;
    if (!scale || tuning.edo < 1) return {};

    const int edo = tuning.edo;
    std::vector<ScaleMark> marks(std::size_t(edo), ScaleMark::Off);
    for (int i = 0; i < scale->count; ++i) {
        const int place = scale->fifths[i];
        // The key (place 0) is the Tonic; a scale without it, as Diminished
        // (from the 2nd), marks no Tonic.
        ScaleMark& m = marks[std::size_t(stepsOf(place, edo, choice.map))];
        if (place == 0) m = ScaleMark::Tonic;
        else if (m == ScaleMark::Off) m = ScaleMark::Degree;
    }
    return marks;
}

} // namespace kedo
