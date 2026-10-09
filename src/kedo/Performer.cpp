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

void Performer::acquire(const Note& note, std::uint16_t velocity) {
    if (holds_[note.id]++ == 0) sink_.noteOn(note, velocity);
}

void Performer::release(int id) {
    const auto it = holds_.find(id);
    if (it == holds_.end()) return;
    if (--it->second == 0) {
        holds_.erase(it);
        sink_.noteOff(id);
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

void Performer::strike(Touch& t, const Note& note, std::uint16_t velocity) {
    if (const auto it = latched_.find(note.id); it != latched_.end()) {
        // Striking a latched note releases it; the pointer then plays nothing.
        latched_.erase(it);
        release(note.id);
        t.note.reset();
        return;
    }
    acquire(note, velocity);
    t.note = note;
    t.shows = note.id;
}

void Performer::press(Touch& t, const Key& key, Point p) {
    t.keyId = key.id();
    if (key.playable) strike(t, key.note, velocityFor(key, p));
}

void Performer::finish(Touch& t, bool latch) {
    if (!t.note) return;
    const int id = t.note->id;
    if (latch && !latched_.count(id)) {
        latched_[id] = t.shows;   // the latch takes over this hold
    } else {
        release(id);
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
    if (config_.drag == DragBehavior::Fixed && !expressionOwner_) expressionOwner_ = id;
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

    const bool canGlide = config_.glide && t.note && holds_[t.note->id] == 1 && !latched_.count(key->note.id);
    if (canGlide) {
        sink_.glide(t.note->id, key->note.pitch);
        t.shows = key->note.id;
        return;
    }
    if (t.note) {
        release(t.note->id);
        t.note.reset();
    }
    strike(t, key->note, velocityFor(*key, p));
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

// Where a message of `scope` from pointer `id` goes: its own note, or the
// channel if it's the pointer driving the channel. False: nowhere.
bool Performer::target(ExpressionScope scope, int id, const Touch& t, std::optional<int>& out) const {
    if (scope == ExpressionScope::Note) {
        if (!t.note) return false;
        out = t.note->id;
        return true;
    }
    if (expressionOwner_ != id) return false;
    out.reset();
    return true;
}

void Performer::expression(int id, Touch& t, Point p) {
    const double d = config_.expressionDistance > 0 ? config_.expressionDistance : 150;
    const double right = (p.x - t.start.x) / d;
    const double up = -(p.y - t.start.y) / d;   // y is down

    std::optional<int> to;
    const bool hBend = config_.horizontal == HorizontalControl::PitchBend;
    const bool vBend = config_.vertical == VerticalControl::PitchBend;
    if ((hBend || vBend) && target(config_.bendScope, id, t, to))
        sink_.pitchBend(to, std::clamp((hBend ? right : 0) + (vBend ? up : 0), -1.0, 1.0));

    const double v = std::clamp(up, 0.0, 1.0);
    if (config_.vertical == VerticalControl::Controller && target(config_.controllerScope, id, t, to))
        sink_.controller(to, config_.verticalController, v);
    else if (config_.vertical == VerticalControl::Pressure && target(config_.pressureScope, id, t, to))
        sink_.pressure(to, v);
}

void Performer::resetExpression(int id, const Touch& t) {
    if (config_.drag != DragBehavior::Fixed) return;
    std::optional<int> to;
    if ((config_.horizontal == HorizontalControl::PitchBend || config_.vertical == VerticalControl::PitchBend) &&
        target(config_.bendScope, id, t, to))
        sink_.pitchBend(to, 0.0);
    if (config_.vertical == VerticalControl::Controller && target(config_.controllerScope, id, t, to))
        sink_.controller(to, config_.verticalController, 0.0);
    else if (config_.vertical == VerticalControl::Pressure && target(config_.pressureScope, id, t, to))
        sink_.pressure(to, 0.0);
}

// --- Computer keys ---------------------------------------------------------------

void Performer::keyDown(int key, const Note& note, Modifiers) {
    if (keys_.count(key)) return;   // auto-repeat
    Touch& t = keys_[key];
    // Computer keys have no strike position: KeyEDO's QWERTY velocity.
    strike(t, note, config_.strikeVelocity ? 0x7F00 : config_.velocity);
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
        const int id = latched_.begin()->first;
        latched_.erase(latched_.begin());
        release(id);
    }
}

void Performer::releaseAll() {
    while (!touches_.empty()) pointerCancel(touches_.begin()->first);
    for (auto& [key, t] : keys_) finish(t, false);
    keys_.clear();
    releaseAllLatched();
}

// --- Drawing state -----------------------------------------------------------------

bool Performer::isSounding(int id) const {
    for (const auto* m : {&touches_, &keys_})
        for (const auto& [pointer, t] : *m)
            if (t.note && t.shows == id) return true;
    return isLatched(id);
}

bool Performer::isLatched(int id) const {
    for (const auto& [held, shows] : latched_)
        if (shows == id) return true;
    return false;
}

} // namespace kedo
