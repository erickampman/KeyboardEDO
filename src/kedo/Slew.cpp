#include "kedo/Slew.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace kedo {

Slew::Slew(NoteSink& out, Clock clock) : out_(out), clock_(std::move(clock)) {}

void Slew::setRate(double centsPerSecond) {
    const double now = clock_();
    // The slides so far at the old rate, then on at the new one.
    for (auto& [id, s] : sliding_) move(id, s, now);
    rate_ = std::isfinite(centsPerSecond) ? std::clamp(centsPerSecond, 0.0, kMaxRate) : 0.0;
    if (rate_ == 0) {
        for (auto& [id, s] : sliding_) {
            pitch_[id] = s.target;
            out_.glide(id, s.target);
        }
        sliding_.clear();
    }
}

bool Slew::move(int id, Slide& s, double now) {
    double&      pitch = pitch_[id];
    const double step  = rate_ / 100 * std::max(0.0, now - s.at);   // semitones
    s.at               = now;
    if (std::abs(s.target - pitch) <= step) {
        pitch = s.target;
        return true;
    }
    pitch += s.target > pitch ? step : -step;
    return false;
}

void Slew::advance() {
    const double now = clock_();
    for (auto it = sliding_.begin(); it != sliding_.end();) {
        if (now < it->second.at + kInterval) {
            ++it;
            continue;
        }
        const bool there = move(it->first, it->second, now);
        out_.glide(it->first, pitch_[it->first]);
        it = there ? sliding_.erase(it) : std::next(it);
    }
}

std::optional<double> Slew::nextEvent() const {
    std::optional<double> t;
    for (const auto& [id, s] : sliding_) t = t ? std::min(*t, s.at + kInterval) : s.at + kInterval;
    return t;
}

void Slew::forget(int id) {
    pitch_.erase(id);
    sliding_.erase(id);
}

void Slew::noteOn(const Note& note, std::uint16_t velocity) {
    forget(note.id);
    pitch_[note.id] = note.pitch;
    out_.noteOn(note, velocity);
}

void Slew::noteOff(int id) {
    forget(id);
    out_.noteOff(id);
}

void Slew::letGo(int id) {
    forget(id);
    out_.letGo(id);
}

void Slew::glide(int id, double pitch) {
    const auto known = pitch_.find(id);
    if (rate_ == 0 || known == pitch_.end()) {
        if (known != pitch_.end()) known->second = pitch;
        sliding_.erase(id);
        out_.glide(id, pitch);
        return;
    }
    const double now = clock_();
    // Already on its way: from where it is now. Otherwise it starts here, and
    // its first step goes out at the next interval.
    if (const auto it = sliding_.find(id); it != sliding_.end()) {
        move(id, it->second, now);
        it->second.target = pitch;
    } else if (known->second != pitch) {
        sliding_[id] = Slide{pitch, now};
    }
}

} // namespace kedo
