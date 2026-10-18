#pragma once

#include <optional>
#include <string>
#include <vector>

#include "kedo/NoteSink.h"

// The performance bar's controls (decisions.md, "Performance bar"): a
// spring-return pitch-bend fader, a mod wheel and four assignable knobs, all
// channel-wide. The app draws them; this keeps where each one is and sends
// what moves, so a control sends nothing until it's moved.
namespace kedo {

// What a knob sends: a channel CC, or channel pressure.
struct KnobAssignment {
    bool        pressure   = false;
    int         controller = 74;    // 0..127, unless pressure
    std::string label{};            // "" shows the CC's number

    bool operator==(const KnobAssignment&) const = default;
};

inline constexpr int kKnobCount = 4;

// CC 74 (Timbre), channel pressure, CC 71 (resonance) and CC 7 (volume): the
// first two drive IFSynth's and PlainOldSynth's Timbre and Pressure sources.
std::vector<KnobAssignment> defaultKnobs();

// What a knob is called: its label, else "CC 74" or "Pressure".
std::string knobName(const KnobAssignment& knob);

// Where a knob starts, 0..1, before it's moved: volume at GM's 100, pressure
// and the mod wheel at none, other controllers centred (64).
double knobStart(const KnobAssignment& knob);

class PerformanceControls {
public:
    explicit PerformanceControls(NoteSink& sink);

    // A knob whose assignment changes goes back to its start, sending nothing.
    // Fewer than kKnobCount: the rest are the defaults'.
    void setKnobs(const std::vector<KnobAssignment>& knobs);
    const std::vector<KnobAssignment>& knobs() const { return knobs_; }

    // Each set sends the new value if it differs from the last one.
    double bend() const { return bend_; }      // -1..1, 0 at rest
    void   setBend(double value);
    double wheel() const { return wheel_; }    // 0..1, CC 1
    void   setWheel(double value);
    double knob(int index) const;              // 0..1
    void   setKnob(int index, double value);

private:
    NoteSink& sink_;
    std::vector<KnobAssignment> knobs_;
    std::vector<double> values_;
    double bend_ = 0, wheel_ = 0;
};

} // namespace kedo
