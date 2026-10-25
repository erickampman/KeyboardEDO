// Viewport, KeyStyle and UmpText: what the UI uses from the core.

#include <algorithm>

#include "Check.h"
#include "kedo/KeyStyle.h"
#include "kedo/Scales.h"
#include "kedo/Synopsis.h"
#include "kedo/UmpOutput.h"
#include "kedo/UmpText.h"
#include "kedo/Viewport.h"

using namespace kedo;
using kedo::test::near;

namespace {

void testViewport() {
    Viewport v;
    v.screen = {10, 20, 400, 200};
    v.zoom = 2;
    v.scroll = {100, 50};
    const Point l = v.toLayout({30, 40});
    CHECK(near(l.x, 110) && near(l.y, 60));
    const Point s = v.toScreen(l);
    CHECK(near(s.x, 30) && near(s.y, 40));
    CHECK(near(v.visible().w, 200) && near(v.visible().h, 100));

    // Zooming keeps the point under the anchor still.
    v.zoomAbout({210, 120}, 1.5);
    CHECK(near(v.zoom, 3));
    const Point after = v.toLayout({210, 120});
    CHECK(near(after.x, 200) && near(after.y, 100));
    v.zoomAbout({0, 0}, 100);
    CHECK(near(v.zoom, Viewport::kMaxZoom));

    // Content smaller than the view is centred; larger is clamped to its edges.
    Viewport c;
    c.screen = {0, 0, 1000, 100};
    c.scroll = {-500, -500};
    c.clampTo({0, 0, 3000, 50});
    CHECK(near(c.scroll.x, 0));
    CHECK(near(c.scroll.y, -25));
    c.scroll.x = 5000;
    c.clampTo({0, 0, 3000, 50});
    CHECK(near(c.scroll.x, 2000));
}

void testKeyLook() {
    Tuning t;
    t.originOctave = 1;   // A1: these numbers assume it
    t.edo = 17;
    Key k;
    k.step = 17;   // A2
    k.playable = true;
    k.shape = KeyShape::Diamond;

    // Diamonds: the origin's note named, with its frequency.
    auto look = keyLook(k, t, LayoutKind::Piano, TileColor::Note, false, false);
    CHECK(look.title == "A");
    CHECK(look.detail == "110.0");
    CHECK(look.fill == (Rgb{1, 1, 1}));
    k.step = 18;
    CHECK(keyLook(k, t, LayoutKind::Piano, TileColor::Note, false, false).title.empty());

    // Tiles: name with octave, and the 1-based scale degree.
    look = keyLook(k, t, LayoutKind::Tiled, TileColor::Note, false, false);
    CHECK(look.title == "A#2");
    CHECK(look.detail == "2");
    CHECK(look.opacity == 1);
    k.playable = false;   // no note: no label, and see-through
    look = keyLook(k, t, LayoutKind::Tiled, TileColor::Note, false, false);
    CHECK(look.title.empty());
    CHECK(look.opacity < 1);

    // Pressed and latched differ from the key's own colour and from each other.
    k.playable = true;
    const auto plain = keyLook(k, t, LayoutKind::Tiled, TileColor::Off, false, false).fill;
    const auto pressed = keyLook(k, t, LayoutKind::Tiled, TileColor::Off, true, false).fill;
    const auto latched = keyLook(k, t, LayoutKind::Tiled, TileColor::Off, true, true).fill;
    CHECK(!(plain == pressed) && !(pressed == latched));

    // The piano names C.
    Tuning twelve;
    Key c4;
    c4.step = 60 - twelve.originMidiNote();
    c4.playable = true;
    CHECK(keyLook(c4, twelve, LayoutKind::Piano, TileColor::Off, false, false).title == "C4");

    // The diamonds' background sits between their black and white keys.
    const Rgb diamonds = keyboardBackground(LayoutKind::Piano, t);
    CHECK(diamonds.g > 0.3f && diamonds.g < 0.7f);
    CHECK(keyboardBackground(LayoutKind::Piano, twelve).r < 0.5f);
}

void testScaleMarks() {
    using enum ScaleMark;
    // The key is the origin note. 12-EDO from A: A major, A (step 0) the tonic.
    Tuning t;
    ScaleChoice c{.scale = "Major (Ionian)", .map = ScaleMap::Fifths};
    CHECK(ScaleChoice{}.map == ScaleMap::Nearest);   // the default
    auto marks = scaleMarks(c, t);
    CHECK((marks == std::vector<ScaleMark>{Tonic, Off, Degree, Off, Degree, Degree, Off, Degree, Off, Degree, Off,
                                          Degree}));
    // Every scale is its familiar self at 12-EDO, whichever the map.
    for (const FamiliarScale& s : familiarScales()) {
        const ScaleChoice fifths{.scale = s.name, .map = ScaleMap::Fifths}, nearest{.scale = s.name};
        const auto m = scaleMarks(fifths, t);
        CHECK(m == scaleMarks(nearest, t));
        CHECK(std::count(m.begin(), m.end(), Off) == 12 - s.count);
    }

    // 19-EDO by fifths (the fifth is 11 steps): the major scale's steps are
    // 0 3 6 8 11 14 17.
    t.edo = 19;
    marks = scaleMarks(c, t);
    CHECK(marks.size() == 19);
    for (int step : {0, 3, 6, 8, 11, 14, 17}) CHECK(marks[std::size_t(step)] != Off);
    CHECK(std::count(marks.begin(), marks.end(), Off) == 12);

    // Nearest: 22-EDO's major third by fifths is 8 steps, by nearest 7.
    t.edo = 22;
    CHECK(scaleMarks(c, t)[8] == Degree);
    c.map = ScaleMap::Nearest;
    CHECK(scaleMarks(c, t)[7] == Degree && scaleMarks(c, t)[8] == Off);

    // None, or a name not in the menu: no marks.
    CHECK(scaleMarks({}, t).empty());
    CHECK(scaleMarks({.scale = "Bebop"}, t).empty());

    // The piano's keys, rectangles and diamonds, white and black, are tinted,
    // the tonic differently; pressed still shows; the tiles aren't.
    Key k;
    k.playable = true;
    for (KeyShape shape : {KeyShape::Rectangle, KeyShape::Diamond})
        for (KeyShade shade : {KeyShade::White, KeyShade::Black}) {
            k.shape = shape;
            k.shade = shade;
            const Rgb plain = keyLook(k, t, LayoutKind::Piano, TileColor::Off, false, false).fill;
            const Rgb degree = keyLook(k, t, LayoutKind::Piano, TileColor::Off, false, false, Degree).fill;
            const Rgb tonic = keyLook(k, t, LayoutKind::Piano, TileColor::Off, false, false, Tonic).fill;
            const Rgb pressed = keyLook(k, t, LayoutKind::Piano, TileColor::Off, true, false, Tonic).fill;
            CHECK(!(plain == degree) && !(degree == tonic) && !(plain == tonic));
            CHECK(!(pressed == tonic));
        }
    CHECK(keyLook(k, t, LayoutKind::Tiled, TileColor::Off, false, false, Tonic).fill ==
          keyLook(k, t, LayoutKind::Tiled, TileColor::Off, false, false).fill);
}

void testUmpText() {
    Tuning t;
    t.originOctave = 1;   // A1: these numbers assume it
    t.edo = 17;
    OutputConfig c;
    c.perNoteManagement = true;
    c.midi1Expression = false;
    UmpOutput out(c);
    out.noteOn(t.note(17), 0xFFFF);
    out.pitchBend(17, 0.5);
    out.glide(17, t.semitones(18));
    auto m = out.take();
    CHECK(describeUmp(m[0]).find("Note Mgmt") != std::string::npos);
    CHECK(describeUmp(m[1]) == "2.0 g1  ch1  Note On      n17  v100.0%  pitch 45.000");
    CHECK(describeUmp(m[2]).find("+50.0%") != std::string::npos);
    CHECK(describeUmp(m[3]).find("Note Pitch") != std::string::npos);

    c.protocol = uw::MidiProtocol::Midi1;
    UmpOutput out1(c);
    out1.noteOn(t.note(17), 0xFFFF);
    m = out1.take();
    CHECK(describeUmp(m[0]) == "1.0 g1  ch1  Note On      n45  v127");
}

void testSynopsis() {
    const std::string dot = "  \u00B7  ";
    LayoutSetup d;   // 12-EDO piano
    CHECK(synopsis(d, {}) == "12-EDO" + dot + "Piano" + dot + "Gliss");

    d.tuning.edo = 17;
    d.tuning.originOctave = 1;
    d.layout.kind = LayoutKind::Tiled;
    d.play.strikeVelocity = true;
    LiveState live;
    live.latch = true;
    live.destination = "IAC Bus 1";
    live.publishing = true;
    CHECK(synopsis(d, live) == "17-EDO" + dot + "A1" + dot + "Tiled: Wicki-Hayden" + dot + "Gliss" + dot +
                                   "Strike Vel" + dot + "Latch" + dot + "to IAC Bus 1" + dot + "Published");

    d.layout.kind = LayoutKind::Piano;
    d.play = {};
    d.play.drag = DragBehavior::Fixed;
    d.play.horizontal = HorizontalControl::PitchBend;
    d.play.vertical = VerticalControl::Controller;
    d.play.verticalController = 74;
    live = {};
    live.destination = "Synth";
    live.destinationFound = false;
    CHECK(synopsis(d, live) == "17-EDO" + dot + "A1" + dot + "Diamond piano" + dot +
                                   "Fixed (H note bend, V note controller 74)" + dot + "to Synth (not found)");
    d.play.bendScope = ExpressionScope::Channel;
    d.play.vertical = VerticalControl::Pressure;
    CHECK(synopsis(d, {}).find("Fixed (H bend, V poly pressure)") != std::string::npos);

    // The arpeggiator, when it's on, as KeyEDO's.
    CHECK(synopsis(d, {}).find("Arp") == std::string::npos);
    d.arp = {.enabled = true, .pattern = ArpPattern::UpDown, .division = ArpDivision::EighthTriplet, .bpm = 96};
    CHECK(synopsis(d, {}).find(dot + "Arp Up-Down 1/8T 96 bpm") != std::string::npos);
    d.arp.octaves = 2;
    d.arp.hold = true;
    CHECK(synopsis(d, {}).find("Arp Up-Down 1/8T 96 bpm 2 oct hold") != std::string::npos);
}

} // namespace

int main() {
    testViewport();
    testKeyLook();
    testScaleMarks();
    testUmpText();
    testSynopsis();
    return kedo::test::finish("View");
}
