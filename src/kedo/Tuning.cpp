#include "kedo/Tuning.h"

#include <algorithm>
#include <cmath>

namespace kedo {

namespace {

constexpr const char* kNames[12] = {"C", "C#", "D", "D#", "E", "F",
                                    "F#", "G", "G#", "A", "A#", "B"};

// C8, KeyEDO's top note.
constexpr int kTopMidiNote = 108;

} // namespace

int pitchClassOf(const std::string& name) {
    struct Entry { const char* name; int pc; };
    static constexpr Entry kEntries[] = {
        {"C", 0},  {"C#", 1}, {"Db", 1},  {"D", 2},  {"D#", 3}, {"Eb", 3},
        {"E", 4},  {"F", 5},  {"F#", 6},  {"Gb", 6}, {"G", 7},  {"G#", 8},
        {"Ab", 8}, {"A", 9},  {"A#", 10}, {"Bb", 10}, {"B", 11},
    };
    for (const auto& e : kEntries)
        if (name == e.name) return e.pc;
    return -1;
}

const char* pitchClassName(int pc) { return kNames[((pc % 12) + 12) % 12]; }

bool isBlackPitchClass(int pc) {
    switch (((pc % 12) + 12) % 12) {
    case 1: case 3: case 6: case 8: case 10: return true;
    default: return false;
    }
}

int Tuning::originMidiNote() const { return (originOctave + 1) * 12 + originPitchClass; }

double Tuning::originFrequency() const {
    return 440.0 * std::pow(2.0, (originMidiNote() - 69) / 12.0);
}

double Tuning::frequency(int step) const {
    return originFrequency() * std::pow(2.0, double(step) / edo);
}

double Tuning::semitones(int step) const {
    return originMidiNote() + 12.0 * double(step) / edo;
}

int Tuning::firstStep() const { return edo == 12 ? -originMidiNote() : 0; }

int Tuning::lastStep() const {
    if (edo == 12) return 127 - originMidiNote();
    // Steps whose pitch is at or below C8, as KeyEDO's totalNotes.
    const double span = (kTopMidiNote - originMidiNote()) / 12.0;   // octaves
    const int steps = int(std::floor(edo * span + 1e-9)) + 1;
    return std::clamp(steps, 1, 128) - 1;
}

std::uint8_t Tuning::noteNumber(int step) const {
    const int n = edo == 12 ? originMidiNote() + step : step;
    return std::uint8_t(std::clamp(n, 0, 127));
}

std::uint8_t Tuning::nearestMidi1Note(int step) const {
    return std::uint8_t(std::clamp(int(std::lround(semitones(step))), 0, 127));
}

Note Tuning::note(int step) const {
    Note n;
    n.id = step;
    n.number = noteNumber(step);
    n.pitch = semitones(step);
    return n;
}

StepLabel Tuning::label(int step) const { return labelOfPitch(semitones(step)); }

StepLabel labelOfPitch(double st) {
    const int nearest = int(std::lround(st));
    StepLabel l;
    l.pitchClass = ((nearest % 12) + 12) % 12;
    l.octave = int(std::floor(nearest / 12.0)) - 1;
    l.cents = (st - nearest) * 100.0;
    return l;
}

std::uint16_t encodePitch7_9(double semitones) {
    if (!std::isfinite(semitones) || semitones <= 0.0) return 0;
    if (semitones >= 128.0) return 0xFFFF;
    const auto v = std::lround(semitones * 512.0);
    return std::uint16_t(std::min<long>(v, 0xFFFF));
}

std::uint32_t encodePitch7_25(double semitones) {
    if (!std::isfinite(semitones) || semitones <= 0.0) return 0;
    if (semitones >= 128.0) return 0xFFFFFFFFu;
    const auto v = std::llround(semitones * double(1u << 25));
    return std::uint32_t(std::min<long long>(v, 0xFFFFFFFFll));
}

} // namespace kedo
