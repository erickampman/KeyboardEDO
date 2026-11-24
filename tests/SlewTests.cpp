// Slew: a glide slides at its rate, the seconds an octave takes, sent every kInterval,
// with time stepped by hand.

#include <string>
#include <vector>

#include "Check.h"
#include "kedo/Slew.h"

using namespace kedo;
using test::near;

namespace {

struct Recorder final : NoteSink {
    struct Call {
        std::string what;
        int id = 0;
        double value = 0;
    };
    std::vector<Call> calls;
    void noteOn(const Note& n, std::uint16_t v) override { calls.push_back({"on", n.id, double(v)}); }
    void noteOff(int id) override { calls.push_back({"off", id}); }
    void letGo(int id) override { calls.push_back({"let go", id}); }
    void glide(int id, double p) override { calls.push_back({"glide", id, p}); }
    void pitchBend(std::optional<int> id, double v) override { calls.push_back({"bend", id.value_or(-1), v}); }
    void controller(std::optional<int> id, int, double v) override { calls.push_back({"cc", id.value_or(-1), v}); }
    void pressure(std::optional<int> id, double v) override { calls.push_back({"pressure", id.value_or(-1), v}); }
    std::vector<double> glides() const {
        std::vector<double> g;
        for (const auto& c : calls)
            if (c.what == "glide") g.push_back(c.value);
        return g;
    }
};

struct Rig {
    double now = 100;
    Recorder out;
    Slew slew{out, [this] { return now; }};
    // Steps time to each nextEvent() and advances, until nothing slides (or `until`).
    void run(double until = 1e9) {
        while (const auto t = slew.nextEvent()) {
            if (*t > until) break;
            now = *t;
            slew.advance();
        }
    }
};

Note note(int id, double pitch) {
    Note n;
    n.id = id;
    n.number = int(pitch);
    n.pitch = pitch;
    return n;
}

// Off (rate 0): a glide passes straight through, as before.
void off() {
    Rig r;
    r.slew.noteOn(note(1, 60), 100);
    r.slew.glide(1, 62);
    CHECK(r.out.glides() == std::vector<double>{62});
    CHECK(!r.slew.nextEvent() && !r.slew.running());
}

// An octave a second: two semitones take a sixth of a second, in steps of
// kInterval, each where the rate says, the last exactly at the key.
void slides() {
    Rig r;
    r.slew.setTime(1);
    r.slew.noteOn(note(1, 60), 100);
    r.slew.glide(1, 62);
    CHECK(r.out.glides().empty() && r.slew.running());
    CHECK(near(*r.slew.nextEvent(), 100 + Slew::kInterval));
    r.run();
    const auto g = r.out.glides();
    CHECK(g.size() == 34);   // 1/6 s at 5 ms: 33.3 steps, the last one short
    if (g.size() > 1) CHECK(near(g[0], 60.06) && near(g[1], 60.12));
    CHECK(!g.empty() && g.back() == 62);
    for (size_t i = 1; i < g.size(); ++i) CHECK(g[i] > g[i - 1]);
    CHECK(near(r.now, 100 + 34 * Slew::kInterval));
    CHECK(!r.slew.running());

    // Down too.
    r.out.calls.clear();
    r.slew.glide(1, 61);
    r.run();
    CHECK(r.out.glides().size() == 17 && r.out.glides().back() == 61);
}

// A glide on its way to one key that's sent to another goes on from where it
// is: a slide over several keys never jumps back.
void retarget() {
    Rig r;
    r.slew.setTime(1);
    r.slew.noteOn(note(1, 60), 100);
    r.slew.glide(1, 61);
    r.run(100 + 10 * Slew::kInterval);   // 0.6 of a semitone up
    r.slew.glide(1, 62);
    r.run();
    const auto g = r.out.glides();
    CHECK(!g.empty() && g.back() == 62);
    for (size_t i = 1; i < g.size(); ++i) CHECK(g[i] > g[i - 1]);
    CHECK(near(r.now - 100, 2.0 / 12, 2 * Slew::kInterval));   // the whole way at the one rate

    // Back to where it is: it stops there.
    r.slew.glide(1, 63);
    r.run(r.now + 3 * Slew::kInterval);
    const double at = r.out.glides().back();
    r.slew.glide(1, at);
    r.run();
    CHECK(near(r.out.glides().back(), at, 0.07));
    CHECK(!r.slew.running());
}

// A note let go mid-slide stops: nothing more is sent for it.
void endsMidSlide() {
    Rig r;
    r.slew.setTime(2);
    r.slew.noteOn(note(1, 60), 100);
    r.slew.noteOn(note(2, 64), 100);
    r.slew.glide(1, 62);
    r.slew.glide(2, 65);
    r.run(100 + 4 * Slew::kInterval);
    r.slew.noteOff(1);
    r.slew.letGo(2);
    const size_t n = r.out.calls.size();
    r.run();
    CHECK(r.out.calls.size() == n && !r.slew.running());
    CHECK(r.out.calls[n - 2].what == "off" && r.out.calls[n - 1].what == "let go");

    // A glide for a note it never saw struck (one the arpeggiator made) passes.
    r.slew.glide(9, 70);
    CHECK(r.out.calls.back().what == "glide" && r.out.calls.back().id == 9 && !r.slew.running());
}

// Turning it off mid-slide sends the note where it was going.
void timeChange() {
    Rig r;
    r.slew.setTime(1);
    r.slew.noteOn(note(1, 60), 100);
    r.slew.glide(1, 64);
    r.run(100 + 5 * Slew::kInterval);
    r.slew.setTime(0);
    CHECK(r.out.glides().back() == 64 && !r.slew.running());
    r.slew.setTime(-5);
    CHECK(r.slew.time() == 0);
    r.slew.setTime(1e9);
    CHECK(r.slew.time() == Slew::kMaxTime);
}

// The larger the time, the slower: a semitone at 2 s an octave takes a sixth
// of a second, at 6 s half a second.
void slower() {
    for (const double t : {2.0, 6.0}) {
        Rig r;
        r.slew.setTime(t);
        r.slew.noteOn(note(1, 60), 100);
        r.slew.glide(1, 61);
        r.run();
        CHECK(near(r.now - 100, t / 12, Slew::kInterval));
        CHECK(r.out.glides().back() == 61);
    }
}

// Expression passes as it came.
void passes() {
    Rig r;
    r.slew.setTime(1);
    r.slew.pitchBend(3, 0.5);
    r.slew.controller(std::nullopt, 74, 0.25);
    r.slew.pressure(3, 1);
    CHECK(r.out.calls.size() == 3 && r.out.calls[0].what == "bend" && r.out.calls[1].id == -1 &&
          r.out.calls[2].what == "pressure");
}

} // namespace

int main() {
    off();
    slides();
    retarget();
    endsMidSlide();
    timeChange();
    slower();
    passes();
    return test::finish("Slew");
}
