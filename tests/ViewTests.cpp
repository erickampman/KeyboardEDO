// Viewport, KeyStyle and UmpText: what the UI uses from the core.

#include "Check.h"
#include "kedo/KeyStyle.h"
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
    k.playable = false;
    look = keyLook(k, t, LayoutKind::Tiled, TileColor::Note, false, false);
    CHECK(look.title.empty());

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

    CHECK(keyboardBackground(LayoutKind::Piano, t).r > 0.5f);
    CHECK(keyboardBackground(LayoutKind::Piano, twelve).r < 0.5f);
}

void testUmpText() {
    Tuning t;
    t.edo = 17;
    OutputConfig c;
    c.perNoteManagement = true;
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

} // namespace

int main() {
    testViewport();
    testKeyLook();
    testUmpText();
    return kedo::test::finish("View");
}
