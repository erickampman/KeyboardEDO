// The performance bar's controls: channel-wide, and nothing sent until one moves.

#include <string>
#include <vector>

#include "Check.h"
#include "kedo/Controls.h"
#include "kedo/UmpOutput.h"
#include "uw/ump/Ump.h"

using namespace kedo;
using kedo::test::near;

namespace {

struct Recorder final : NoteSink {
    std::vector<std::string> log;
    std::vector<double> values;
    static std::string who(std::optional<int> n) { return n ? std::to_string(*n) : "ch"; }
    void noteOn(const Note&, std::uint16_t) override { log.push_back("on"); }
    void noteOff(int) override { log.push_back("off"); }
    void glide(int, double) override { log.push_back("glide"); }
    void pitchBend(std::optional<int> n, double v) override { log.push_back("bend " + who(n)); values.push_back(v); }
    void controller(std::optional<int> n, int c, double v) override {
        log.push_back("cc " + who(n) + " " + std::to_string(c));
        values.push_back(v);
    }
    void pressure(std::optional<int> n, double v) override { log.push_back("pressure " + who(n)); values.push_back(v); }
};

using Log = std::vector<std::string>;

void testDefaults() {
    const auto k = defaultKnobs();
    CHECK(k.size() == std::size_t(kKnobCount));
    CHECK(!k[0].pressure && k[0].controller == 74);
    CHECK(k[1].pressure);
    CHECK(!k[2].pressure && k[2].controller == 71);
    CHECK(!k[3].pressure && k[3].controller == 7);
    CHECK(knobName(k[1]) == "Pressure");
    CHECK(knobName(k[2]) == "CC 71" && knobName(k[3]) == "CC 7");
    CHECK(knobName({.controller = 20}) == "CC 20");
    CHECK(knobName({.pressure = true}) == "Pressure");
    CHECK(near(knobStart(k[3]), 100.0 / 127) && knobStart(k[1]) == 0 && near(knobStart(k[0]), 64.0 / 127));
}

void testSendsWhatMoves() {
    Recorder r;
    PerformanceControls c(r);
    CHECK(r.log.empty());   // made, nothing sent
    CHECK(c.bend() == 0 && c.wheel() == 0 && near(c.knob(3), 100.0 / 127));

    c.setBend(0.5);
    c.setBend(0.5);   // unchanged: not sent again
    c.setBend(0);     // the spring
    c.setWheel(2);    // clamped
    c.setKnob(0, 0.25);
    c.setKnob(1, 0.75);
    c.setKnob(3, -1);
    c.setKnob(9, 1);   // no such knob
    CHECK((r.log == Log{"bend ch", "bend ch", "cc ch 1", "cc ch 74", "pressure ch", "cc ch 7"}));
    CHECK((r.values == std::vector<double>{0.5, 0, 1, 0.25, 0.75, 0}));
}

void testAssignments() {
    Recorder r;
    PerformanceControls c(r);
    c.setKnob(0, 1);
    c.setKnob(2, 1);
    r.log.clear();

    // Knob 2 becomes CC 20: back to its start, nothing sent; knob 0 is
    // unchanged and keeps its place.
    auto knobs = defaultKnobs();
    knobs[2] = {.controller = 20, .label = "Cutoff"};
    c.setKnobs(knobs);
    CHECK(r.log.empty());
    CHECK(c.knob(0) == 1 && near(c.knob(2), 64.0 / 127));
    c.setKnob(2, 0.5);
    CHECK((r.log == Log{"cc ch 20"}));

    // Too few: the rest are the defaults'.
    c.setKnobs({{.controller = 10}});
    CHECK(c.knobs().size() == std::size_t(kKnobCount) && c.knobs()[0].controller == 10 && c.knobs()[1].pressure);
}

// Through UmpOutput: channel messages on the output's group and channel, as
// MIDI 2.0 or MIDI 1.0 as it says.
void testThroughOutput() {
    OutputConfig config;
    config.group = 2;
    config.channel = 5;
    config.midi1Expression = false;
    UmpOutput out(config);
    PerformanceControls c(out);
    c.setKnob(0, 1);
    auto m = out.take();
    CHECK(m.size() == 1);
    if (m.size() == 1) {
        CHECK(m[0].wordCount == 2);
        CHECK(m[0].words[0] == ((0x4u << 28) | (2u << 24) | (0xBu << 20) | (5u << 16) | (74u << 8)));
        CHECK(m[0].words[1] == 0xFFFFFFFFu);
    }
    config.midi1Expression = true;
    out.setConfig(config);
    c.setBend(-1);
    m = out.take();
    CHECK(m.size() == 1);
    if (m.size() == 1) {
        CHECK(m[0].wordCount == 1);
        CHECK((m[0].words[0] >> 16) == ((0x2u << 12) | (2u << 8) | 0xE5u));   // MT 2, group 2, bend ch 6
        CHECK((m[0].words[0] & 0xFFFF) == 0);                                 // 14-bit 0: all the way down
    }
}

} // namespace

int main() {
    testDefaults();
    testSendsWhatMoves();
    testAssignments();
    testThroughOutput();
    return kedo::test::finish("Controls");
}
