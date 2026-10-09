#include "kedo/Performer.h"

#include <algorithm>
#include <cmath>

#include "uw/ump/Ump.h"

namespace kedo {

Performer::Performer(NoteSink& sink) : sink_(sink) {}

void Performer::setLayout(const Layout* layout) {
    if (layout == layout_) return;
    releaseAll();
    layout_ = layout;
}

void Performer::setLatchMode(bool on) {
    latchMode_ = on;
    if (!on) releaseAllLatched();
}

// --- Holding notes ------------------------------------------------------------
//
// Tiled repeats steps, so two fingers can hold the same note. It sounds once,
// from the first hold, and stops when the last hold goes.

void Performer::acquire(int note, std::uint16_t velocity) {
    if (holds_[note]++ == 0) sink_.noteOn(note, velocity);
}

void Performer::release(int note) {
    const auto it = holds_.find(note);
    if (it == holds_.end()) return;
    if (--it->second == 0) {
        holds_.erase(it);
        sink_.noteOff(note);
    }
}

std::uint16_t Performer::velocityFor(const Key& key, Point p) const {
    if (!config_.strikeVelocity) return config_.velocity;
    // KeyEDO: the top 12% of a key is full velocity; below it, velocity falls
    // in a line to the minimum at the bottom.
    constexpr double kTopZone = 0.12;
    const double depth = std::clamp(key.depth(p), 0.0, 1.0);
    if (depth <= kTopZone) return 0xFFFF;
    const double t = (depth - kTopZone) / (1.0 - kTopZone);
    const double minimum = uw::ump::scaleUp(std::uint32_t(std::clamp(config_.strikeVelocityMin, 1, 127)), 7, 16);
    return std::uint16_t(std::lround(0xFFFF + t * (minimum - 0xFFFF)));
}

void Performer::strike(Touch& t, int step, std::uint16_t velocity) {
    if (const auto it = latched_.find(step); it != latched_.end()) {
        // Striking a latched note releases it; the pointer then plays nothing.
        latched_.erase(it);
        release(step);
        t.note.reset();
        return;
    }
    acquire(step, velocity);
    t.note = step;
    t.step = step;
}

void Performer::press(Touch& t, const Key& key, Point p) {
    t.keyId = key.id();
    if (key.playable) strike(t, key.step, velocityFor(key, p));
}

void Performer::finish(Touch& t, bool latch) {
    if (!t.note) return;
    const int note = *t.note;
    if (latch && !latched_.count(note)) {
        latched_[note] = t.step;   // the latch takes over this hold
    } else {
        release(note);
    }
    t.note.reset();
}

// --- Pointers -------------------------------------------------------------------

void Performer::pointerDown(int id, Point p, Modifiers) {
    if (!layout_) return;
    pointerCancel(id);   // a stale pointer with this id
    Touch& t = touches_[id];
    t.start = p;
    if (const auto key = layout_->keyAt(p)) press(t, *key, p);
    if (config_.drag == DragBehavior::Fixed && !config_.perNoteExpression && !expressionOwner_)
        expressionOwner_ = id;
}

void Performer::pointerMove(int id, Point p) {
    const auto it = touches_.find(id);
    if (it == touches_.end() || !layout_) return;
    Touch& t = it->second;

    if (config_.drag == DragBehavior::Fixed) {
        expression(id, t, p);
        return;
    }

    // Gliss. Between keys (the Diamond layout's gaps) the note holds until the
    // pointer reaches the next key, as KeyEDO on the Mac.
    const auto key = layout_->keyAt(p);
    if (!key || !key->playable || key->id() == t.keyId) return;
    t.keyId = key->id();

    const bool canGlide = config_.glide && t.note && holds_[*t.note] == 1 && !latched_.count(key->step);
    if (canGlide) {
        sink_.glide(*t.note, key->step);
        t.step = key->step;
        return;
    }
    if (t.note) {
        release(*t.note);
        t.note.reset();
    }
    strike(t, key->step, velocityFor(*key, p));
}

void Performer::pointerUp(int id, Modifiers mods) {
    const auto it = touches_.find(id);
    if (it == touches_.end()) return;
    resetExpression(id, it->second);
    finish(it->second, mods.latch || latchMode_);
    touches_.erase(it);
    if (expressionOwner_ == id) expressionOwner_.reset();
}

void Performer::pointerCancel(int id) {
    const auto it = touches_.find(id);
    if (it == touches_.end()) return;
    resetExpression(id, it->second);
    finish(it->second, false);
    touches_.erase(it);
    if (expressionOwner_ == id) expressionOwner_.reset();
}

// --- Expression -------------------------------------------------------------------

void Performer::expression(int id, Touch& t, Point p) {
    std::optional<int> target;
    if (config_.perNoteExpression) {
        if (!t.note) return;
        target = t.note;
    } else if (expressionOwner_ != id) {
        return;
    }

    const double d = config_.expressionDistance > 0 ? config_.expressionDistance : 150;
    const double right = (p.x - t.start.x) / d;
    const double up = -(p.y - t.start.y) / d;   // y is down

    const bool hBend = config_.horizontal == HorizontalControl::PitchBend;
    const bool vBend = config_.vertical == VerticalControl::PitchBend;
    if (hBend || vBend)
        sink_.pitchBend(target, std::clamp((hBend ? right : 0) + (vBend ? up : 0), -1.0, 1.0));

    const double v = std::clamp(up, 0.0, 1.0);
    if (config_.vertical == VerticalControl::Controller)
        sink_.controller(target, config_.verticalController, v);
    else if (config_.vertical == VerticalControl::Pressure)
        sink_.pressure(target, v);
}

void Performer::resetExpression(int id, const Touch& t) {
    if (config_.drag != DragBehavior::Fixed) return;
    std::optional<int> target;
    if (config_.perNoteExpression) {
        if (!t.note) return;
        target = t.note;
    } else if (expressionOwner_ != id) {
        return;
    }
    if (config_.horizontal == HorizontalControl::PitchBend || config_.vertical == VerticalControl::PitchBend)
        sink_.pitchBend(target, 0.0);
    if (config_.vertical == VerticalControl::Controller)
        sink_.controller(target, config_.verticalController, 0.0);
    else if (config_.vertical == VerticalControl::Pressure)
        sink_.pressure(target, 0.0);
}

// --- Computer keys ---------------------------------------------------------------

void Performer::keyDown(int key, int step, Modifiers) {
    if (keys_.count(key)) return;   // auto-repeat
    Touch& t = keys_[key];
    const auto k = layout_ ? layout_->keyForStep(step) : std::nullopt;
    if (!k || !k->playable) return;
    // Computer keys have no strike position: KeyEDO's QWERTY velocity.
    strike(t, step, config_.strikeVelocity ? 0x7F00 : config_.velocity);
}

void Performer::keyUp(int key, Modifiers mods) {
    const auto it = keys_.find(key);
    if (it == keys_.end()) return;
    finish(it->second, mods.latch || latchMode_);
    keys_.erase(it);
}

// --- Releasing ---------------------------------------------------------------------

void Performer::releaseAllLatched() {
    while (!latched_.empty()) {
        const int note = latched_.begin()->first;
        latched_.erase(latched_.begin());
        release(note);
    }
}

void Performer::releaseAll() {
    while (!touches_.empty()) pointerCancel(touches_.begin()->first);
    for (auto& [key, t] : keys_) finish(t, false);
    keys_.clear();
    releaseAllLatched();
}

// --- Drawing state -----------------------------------------------------------------

bool Performer::isSounding(int step) const {
    for (const auto* m : {&touches_, &keys_})
        for (const auto& [id, t] : *m)
            if (t.note && t.step == step) return true;
    return isLatched(step);
}

bool Performer::isLatched(int step) const {
    for (const auto& [note, sounding] : latched_)
        if (sounding == step) return true;
    return false;
}

} // namespace kedo
