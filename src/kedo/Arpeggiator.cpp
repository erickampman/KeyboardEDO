#include "kedo/Arpeggiator.h"

#include <algorithm>

namespace kedo {

namespace {

const char* const kPatternNames[] = {"Up", "Down", "Up-Down", "Down-Up", "As Played", "Random"};
const char* const kDivisionNames[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/2T", "1/4T", "1/8T", "1/16T"};
const double kQuarterNotes[] = {4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 4.0 / 3, 2.0 / 3, 1.0 / 3, 1.0 / 6};
const char* const kVelocityNames[] = {"As Played", "Fixed", "Random"};

// Octave copies made without a layout get ids of their own, past any step.
constexpr int kOctaveIdStride = 1 << 24;

} // namespace

const char* arpPatternName(ArpPattern p) { return kPatternNames[int(p)]; }
const char* arpDivisionName(ArpDivision d) { return kDivisionNames[int(d)]; }
const char* arpVelocityName(ArpVelocity v) { return kVelocityNames[int(v)]; }
double quarterNotes(ArpDivision d) { return kQuarterNotes[int(d)]; }

double ArpConfig::stepLength() const { return 60.0 / std::clamp(bpm, 1.0, 999.0) * quarterNotes(division); }

Note Arpeggiator::octaveAbove(const Note& note, int octaves) {
    Note n = note;
    n.pitch += 12.0 * octaves;
    n.number = std::uint8_t(std::clamp(int(note.number) + 12 * octaves, 0, 127));
    n.id = note.id + octaves * kOctaveIdStride;
    return n;
}

Arpeggiator::Arpeggiator(NoteSink& out, Clock clock) : out_(out), clock_(std::move(clock)) {}

void Arpeggiator::setConfig(const ArpConfig& c) {
    const ArpConfig old = config_;
    config_ = c;
    config_.bpm = std::clamp(c.bpm, 1.0, 999.0);
    config_.gate = std::clamp(c.gate, 0.01, 1.0);
    config_.octaves = std::clamp(c.octaves, 1, 4);
    config_.fixedVelocity = std::clamp(c.fixedVelocity, 1, 127);
    config_.swing = std::clamp(c.swing, 0.5, 0.75);
    config_.slop = std::clamp(c.slop, 0.0, 50.0);
    // Hold off: the notes it kept go, as a sustain pedal's would, on or off.
    if (old.hold && !config_.hold) std::erase_if(pool_, [](const Slot& s) { return !s.pressed; });
    // Off, it falls silent but keeps what Hold kept (the caller released what
    // was pressed); on again, it plays them, and Clear (stop()) empties it.
    if (config_.enabled != old.enabled) {
        idle();
        std::erase_if(pool_, [](const Slot& s) { return s.pressed; });
        if (on() && !pool_.empty()) start();
        return;
    }
    if (!on()) return;
    if (pool_.empty()) {
        idle();
        return;
    }
    if (old.hold && !config_.hold) rebuild();
    if (config_.pattern != old.pattern || config_.octaves != old.octaves) rebuild();
    // A new tempo: the next step a new step's length after the last, so the
    // change is heard at once without a jump. A new swing moves it too.
    if (running_ && (config_.stepLength() != old.stepLength() || config_.swing != old.swing)) {
        grid_ = last_ + config_.stepLength();
        schedule();
    }
}

Arpeggiator::Slot* Arpeggiator::slot(int id) {
    for (Slot& s : pool_)
        if (s.note.id == id) return &s;
    return nullptr;
}

bool Arpeggiator::inPool(int id) const {
    return std::any_of(pool_.begin(), pool_.end(), [&](const Slot& s) { return s.note.id == id; });
}

bool Arpeggiator::heldByHold(int id) const {
    return std::any_of(pool_.begin(), pool_.end(), [&](const Slot& s) { return s.note.id == id && !s.pressed; });
}

std::vector<Arpeggiator::PoolNote> Arpeggiator::pool() const {
    std::vector<PoolNote> notes;
    for (const Slot& s : pool_) notes.push_back({s.note, s.velocity, !s.pressed});
    return notes;
}

std::vector<std::pair<Note, std::uint16_t>> Arpeggiator::heldNotes() const {
    std::vector<std::pair<Note, std::uint16_t>> held;
    for (const Slot& s : pool_)
        if (!s.pressed) held.emplace_back(s.note, s.velocity);
    return held;
}

// --- The sequence -------------------------------------------------------------------

// KeyEDO's: the pattern over the pool, then repeated an octave higher for each
// octave of the span. Sorted by pitch, so a layout's own numbering doesn't
// matter; Up-Down and Down-Up don't repeat their ends.
void Arpeggiator::rebuild() {
    std::vector<int> sorted, base;
    for (const Slot& s : pool_) sorted.push_back(s.note.id);
    auto pitchOf = [&](int id) { return slot(id)->note.pitch; };
    std::stable_sort(sorted.begin(), sorted.end(), [&](int a, int b) {
        return pitchOf(a) != pitchOf(b) ? pitchOf(a) < pitchOf(b) : a < b;
    });
    auto there_and_back = [&](std::vector<int> v) {
        if (v.size() > 2) {
            const std::vector<int> back(v.rbegin() + 1, v.rend() - 1);
            v.insert(v.end(), back.begin(), back.end());
        }
        return v;
    };
    switch (config_.pattern) {
    case ArpPattern::Up: base = sorted; break;
    case ArpPattern::Down: base.assign(sorted.rbegin(), sorted.rend()); break;
    case ArpPattern::UpDown: base = there_and_back(sorted); break;
    case ArpPattern::DownUp: base = there_and_back({sorted.rbegin(), sorted.rend()}); break;
    case ArpPattern::AsPlayed:
        for (const Slot& s : pool_) base.push_back(s.note.id);
        break;
    case ArpPattern::Random:
        for (const Slot& s : pool_) base.push_back(s.note.id);
        std::shuffle(base.begin(), base.end(), random_);
        break;
    }
    sequence_.clear();
    for (int octave = 0; octave < config_.octaves; ++octave)
        for (const int id : base) sequence_.push_back({id, octave});
    if (sequence_.empty()) return;
    // Carry on after the step played last, so a chord's first note (played
    // at once, alone) isn't played again as the rest arrive.
    index_ %= sequence_.size();
    if (played_)
        for (std::size_t i = 0; i < sequence_.size(); ++i)
            if (sequence_[i].id == played_->id && sequence_[i].octave == played_->octave) {
                index_ = (i + 1) % sequence_.size();
                break;
            }
}

// --- Time -------------------------------------------------------------------------------

std::uint16_t Arpeggiator::velocityFor(const Slot& s) {
    switch (config_.velocity) {
    case ArpVelocity::AsPlayed: return s.velocity;
    case ArpVelocity::Fixed: return std::uint16_t(config_.fixedVelocity * 0xFFFF / 127);
    case ArpVelocity::Random: return std::uniform_int_distribution<int>(0x3F00, 0xFFFF)(random_);
    }
    return s.velocity;
}

void Arpeggiator::reapply(const Slot& s, int id) {
    if (s.bend) out_.pitchBend(id, *s.bend);
    for (const auto& [number, value] : s.controllers) out_.controller(id, number, value);
    if (s.pressure) out_.pressure(id, *s.pressure);
}

void Arpeggiator::silence() {
    if (!active_) return;
    out_.noteOff(active_->out.id);
    active_.reset();
}

void Arpeggiator::idle() {
    silence();
    running_ = false;
    count_ = 0;
    jitter_ = 0;
    index_ = 0;
    played_.reset();
    sequence_.clear();
}

void Arpeggiator::stop() {
    pool_.clear();
    idle();
}

// When the next step begins: its place on the grid, an odd step later by the
// swing, and its slop. Swing moves a step at most half a step and slop a
// quarter, so the steps never change places.
void Arpeggiator::schedule() {
    const double length = config_.stepLength();
    const double swing = count_ % 2 ? (2 * config_.swing - 1) * length : 0;
    next_ = grid_ + swing + std::clamp(jitter_, -length / 4, length / 4);
}

// The step beginning at `at`, `grid` its place on the grid: the last note
// ends (a hard gate, as KeyEDO, so two never overlap), the next starts. Its
// gate is a share of the time until the step after it.
void Arpeggiator::fire(double at, double grid) {
    silence();
    if (sequence_.empty()) {
        idle();
        return;
    }
    const Step step = sequence_[index_];
    index_ = (index_ + 1) % sequence_.size();
    played_ = step;
    const Slot s = *slot(step.id);
    // A new shuffle each time round.
    if (config_.pattern == ArpPattern::Random && index_ == 0) {
        rebuild();
        index_ = 0;
    }

    const Note out = step.octave == 0 ? s.note : octave_ ? octave_(s.note, step.octave) : octaveAbove(s.note, step.octave);
    out_.noteOn(out, velocityFor(s));
    reapply(s, out.id);
    active_ = Active{step.id, step.octave, out};
    last_ = grid;
    grid_ = grid + config_.stepLength();
    ++count_;
    jitter_ = config_.slop > 0 ? std::uniform_real_distribution<double>(-1, 1)(random_) * config_.slop / 1000 : 0;
    schedule();
    offAt_ = at + config_.gate * (next_ - at);
}

void Arpeggiator::advance() {
    if (!on()) return;
    const double now = clock_();
    for (;;) {
        // A gate that ends with the step (100%) ends before the next note.
        if (active_ && offAt_ <= now && (!running_ || offAt_ <= next_)) {
            silence();
        } else if (running_ && next_ <= now) {
            // Late by more than a step (the computer stalled): carry on from
            // now rather than play the missed steps at once.
            if (now - next_ >= config_.stepLength()) fire(now, now);
            else fire(next_, grid_);
        } else {
            break;
        }
    }
}

std::optional<double> Arpeggiator::nextEvent() const {
    if (!on()) return std::nullopt;
    std::optional<double> t;
    if (active_) t = offAt_;
    if (running_) t = t ? std::min(*t, next_) : next_;
    return t;
}

// --- NoteSink ---------------------------------------------------------------------------

void Arpeggiator::noteOn(const Note& note, std::uint16_t velocity) {
    if (!on()) return out_.noteOn(note, velocity);
    if (Slot* s = slot(note.id)) {
        // Struck again while Hold kept it: let go, as a latched note is.
        if (!s->pressed) remove(note.id);
        return;
    }
    pool_.push_back(Slot{.note = note, .velocity = velocity});
    rebuild();
    if (!running_) start();
}

// The first note plays at once, and the steps count from it.
void Arpeggiator::start() {
    rebuild();
    running_ = true;
    index_ = 0;
    count_ = 0;
    jitter_ = 0;
    grid_ = next_ = clock_();
    advance();
}

void Arpeggiator::keep(const Note& note, std::uint16_t velocity) {
    if (slot(note.id)) return;
    pool_.push_back(Slot{.note = note, .velocity = velocity, .pressed = false});
    rebuild();
    if (on() && !running_) start();
}

void Arpeggiator::noteOff(int id) {
    if (!on()) return out_.noteOff(id);
    Slot* s = slot(id);
    if (!s) return;
    if (config_.hold) {
        s->pressed = false;
        return;
    }
    remove(id);
}

void Arpeggiator::letGo(int id) {
    if (!on()) return out_.noteOff(id);
    if (slot(id)) remove(id);
}

void Arpeggiator::remove(int id) {
    std::erase_if(pool_, [&](const Slot& x) { return x.note.id == id; });
    if (pool_.empty()) idle();   // the last key up: silence at once
    else rebuild();
}

void Arpeggiator::glide(int id, double pitch) {
    if (!on()) return out_.glide(id, pitch);
    Slot* s = slot(id);
    if (!s) return;
    s->note.pitch = pitch;
    if (active_ && active_->from == id) {
        active_->out.pitch = pitch + 12.0 * active_->octave;
        out_.glide(active_->out.id, active_->out.pitch);
    }
    rebuild();
}

// Expression for the whole channel passes through. A note's own goes to its
// step while that sounds, and is kept for its next steps.
void Arpeggiator::pitchBend(std::optional<int> id, double value) {
    if (!on() || !id) return out_.pitchBend(id, value);
    if (Slot* s = slot(*id)) {
        s->bend = value;
        if (active_ && active_->from == *id) out_.pitchBend(active_->out.id, value);
    }
}

void Arpeggiator::controller(std::optional<int> id, int number, double value) {
    if (!on() || !id) return out_.controller(id, number, value);
    if (Slot* s = slot(*id)) {
        s->controllers[number] = value;
        if (active_ && active_->from == *id) out_.controller(active_->out.id, number, value);
    }
}

void Arpeggiator::pressure(std::optional<int> id, double value) {
    if (!on() || !id) return out_.pressure(id, value);
    if (Slot* s = slot(*id)) {
        s->pressure = value;
        if (active_ && active_->from == *id) out_.pressure(active_->out.id, value);
    }
}

} // namespace kedo
