#include "kedo/Controls.h"

#include <algorithm>

namespace kedo {

std::vector<KnobAssignment> defaultKnobs() {
    return {{.controller = 74, .label = "Timbre"},
            {.pressure = true, .controller = 0, .label = "Pressure"},
            {.controller = 71, .label = ""},   // named by number: "CC 71"
            {.controller = 7, .label = ""}};
}

std::string knobName(const KnobAssignment& knob) {
    if (!knob.label.empty()) return knob.label;
    return knob.pressure ? "Pressure" : "CC " + std::to_string(knob.controller);
}

double knobStart(const KnobAssignment& knob) {
    if (knob.pressure || knob.controller == 1) return 0;
    if (knob.controller == 7) return 100.0 / 127;
    return 64.0 / 127;
}

PerformanceControls::PerformanceControls(NoteSink& sink) : sink_(sink) { setKnobs(defaultKnobs()); }

void PerformanceControls::setKnobs(const std::vector<KnobAssignment>& knobs) {
    std::vector<KnobAssignment> next = defaultKnobs();
    std::copy_n(knobs.begin(), std::min<std::size_t>(knobs.size(), kKnobCount), next.begin());
    values_.resize(kKnobCount, -1);
    for (int i = 0; i < kKnobCount; ++i)
        if (i >= int(knobs_.size()) || !(knobs_[i] == next[i])) values_[i] = knobStart(next[i]);
    knobs_ = std::move(next);
}

void PerformanceControls::setBend(double value) {
    value = std::clamp(value, -1.0, 1.0);
    if (value == bend_) return;
    bend_ = value;
    sink_.pitchBend(std::nullopt, value);
}

void PerformanceControls::setWheel(double value) {
    value = std::clamp(value, 0.0, 1.0);
    if (value == wheel_) return;
    wheel_ = value;
    sink_.controller(std::nullopt, 1, value);
}

double PerformanceControls::knob(int index) const {
    return index >= 0 && index < kKnobCount ? values_[index] : 0;
}

void PerformanceControls::setKnob(int index, double value) {
    if (index < 0 || index >= kKnobCount) return;
    value = std::clamp(value, 0.0, 1.0);
    if (value == values_[index]) return;
    values_[index] = value;
    const KnobAssignment& k = knobs_[index];
    if (k.pressure) sink_.pressure(std::nullopt, value);
    else sink_.controller(std::nullopt, std::clamp(k.controller, 0, 127), value);
}

} // namespace kedo
