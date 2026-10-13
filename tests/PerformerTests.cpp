// Performer: pointers and keys in, NoteSink calls out.

#include <cmath>
#include <string>
#include <vector>

#include "Check.h"
#include "kedo/Performer.h"

using namespace kedo;

namespace {

// Records each call as a line of text.
struct Recorder final : NoteSink {
    std::vector<std::string> log;
    std::vector<double> values;

    static std::string who(std::optional<int> n) { return n ? std::to_string(*n) : "ch"; }
    std::vector<Note> notes;   // every noteOn's Note
    void noteOn(const Note& n, std::uint16_t v) override {
        log.push_back("on " + std::to_string(n.id));
        values.push_back(v);
        notes.push_back(n);
    }
    void noteOff(int n) override { log.push_back("off " + std::to_string(n)); }
    std::vector<double> glides;   // every glide's pitch
    void glide(int n, double pitch) override {
        log.push_back("glide " + std::to_string(n));
        glides.push_back(pitch);
    }
    void pitchBend(std::optional<int> n, double v) override { log.push_back("bend " + who(n)); values.push_back(v); }
    void controller(std::optional<int> n, int c, double v) override {
        log.push_back("cc " + who(n) + " " + std::to_string(c));
        values.push_back(v);
    }
    void pressure(std::optional<int> n, double v) override { log.push_back("pressure " + who(n)); values.push_back(v); }

    std::vector<std::string> take() {
        auto l = log;
        log.clear();
        values.clear();
        return l;
    }
};

using Log = std::vector<std::string>;

struct Rig {
    Tuning tuning;
    std::unique_ptr<TiledLayout> layout;
    Recorder sink;
    Performer performer{sink};

