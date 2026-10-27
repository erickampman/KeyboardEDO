// Arpeggiator: KeyEDO's patterns, divisions, gate, octave span, velocity
// source and hold, as a NoteSink between the Performer and the output, with
// time stepped by hand.

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "Check.h"
#include "kedo/Arpeggiator.h"
#include "kedo/Performer.h"

using namespace kedo;
using test::near;

namespace {

// Records each call, with the time it was made.
struct Recorder final : NoteSink {
    double* clock = nullptr;
    struct Call {
        std::string what;
        int id = 0;
        double at = 0, value = 0;
        Note note;
    };
    std::vector<Call> calls;

    void add(std::string what, int id, double value = 0, Note n = {}) {
        calls.push_back({std::move(what), id, *clock, value, n});
    }
    void noteOn(const Note& n, std::uint16_t v) override { add("on", n.id, v, n); }
    void noteOff(int id) override { add("off", id); }
    void glide(int id, double p) override { add("glide", id, p); }
    void pitchBend(std::optional<int> id, double v) override { add(id ? "bend" : "bend ch", id.value_or(-1), v); }
    void controller(std::optional<int> id, int c, double v) override {
        add((id ? "cc " : "cc ch ") + std::to_string(c), id.value_or(-1), v);
    }
    void pressure(std::optional<int> id, double v) override { add(id ? "pressure" : "pressure ch", id.value_or(-1), v); }

