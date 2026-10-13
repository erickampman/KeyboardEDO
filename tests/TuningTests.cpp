// Tuning and generator presets.

#include "Check.h"
#include "kedo/Generators.h"
#include "kedo/Tuning.h"

using namespace kedo;
using kedo::test::near;

namespace {

void testPitchClasses() {
    CHECK(pitchClassOf("C") == 0);
    CHECK(pitchClassOf("A") == 9);
    CHECK(pitchClassOf("Bb") == 10);
    CHECK(pitchClassOf("H") == -1);
    CHECK(std::string(pitchClassName(13)) == "C#");
    CHECK(isBlackPitchClass(6));
    CHECK(!isBlackPitchClass(4));
}

void testTwelve() {
    Tuning t;   // 12-EDO from A1
    t.originOctave = 1;   // A1: these numbers assume it
    CHECK(t.originMidiNote() == 33);
    CHECK(near(t.originFrequency(), 55.0));
    CHECK(near(t.frequency(36), 440.0));
    // Every MIDI note is playable at 12-EDO, below the origin too.
    CHECK(t.firstStep() == -33);
    CHECK(t.lastStep() == 94);
    CHECK(t.noteNumber(36) == 69);
    CHECK(t.noteNumber(-33) == 0);
    CHECK(t.nearestMidi1Note(36) == 69);
}

void testDefaultsAndLowest() {
    Tuning t;   // A0, the piano's lowest key
    CHECK(t.originPitchClass == 9 && t.originOctave == 0);
    CHECK(t.originMidiNote() == 21);
    // The tile layouts' lowest key: the origin note in octave -1.
    CHECK(t.lowestStep() == -12);
    CHECK(t.noteNumber(t.lowestStep()) == 9);   // A-1
    t.originPitchClass = 0;                     // C0: C-1, note 0
    CHECK(t.noteNumber(t.lowestStep()) == 0);
    t.edo = 31;
    t.originOctave = 2;
    CHECK(t.lowestStep() == -93);                // three octaves below C2
    CHECK(!t.isPlayable(t.lowestStep()));        // below the origin: no note number
}

void testSeventeen() {
    Tuning t;
    t.originOctave = 1;   // A1: these numbers assume it
    t.edo = 17;   // KeyEDO's screenshot: 17-EDO from A1
    CHECK(near(t.frequency(17), 110.0));
    CHECK(near(t.frequency(34), 220.0));
    CHECK(t.firstStep() == 0);
    // A1 to C8 is 75 semitones, 6.25 octaves: 106 steps, plus the origin.
    CHECK(t.lastStep() == 106);
    CHECK(t.noteNumber(0) == 0);
    CHECK(t.noteNumber(17) == 17);
    // One 17-EDO step is 70.6 cents: nearest 12-EDO note is A#1, 29.4 cents flat.
    const auto l = t.label(1);
    CHECK(l.pitchClass == 10);
    CHECK(l.octave == 1);
    CHECK(near(l.cents, 1200.0 / 17 - 100, 1e-9));
    CHECK(t.nearestMidi1Note(1) == 34);
}

void testWideEdos() {
    Tuning t;
    t.originOctave = 1;   // A1: these numbers assume it
    t.edo = 72;
    t.originOctave = 0;   // A0: 87 semitones to C8 would be 522 steps
    CHECK(t.lastStep() == 127);   // at most 128 note numbers
}

void testPitchEncodings() {
    CHECK(encodePitch7_9(69.0) == (69 << 9));
    CHECK(encodePitch7_9(69.5) == ((69 << 9) | 256));
    CHECK(encodePitch7_9(-1.0) == 0);
    CHECK(encodePitch7_9(200.0) == 0xFFFF);
    CHECK(encodePitch7_25(60.0) == (60u << 25));
    CHECK(encodePitch7_25(60.25) == ((60u << 25) | (1u << 23)));
}

void testGenerators() {
    const auto twelve = generatorPresets(12);
    CHECK(!twelve.empty());
    CHECK(twelve[0].name == "Wicki-Hayden");
    CHECK(twelve[0].right == 2);
    CHECK(twelve[0].upRight == 7);
    CHECK(twelve[0].upLeft() == 5);
    for (const auto& g : twelve) {
        if (g.name == "Harmonic Table") {
            CHECK(g.upRight == 4);
            CHECK(g.upLeft() == 3);
        }
        if (g.name == "Janko") {
            CHECK(g.right == 2);
            CHECK(g.upRight == 1);
        }
    }
    const auto seventeen = generatorPresets(17);
    CHECK(seventeen[0].name == "Wicki-Hayden");
    CHECK(seventeen[0].right == 3);
    CHECK(seventeen[0].upRight == 10);
    // Every preset reaches every step.
    for (int edo = Tuning::kMinEdo; edo <= Tuning::kMaxEdo; ++edo) {
        const auto presets = generatorPresets(edo);
        CHECK(!presets.empty());
        for (const auto& g : presets) CHECK(gcd(g.right, g.upRight) == 1);
    }
}

} // namespace

int main() {
    testPitchClasses();
    testTwelve();
    testDefaultsAndLowest();
    testSeventeen();
    testWideEdos();
    testPitchEncodings();
    testGenerators();
    return kedo::test::finish("Tuning");
}