    Rig() {
        tuning.edo = 17;
        tuning.originOctave = -1;   // the lowest key the origin, so tile (0, 0) is step 0
        layout = std::make_unique<TiledLayout>(tuning, Generator{"Wicki-Hayden", 3, 10}, 8);
        performer.setLayout(layout.get());
    }
    Point at(int col, int row) const { return layout->keyAtLattice(col, row).center; }
};

void testTapAndGliss() {
    Rig r;
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerMove(1, r.at(1, 0));
    r.performer.pointerMove(1, {r.at(1, 0).x + 3, r.at(1, 0).y});   // same key: nothing
    r.performer.pointerUp(1);
    CHECK((r.sink.take() == Log{"on 0", "off 0", "on 3", "off 3"}));
}

void testMultitouchAndSharedSteps() {
    Rig r;
    // Two fingers, two notes.
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerDown(2, r.at(0, 1));
    CHECK(r.performer.isSounding(0) && r.performer.isSounding(10));
    r.performer.pointerUp(1);
    r.performer.pointerUp(2);
    CHECK((r.sink.take() == Log{"on 0", "on 10", "off 0", "off 10"}));

    // Tiled repeats steps: (10, 0) and (0, 3) both play 30. Two fingers on
    // them hold one note.
    CHECK(r.layout->keyAtLattice(10, 0).step == 30 && r.layout->keyAtLattice(0, 3).step == 30);
    r.performer.pointerDown(1, r.at(10, 0));
    r.performer.pointerDown(2, r.at(0, 3));
    r.performer.pointerUp(1);   // still held by the other finger
    CHECK((r.sink.take() == Log{"on 30"}));
    r.performer.pointerUp(2);
    CHECK((r.sink.take() == Log{"off 30"}));
}

void testLatch() {
    Rig r;
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerUp(1, {.latch = true});
    CHECK(r.performer.isLatched(0));
    CHECK((r.sink.take() == Log{"on 0"}));
    // Striking a latched note releases it, and the pointer plays nothing.
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerUp(1);
    CHECK((r.sink.take() == Log{"off 0"}));

    // Latch mode: every release latches; turning it off releases them all.
    r.performer.setLatchMode(true);
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerUp(1);
    r.performer.pointerDown(1, r.at(1, 0));
    r.performer.pointerUp(1);
    CHECK((r.sink.take() == Log{"on 0", "on 3"}));
    r.performer.setLatchMode(false);
    CHECK((r.sink.take() == Log{"off 0", "off 3"}));
}

void testGlide() {
    Rig r;
    PlayConfig c;
    c.glide = true;
    r.performer.setConfig(c);
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerMove(1, r.at(1, 0));
    r.performer.pointerMove(1, r.at(1, 1));
    CHECK(r.performer.isSounding(13) && !r.performer.isSounding(0));
    r.performer.pointerUp(1);
    CHECK((r.sink.log == Log{"on 0", "glide 0", "glide 0", "off 0"}));
    // Each glide goes to the exact pitch of the key reached.
    CHECK(r.sink.glides.size() == 2);
    CHECK(kedo::test::near(r.sink.glides[0], r.tuning.semitones(3)));
    CHECK(kedo::test::near(r.sink.glides[1], r.tuning.semitones(13)));
    r.sink.take();
    r.sink.glides.clear();
}

void testFixedPerNote() {
    Rig r;
    PlayConfig c;
    c.drag = DragBehavior::Fixed;
    c.horizontal = HorizontalControl::PitchBend;
    c.vertical = VerticalControl::Controller;
    c.verticalController = 74;
    c.expressionDistance = 100;
    r.performer.setConfig(c);

    const Point a = r.at(0, 0), b = r.at(0, 2);
    r.performer.pointerDown(1, a);
    r.performer.pointerDown(2, b);
    r.sink.take();
    // Each finger bends its own note; the note doesn't change.
    r.performer.pointerMove(1, {a.x + 50, a.y - 100});
    CHECK((r.sink.log == Log{"bend 0", "cc 0 74"}));
    CHECK(kedo::test::near(r.sink.values[0], 0.5));
    CHECK(kedo::test::near(r.sink.values[1], 1.0));
    r.sink.take();
    r.performer.pointerMove(2, {b.x - 200, b.y});
    CHECK((r.sink.log == Log{"bend 20", "cc 20 74"}));
    CHECK(kedo::test::near(r.sink.values[0], -1.0));   // clamped
    r.sink.take();
    // Release resets that note's expression, then ends it.
    r.performer.pointerUp(1);
    CHECK((r.sink.take() == Log{"bend 0", "cc 0 74", "off 0"}));
}

void testFixedChannel() {
    Rig r;
    PlayConfig c;
    c.drag = DragBehavior::Fixed;
    c.horizontal = HorizontalControl::PitchBend;
    c.bendScope = ExpressionScope::Channel;
    r.performer.setConfig(c);
    r.performer.pointerDown(1, r.at(0, 0));
    r.performer.pointerDown(2, r.at(1, 0));
    r.sink.take();
    // Only the first pointer drives the channel, as KeyEDO.
    r.performer.pointerMove(2, {r.at(1, 0).x + 20, r.at(1, 0).y});
    CHECK(r.sink.take().empty());
    r.performer.pointerMove(1, {r.at(0, 0).x + 20, r.at(0, 0).y});
    CHECK((r.sink.take() == Log{"bend ch"}));
}

// Each message's scope is its own: here the bend goes to the channel, from
// the first finger only, while pressure stays on each finger's note.
void testMixedScopes() {
    Rig r;
    PlayConfig c;
    c.drag = DragBehavior::Fixed;
    c.horizontal = HorizontalControl::PitchBend;
    c.vertical = VerticalControl::Pressure;
    c.bendScope = ExpressionScope::Channel;
    c.pressureScope = ExpressionScope::Note;
    c.expressionDistance = 100;
    r.performer.setConfig(c);
    const Point a = r.at(0, 0), b = r.at(0, 2);
    r.performer.pointerDown(1, a);
    r.performer.pointerDown(2, b);
    r.sink.take();
    r.performer.pointerMove(2, {b.x + 50, b.y - 50});   // not the first finger: no bend
    CHECK((r.sink.take() == Log{"pressure 20"}));
    r.performer.pointerMove(1, {a.x + 50, a.y - 50});
    CHECK((r.sink.take() == Log{"bend ch", "pressure 0"}));
    // The first finger lifts; the channel isn't handed on mid-gesture.
    r.performer.pointerUp(1);
    CHECK((r.sink.take() == Log{"bend ch", "pressure 0", "off 0"}));
    r.performer.pointerMove(2, {b.x + 60, b.y - 60});
    CHECK((r.sink.take() == Log{"pressure 20"}));
}

void testStrikeVelocity() {
    Rig r;
    PlayConfig c;
    c.strikeVelocity = true;
    c.strikeVelocityMin = 1;
    r.performer.setConfig(c);
    const Key k = r.layout->keyAtLattice(0, 0);
    r.performer.pointerDown(1, {k.center.x, k.center.y - k.halfHeight + 1});   // near the top
    r.performer.pointerDown(2, r.at(1, 0));                                      // the middle
    CHECK(r.sink.values.size() == 2);
    CHECK(r.sink.values[0] == 0xFFFF);
    CHECK(r.sink.values[1] < 0xFFFF && r.sink.values[1] > 0x7000);
}

void testKeysAndRelease() {
    Rig r;
    r.performer.keyDown('z', r.tuning.note(0));
    r.performer.keyDown('z', r.tuning.note(0));   // auto-repeat
    r.performer.pointerDown(1, r.at(1, 0));
    CHECK((r.sink.take() == Log{"on 0", "on 3"}));
    r.performer.releaseAll();
    const auto log = r.sink.take();
    CHECK(log.size() == 2);
    CHECK(!r.performer.isSounding(0) && !r.performer.isSounding(3));
    // A new layout releases everything too.
    r.performer.pointerDown(1, r.at(0, 0));
    r.sink.take();
    TiledLayout other(r.tuning, Generator{"Bosanquet", 1, 10}, 4);
    r.performer.setLayout(&other);
    CHECK((r.sink.take() == Log{"off 0"}));
}

// The Performer passes each key's Note through as it is: number, pitch,
// group and channel, whatever made them.
void testNotesPassThrough() {
    Rig r;
    r.performer.pointerDown(1, r.at(2, 1));   // step 16
    CHECK(r.sink.notes.size() == 1);
    CHECK(r.sink.notes[0] == r.tuning.note(16));
    r.performer.pointerUp(1);

    Note custom;
    custom.id = 500;
    custom.number = 61;
    custom.pitch = 61.3;
    custom.channel = 4;
    r.sink.notes.clear();
    r.performer.keyDown('q', custom);
    CHECK(r.sink.notes.size() == 1 && r.sink.notes[0] == custom);
    CHECK(r.performer.isSounding(500));
    r.performer.keyUp('q');
    CHECK((r.sink.take() == Log{"on 16", "off 16", "on 500", "off 500"}));
}

void testDiamondGaps() {
    // Gliss holds the note across the Diamond layout's gaps.
    Tuning t;
    t.originOctave = 1;   // A1: these numbers assume it
    t.edo = 19;
    DiamondLayout d(t);
    Recorder sink;
    Performer p(sink);
    p.setLayout(&d);
    const Key k = *d.keyForStep(0);
    p.pointerDown(1, k.center);
    p.pointerMove(1, {k.center.x, k.center.y + k.halfHeight + 5});   // below the keys
    CHECK((sink.take() == Log{"on 0"}));
    CHECK(p.isSounding(0));
}

} // namespace

int main() {
    testTapAndGliss();
    testMultitouchAndSharedSteps();
    testLatch();
    testGlide();
    testFixedPerNote();
    testFixedChannel();
    testMixedScopes();
    testStrikeVelocity();
    testKeysAndRelease();
    testNotesPassThrough();
    testDiamondGaps();
    return kedo::test::finish("Performer");
}
