#pragma once

#include <cstdint>
#include <string>

#include "kedo/Note.h"

// An equal division of the octave and where it starts. Everything else in the
// keyboard (layouts, note numbers, UMP) works in *steps*: whole EDO steps
// counted up from the origin note, so step 0 is the origin.
namespace kedo {

// Pitch classes of 12-EDO, C = 0. Used for the origin note and for labels.
int pitchClassOf(const std::string& name);            // "A", "C#", "Bb" -> 0..11 (-1 if unknown)
const char* pitchClassName(int pc);                    // 0..11 -> "C", "C#", ...
bool isBlackPitchClass(int pc);                        // C#, D#, F#, G#, A#

// A label for one step: the nearest 12-EDO note and how far the step is from it.
struct StepLabel {
    int    pitchClass = 0;      // 0..11 of the nearest 12-EDO note
    int    octave     = 0;      // scientific octave (A4 = 440 Hz)
    double cents      = 0.0;    // deviation from that note, -50..+50
};

struct Tuning {
    static constexpr int kMinEdo = 5;
    static constexpr int kMaxEdo = 72;

    int edo            = 12;
    int originPitchClass = 9;    // A
    int originOctave   = 0;      // A0 = 27.5 Hz: in the piano's lowest octave

    // The origin as a 12-EDO MIDI note number (A1 = 33).
    int originMidiNote() const;
    double originFrequency() const;

    // Frequency of `step` steps above (or, at 12-EDO, below) the origin.
    double frequency(int step) const;

    // Pitch in 12-EDO semitones above MIDI note 0, fractional: the form both
    // Pitch 7.9 and Pitch 7.25 encode.
    double semitones(int step) const;

    // The playable steps. At 12-EDO every MIDI note is playable, so steps
    // below the origin are too. Otherwise steps run from the origin up to C8
    // (as KeyEDO), at most 128 of them, one MIDI 2.0 note number each.
    int firstStep() const;
    int lastStep() const;           // inclusive
    bool isPlayable(int step) const { return step >= firstStep() && step <= lastStep(); }

    // The lowest key the tile layouts show: the origin note in octave -1 (A-1
    // for an A origin), whatever the origin octave. At 12-EDO the keys below
    // it are never shown; at other EDOs, those from it up to the origin are
    // shown, but grey (they have no note number).
    int lowestStep() const { return -edo * (originOctave + 1); }

    // The MIDI 2.0 note number that identifies `step`. At 12-EDO it is the
    // standard note number; otherwise steps are numbered from 0 at the origin,
    // and the Pitch 7.9 attribute carries the real pitch (as KeyEDO).
    std::uint8_t noteNumber(int step) const;

    // The nearest 12-EDO note: the MIDI 1.0 fallback's note number.
    std::uint8_t nearestMidi1Note(int step) const;

    // What a key on `step` plays: id `step`, noteNumber(), semitones(), and
    // the output's group and channel.
    Note note(int step) const;

    StepLabel label(int step) const;

    bool operator==(const Tuning&) const = default;
};

// The label for a pitch in semitones above MIDI note 0.
StepLabel labelOfPitch(double semitones);

// Pitch encodings. `semitones` is a pitch above MIDI note 0 (8.18 Hz).
std::uint16_t encodePitch7_9(double semitones);     // note-on attribute
std::uint32_t encodePitch7_25(double semitones);    // registered per-note controller #3

} // namespace kedo