    // The note-ons' ids, in order.
    std::vector<int> ons() const {
        std::vector<int> out;
        for (const auto& c : calls)
            if (c.what == "on") out.push_back(c.id);
        return out;
    }
    int count(const std::string& what) const {
        return int(std::count_if(calls.begin(), calls.end(), [&](const Call& c) { return c.what == what; }));
    }
};

struct Rig {
    double now = 100;   // seconds
    Recorder out;
    Arpeggiator arp{out, [this] { return now; }};
    Rig(ArpConfig c = {.enabled = true}) {
        out.clock = &now;
        arp.setConfig(c);
    }
    // Moves time on in small ticks, as the app's thread would wake.
    void run(double seconds) {
        const double end = now + seconds;
        while (now < end - 1e-9) {
            const auto next = arp.nextEvent();
            now = next && *next < end ? std::max(now, *next) : end;
            arp.advance();
        }
    }
};

// A note with pitch `pitch`, numbered as given (default: the pitch).
Note note(int id, double pitch, int number = -1) {
    return {.id = id, .number = std::uint8_t(number < 0 ? int(pitch) : number), .pitch = pitch};
}

void off() {
    Rig r({.enabled = false});
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.glide(1, 61);
    r.arp.pitchBend(1, 0.5);
    r.arp.controller(std::nullopt, 74, 0.25);
    r.arp.pressure(1, 0.75);
    r.arp.noteOff(1);
    CHECK(r.out.calls.size() == 6);
    CHECK(r.out.calls[0].what == "on" && r.out.calls[0].value == 1000);
    CHECK(r.out.calls[1].what == "glide" && r.out.calls[1].value == 61);
    CHECK(r.out.calls[2].what == "bend" && r.out.calls[2].id == 1);
    CHECK(r.out.calls[3].what == "cc ch 74");
    CHECK(r.out.calls[5].what == "off");
    CHECK(!r.arp.running() && !r.arp.nextEvent());
}

// 120 bpm sixteenths: a step every 0.125 s, the first at once, each note
// sounding half of its step.
void timing() {
    Rig r;
    r.arp.noteOn(note(1, 60), 1000);
    CHECK(r.out.calls.size() == 1 && r.out.calls[0].what == "on" && r.out.calls[0].at == 100);
    CHECK(r.arp.nextEvent() && near(*r.arp.nextEvent(), 100.0625));
    r.run(0.49);
    // On at 0, .125, .25, .375; off half a step after each.
    std::vector<double> on, offs;
    for (const auto& c : r.out.calls) (c.what == "on" ? on : offs).push_back(c.at - 100);
    CHECK(on.size() == 4 && offs.size() == 4);
    for (size_t i = 0; i < on.size() && i < offs.size(); ++i) {
        CHECK(near(on[i], 0.125 * i));
        CHECK(near(offs[i], 0.125 * i + 0.0625));
    }
    CHECK(near(ArpConfig{.division = ArpDivision::EighthTriplet}.stepLength(), 0.5 / 3));
    CHECK(near(ArpConfig{.division = ArpDivision::Quarter, .bpm = 90}.stepLength(), 60.0 / 90));
    CHECK(near(ArpConfig{.division = ArpDivision::Whole, .bpm = 60}.stepLength(), 4.0));
}

// Gate 100%: each note ends as the next begins, never overlapping.
void fullGate() {
    Rig r({.enabled = true, .gate = 1.0});
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.noteOn(note(2, 64), 1000);
    r.run(0.3);
    std::vector<std::string> seq;
    for (const auto& c : r.out.calls) seq.push_back(c.what);
    CHECK((seq == std::vector<std::string>{"on", "off", "on", "off", "on"}));
    CHECK(near(r.out.calls[1].at, r.out.calls[2].at));
}

// The note-ons' times, from 100.
std::vector<double> onsets(const Rig& r) {
    std::vector<double> on;
    for (const auto& c : r.out.calls)
        if (c.what == "on") on.push_back(c.at - 100);
    return on;
}

// Swing: every second step late, the first of a pair taking the swing's share
// of it; each gate a share of the time until the next step.
void swing() {
    Rig r({.enabled = true, .gate = 0.5, .swing = 0.75});
    r.arp.noteOn(note(1, 60), 1000);
    r.run(0.49);
    const std::vector<double> on = onsets(r);
    CHECK(on.size() == 4 && near(on[0], 0) && near(on[1], 0.1875) && near(on[2], 0.25) && near(on[3], 0.4375));
    std::vector<double> offs;
    for (const auto& c : r.out.calls)
        if (c.what == "off") offs.push_back(c.at - 100);
    CHECK(offs.size() >= 2 && near(offs[0], 0.09375) && near(offs[1], 0.1875 + 0.03125));

    // Out of range: clamped. A change applies from the next step.
    Rig c({.enabled = true, .swing = 2});
    CHECK(c.arp.config().swing == 0.75);
    c.arp.setConfig({.enabled = true, .swing = 0});
    CHECK(c.arp.config().swing == 0.5);
    c.arp.noteOn(note(1, 60), 1000);
    c.run(0.01);
    c.arp.setConfig({.enabled = true, .swing = 2.0 / 3});
    c.run(0.3);
    const std::vector<double> later = onsets(c);
    CHECK(later.size() == 3 && near(later[1], 0.25 * 2 / 3) && near(later[2], 0.25));
}

// Slop: each step after the first early or late at random, by no more than
// the slop or a quarter of a step; the steps keep their order.
void slop() {
    Rig r({.enabled = true, .slop = 20});
    r.arp.seed(3);
    r.arp.noteOn(note(1, 60), 1000);
    r.run(2.0);
    const std::vector<double> on = onsets(r);
    CHECK(on.size() == 16 && on[0] == 0);
    bool moved = false;
    for (size_t i = 0; i < on.size(); ++i) {
        CHECK(std::abs(on[i] - 0.125 * i) <= 0.020 + 1e-9);
        moved = moved || std::abs(on[i] - 0.125 * i) > 1e-6;
    }
    CHECK(moved);

    // Fast, swung and sloppy: never more than a quarter step, never out of order.
    Rig f({.enabled = true, .division = ArpDivision::ThirtySecond, .bpm = 999, .swing = 0.75, .slop = 50});
    f.arp.seed(5);
    f.arp.noteOn(note(1, 60), 1000);
    f.run(0.5);
    const std::vector<double> fast = onsets(f);
    const double step = f.arp.config().stepLength();
    CHECK(fast.size() > 50);
    for (size_t i = 0; i < fast.size(); ++i) {
        const double grid = step * i + (i % 2 ? step / 2 : 0);
        CHECK(std::abs(fast[i] - grid) <= step / 4 + 1e-9);
        if (i) CHECK(fast[i] >= fast[i - 1]);
    }
    CHECK(f.out.count("on") - f.out.count("off") <= 1);
}

std::vector<int> pattern(ArpPattern p, int octaves = 1, int steps = 0) {
    Rig r({.enabled = true, .pattern = p, .octaves = octaves});
    r.arp.seed(7);
    // Played out of pitch order, and numbered unlike their pitches (a custom
    // layout's might be): the patterns go by pitch.
    r.arp.noteOn(note(3, 67, 1), 1000);
    r.arp.noteOn(note(1, 60, 3), 1000);
    r.arp.noteOn(note(4, 72, 0), 1000);
    r.arp.noteOn(note(2, 64, 2), 1000);
    r.out.calls.clear();
    r.run(0.125 * (steps ? steps : 8) + 0.01);
    return r.out.ons();
}

void patterns() {
    // The first note plays at once; then the pool has all four.
    using V = std::vector<int>;
    // Note 3 (67) played first, at once; the steps after it carry on from
    // it in the pattern of all four, rather than starting it again.
    CHECK((pattern(ArpPattern::Up) == V{4, 1, 2, 3, 4, 1, 2, 3}));
    CHECK((pattern(ArpPattern::Down) == V{2, 1, 4, 3, 2, 1, 4, 3}));
    CHECK((pattern(ArpPattern::UpDown, 1, 12) == V{4, 3, 2, 1, 2, 3, 4, 3, 2, 1, 2, 3}));
    CHECK((pattern(ArpPattern::DownUp, 1, 12) == V{2, 1, 2, 3, 4, 3, 2, 1, 2, 3, 4, 3}));
    CHECK((pattern(ArpPattern::AsPlayed) == V{1, 4, 2, 3, 1, 4, 2, 3}));
    // Random: shuffled afresh each round, so each note comes round, and not
    // in one fixed order.
    const V random = pattern(ArpPattern::Random, 1, 16);
    CHECK(random.size() == 16);
    for (const int id : {1, 2, 3, 4}) CHECK(std::count(random.begin(), random.end(), id) >= 3);
    CHECK(!std::equal(random.begin(), random.begin() + 4, random.begin() + 4) ||
          !std::equal(random.begin() + 4, random.begin() + 8, random.begin() + 8));
    // Two notes going up and down don't repeat their ends.
    Rig r({.enabled = true, .pattern = ArpPattern::UpDown});
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.noteOn(note(2, 64), 1000);
    r.run(0.5 - 0.01);
    CHECK((r.out.ons() == V{1, 2, 1, 2}));
}

// KeyEDO's octave span: the pattern, then the pattern an octave higher.
void octaves() {
    Rig r({.enabled = true, .pattern = ArpPattern::UpDown, .octaves = 2});
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.noteOn(note(2, 64), 1000);
    r.arp.noteOn(note(3, 67), 1000);
    r.out.calls.clear();
    r.run(0.125 * 8 + 0.01);
    std::vector<double> pitches;
    for (const auto& c : r.out.calls)
        if (c.what == "on") pitches.push_back(c.note.pitch);
    // After the first note (60) the pool has all three: 64 67 64 | 72 76 79 76 | 60 ...
    CHECK((pitches == std::vector<double>{64, 67, 64, 72, 76, 79, 76, 60}));
    // Without a layout, an octave up is 12 more in pitch and number, with an
    // id of its own; each step ends before the next.
    for (const auto& c : r.out.calls)
        if (c.what == "on" && c.note.pitch >= 72) {
            CHECK(c.note.number == int(c.note.pitch));
            CHECK(c.note.id != 1 && c.note.id != 2 && c.note.id != 3);
        }
    // (The first note's on was cleared; its off is here, the last note's not yet.)
    CHECK(r.out.count("on") == r.out.count("off"));

    // The layout's own note for the octave: a 31-EDO step 31 above.
    Rig e({.enabled = true, .octaves = 3});
    e.arp.setOctaveFinder([](const Note& n, int octaves) {
        Note up = n;
        up.id = n.id + 31 * octaves;
        up.number = std::uint8_t(n.number + 31 * octaves);
        up.pitch = n.pitch + 12 * octaves;
        return up;
    });
    e.arp.noteOn(note(10, 57, 10), 1000);
    e.run(0.3);   // the third step in its gate
    CHECK(e.out.ons() == (std::vector<int>{10, 41, 72}));
    CHECK(e.out.calls[2].note.number == 41 && near(e.out.calls[2].note.pitch, 69));
    CHECK(e.arp.sounding(72) && !e.arp.sounding(10));
}

// Releasing: a note leaves the pool; the last one up silences at once.
void release() {
    Rig r;
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.noteOn(note(2, 64), 1000);
    r.run(0.25 - 0.01);   // 1, 2
    r.arp.noteOff(1);
    r.run(0.25);          // 2, 2
    CHECK((r.out.ons() == std::vector<int>{1, 2, 2, 2}));
    r.run(0.03);          // into the last one's gate
    CHECK(r.out.calls.back().what == "on");
    r.arp.noteOff(2);
    CHECK(r.out.calls.back().what == "off" && r.out.calls.back().at == r.now);
    CHECK(!r.arp.running() && !r.arp.nextEvent());
    // A new note starts again at once.
    r.now += 3.3;
    r.arp.noteOn(note(5, 70), 1000);
    CHECK(r.out.calls.back().what == "on" && r.out.calls.back().id == 5);
}

// Hold (KeyEDO's): released notes stay, new ones add to them, and only
// stopping (or turning Hold off) clears them.
void hold() {
    Rig r({.enabled = true, .hold = true});
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.noteOff(1);
    CHECK(r.arp.inPool(1) && r.arp.heldByHold(1));
    r.arp.noteOn(note(2, 64), 1000);
    CHECK(r.arp.inPool(2) && !r.arp.heldByHold(2));
    r.arp.noteOff(2);
    r.run(0.51);
    CHECK((r.out.ons() == std::vector<int>{1, 2, 1, 2, 1}));
    // Struck again, it's played rather than held.
    r.arp.noteOn(note(1, 60), 1000);
    CHECK(!r.arp.heldByHold(1));
    // Hold off: what only Hold kept goes.
    r.arp.setConfig({.enabled = true, .hold = false});
    CHECK(r.arp.inPool(1) && !r.arp.inPool(2) && r.arp.running());
    r.arp.setConfig({.enabled = true, .hold = true});
    r.arp.stop();
    CHECK(!r.arp.inPool(1) && !r.arp.running());
    CHECK(r.out.count("on") == r.out.count("off"));
}

void velocity() {
    {
        Rig r;
        r.arp.noteOn(note(1, 60), 1234);
        CHECK(r.out.calls[0].value == 1234);
    }
    {
        Rig r({.enabled = true, .velocity = ArpVelocity::Fixed, .fixedVelocity = 127});
        r.arp.noteOn(note(1, 60), 1234);
        CHECK(r.out.calls[0].value == 0xFFFF);
        r.arp.setConfig({.enabled = true, .velocity = ArpVelocity::Fixed, .fixedVelocity = 64});
        r.run(0.125);
        CHECK(r.out.calls.back().what == "on" && near(r.out.calls.back().value, 64 * 0xFFFF / 127));
    }
    {
        Rig r({.enabled = true, .velocity = ArpVelocity::Random});
        r.arp.seed(1);
        r.arp.noteOn(note(1, 60), 1234);
        r.run(4);
        std::set<double> seen;
        for (const auto& c : r.out.calls)
            if (c.what == "on") {
                CHECK(c.value >= 0x3F00 && c.value <= 0xFFFF);
                seen.insert(c.value);
            }
        CHECK(seen.size() > 20);
    }
}

// A tempo change keeps the phase: the next step is the new length after the
// last one.
void tempo() {
    Rig r;
    r.arp.noteOn(note(1, 60), 1000);
    r.run(0.1);
    r.arp.setConfig({.enabled = true, .bpm = 60});   // a step of 0.25 s
    r.run(0.5);
    std::vector<double> on;
    for (const auto& c : r.out.calls)
        if (c.what == "on") on.push_back(c.at - 100);
    CHECK(on.size() == 3 && near(on[1], 0.25) && near(on[2], 0.5));
    // A gate shortened applies from the next note.
    r.arp.setConfig({.enabled = true, .bpm = 60, .gate = 0.1});
    r.run(0.26);
    CHECK(near(r.out.calls.back().at - r.out.calls[r.out.calls.size() - 2].at, 0.025));

    // A stall of seconds plays one step, not the ones missed.
    Rig s;
    s.arp.noteOn(note(1, 60), 1000);
    s.now += 5;
    s.arp.advance();
    CHECK(s.out.count("on") == 2);
    CHECK(near(*s.arp.nextEvent(), s.now + 0.0625));
}

// Turning it on or off empties the pool and silences it; changes to the
// pattern apply from the next step.
void toggling() {
    Rig r({.enabled = true, .hold = true});
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.setConfig({.enabled = false, .hold = true});
    CHECK(r.out.calls.back().what == "off" && !r.arp.inPool(1) && !r.arp.running());
    r.arp.setConfig({.enabled = true, .hold = true});
    CHECK(!r.arp.running());
    r.arp.noteOn(note(1, 60), 1000);
    r.arp.noteOn(note(2, 64), 1000);
    r.arp.noteOn(note(3, 67), 1000);
    r.arp.setConfig({.enabled = true, .pattern = ArpPattern::Down, .hold = true});
    r.out.calls.clear();
    r.run(0.375 + 0.01);
    // Down, carrying on from note 1, which played last.
    CHECK((r.out.ons() == std::vector<int>{3, 2, 1}));
}

// A note's own expression goes to its steps, and again after each one's
// note-on; the channel's passes through. A glide moves the note's pitch.
void expression() {
    Rig r({.enabled = true, .octaves = 2});
    r.arp.noteOn(note(1, 60), 1000);   // on 1
    r.arp.pitchBend(1, 0.5);
    r.arp.controller(1, 74, 0.25);
    r.arp.pressure(std::nullopt, 0.75);
    CHECK(r.out.calls[1].what == "bend" && r.out.calls[1].id == 1 && r.out.calls[1].value == 0.5);
    CHECK(r.out.calls[2].what == "cc 74" && r.out.calls[2].id == 1);
    CHECK(r.out.calls[3].what == "pressure ch");
    r.out.calls.clear();
    r.run(0.125);   // off 1, on the octave copy, its bend and controller
    CHECK(r.out.calls.size() == 4);
    const int up = r.out.calls[1].id;
    CHECK(r.out.calls[1].what == "on" && up != 1);
    CHECK(r.out.calls[2].what == "bend" && r.out.calls[2].id == up && r.out.calls[2].value == 0.5);
    CHECK(r.out.calls[3].what == "cc 74" && r.out.calls[3].id == up);
    // Expression on a note in the pool that isn't sounding is kept for later.
    r.arp.noteOn(note(2, 64), 1000);
    r.out.calls.clear();
    r.arp.pressure(2, 0.5);
    CHECK(r.out.calls.empty());
    // Glide: the sounding octave copy follows, an octave up.
    r.arp.glide(1, 61);
    CHECK(r.out.calls.size() == 1 && r.out.calls[0].what == "glide" && r.out.calls[0].id == up &&
          r.out.calls[0].value == 73);
}

// Between a Performer and the output: keys pressed join the pool; the
// Performer's latch holds them there too.
void withPerformer() {
    const Tuning t;
    const PianoLayout piano(t, 21, 108);
    Rig r;
    Performer p(r.arp);
    p.setLayout(&piano);
    const Note a = t.note(36), cs = t.note(40);
    p.keyDown(1, a);
    p.keyDown(2, cs, {.latch = true});
    p.keyUp(2, {.latch = true});
    CHECK(r.arp.inPool(a.id) && r.arp.inPool(cs.id) && !r.arp.heldByHold(cs.id));
    r.run(0.25 - 0.01);
    CHECK(r.out.ons().size() == 2);
    p.releaseAll();
    CHECK(!r.arp.running());
    CHECK(r.out.count("on") == r.out.count("off"));
}

} // namespace

int main() {
    off();
    timing();
    fullGate();
    swing();
    slop();
    patterns();
    octaves();
    release();
    hold();
    velocity();
    tempo();
    toggling();
    expression();
    withPerformer();
    return test::finish("Arpeggiator");
}
