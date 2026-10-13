// The three layouts: positions, hit-testing, and the Tiled lattice's isomorphism.

#include <cmath>
#include <map>
#include <set>

#include "Check.h"
#include "kedo/CustomLayout.h"
#include "kedo/Layout.h"

using namespace kedo;
using kedo::test::near;

namespace {

const double kHalfDiag = 60 * std::sqrt(2.0) / 2;

// Same-size diamonds overlap only if their centres are closer than a full
// diagonal in the taxicab metric.
bool overlap(const Key& a, const Key& b) {
    return std::abs(a.center.x - b.center.x) + std::abs(a.center.y - b.center.y) < 2 * kHalfDiag - 1e-6;
}

// Every key is found at its own centre, and keys don't overlap.
void checkConsistent(const Layout& layout) {
    const auto keys = layout.keysIn(layout.bounds());
    CHECK(!keys.empty());
    for (const auto& k : keys) {
        const auto hit = layout.keyAt(k.center);
        CHECK(hit && hit->id() == k.id());
    }
}

// Every key carries its tuning's note for its step.
void checkNotes(const Layout& layout, const Tuning& t) {
    for (const auto& k : layout.keysIn(layout.bounds())) CHECK(k.note == t.note(k.step));
}

void testPiano() {
    Tuning t;   // 12-EDO from A1
    t.originOctave = 1;
    PianoLayout piano(t, 24, 108);   // C1..C8
    CHECK(near(piano.bounds().w, 50 * PianoLayout::kWhiteWidth));
    checkConsistent(piano);
    checkNotes(piano, t);
    // At 12-EDO the note is plain: its number is its pitch.
    const auto a4 = piano.keyForStep(36);
    CHECK(a4 && a4->note.number == 69 && !a4->note.needsPitchAttribute());

    // C1 is the first white key, two octaves and a third below A1... 9 steps.
    const auto c1 = piano.keyAt({PianoLayout::kWhiteWidth / 2, PianoLayout::kWhiteHeight - 1});
    CHECK(c1 && c1->step == -9 && c1->shade == KeyShade::White);

    // Black keys win where they overlap the whites: C#1 starts 0.6 white keys in.
    const auto cs1 = piano.keyAt({PianoLayout::kWhiteWidth * 0.8, 10});
    CHECK(cs1 && cs1->step == -8 && cs1->shade == KeyShade::Black);
    // Below the black key, the white one.
    const auto d1 = piano.keyAt({PianoLayout::kWhiteWidth * 1.2, PianoLayout::kWhiteHeight - 1});
    CHECK(d1 && d1->step == -7);

    // A low black key moves down to a white one.
    PianoLayout fromBlack(t, 25, 36);
    CHECK(fromBlack.keyAt({1, PianoLayout::kWhiteHeight - 1})->step == 24 - 33);

    // Drawing order: whites, then blacks on top.
    const auto keys = piano.keysIn(piano.bounds());
    bool seenBlack = false, whiteAfterBlack = false;
    for (const auto& k : keys) {
        if (k.shade == KeyShade::Black) seenBlack = true;
        else if (seenBlack) whiteAfterBlack = true;
    }
    CHECK(!whiteAfterBlack);
}

void testDiamondSeventeen() {
    Tuning t;
    t.edo = 17;   // from A1, as KeyEDO's screenshot
    DiamondLayout d(t);
    checkConsistent(d);
    checkNotes(d, t);
    CHECK(d.keyForStep(1)->note.needsPitchAttribute());

    // Steps 0 and 1 are in A's column (a 17-EDO step is 70.6 cents), stacked.
    const auto s0 = d.keyForStep(0), s1 = d.keyForStep(1), s2 = d.keyForStep(2);
    CHECK(s0 && s1 && s2);
    CHECK(s0->col == 0 && s0->row == 0 && s1->col == 0 && s1->row == 1);
    CHECK(near(s1->center.x, s0->center.x));
    CHECK(near(s0->center.y - s1->center.y, 2 * kHalfDiag));
    // Step 2 (141 cents) is in A#'s column: black, half a diamond right and up,
    // sharing an edge with step 0.
    CHECK(s2->col == 1 && s2->shade == KeyShade::Black);
    CHECK(near(s2->center.x - s0->center.x, kHalfDiag));
    CHECK(near(s0->center.y - s2->center.y, kHalfDiag));
}

void testDiamondGaps() {
    Tuning t;
    t.edo = 19;
    t.originPitchClass = 0;   // from C, so E is column 4 and F column 5
    DiamondLayout d(t);
    double eX = 0, fX = 0, dX = 0;
    for (const auto& k : d.keysIn(d.bounds())) {
        if (k.col == 2) dX = k.center.x;
        if (k.col == 4) eX = k.center.x;
        if (k.col == 5) fX = k.center.x;
    }
    // D to E skips over D#: a white key's width. E to F is the same, by the gap.
    CHECK(near(eX - dX, 2 * kHalfDiag));
    CHECK(near(fX - eX, 2 * kHalfDiag));
}

void testDiamondAllEdos() {
    for (int edo = Tuning::kMinEdo; edo <= Tuning::kMaxEdo; ++edo) {
        if (edo == 12) continue;
        for (int origin : {0, 4, 9, 11}) {
            Tuning t;
            t.edo = edo;
            t.originPitchClass = origin;
            DiamondLayout d(t);
            checkConsistent(d);
            const auto keys = d.keysIn(d.bounds());
            CHECK(int(keys.size()) == t.lastStep() - t.firstStep() + 1);
            for (size_t i = 0; i < keys.size(); ++i)
                for (size_t j = i + 1; j < keys.size(); ++j) CHECK(!overlap(keys[i], keys[j]));
        }
    }
}

void testTiledIsomorphic() {
    Tuning t;
    t.edo = 17;
    const Generator wh{"Wicki-Hayden", 3, 10};
    TiledLayout tiled(t, wh, 8);
    checkConsistent(tiled);
    checkNotes(tiled, t);
    // Two tiles on one step carry one note.
    CHECK(tiled.keyAtLattice(10, 0).note == tiled.keyAtLattice(0, 3).note);

    // The same interval in the same direction from every tile, whatever its row.
    for (int row = 0; row < 7; ++row) {
        for (int col = -5; col < 5; ++col) {
            const Key k = tiled.keyAtLattice(col, row);
            const Key right = tiled.keyAtLattice(col + 1, row);
            const Key upRight = tiled.keyAtLattice(col, row + 1);
            const Key upLeft = tiled.keyAtLattice(col - 1, row + 1);
            CHECK(right.step - k.step == 3);
            CHECK(upRight.step - k.step == 10);
            CHECK(upLeft.step - k.step == 7);
            CHECK(near(right.center.x - k.center.x, 2 * kHalfDiag));
            CHECK(near(upRight.center.x - k.center.x, kHalfDiag));
            CHECK(near(upLeft.center.x - k.center.x, -kHalfDiag));
            CHECK(near(k.center.y - upRight.center.y, kHalfDiag));
            CHECK(!overlap(k, right));
            CHECK(!overlap(k, upRight));
            CHECK(!overlap(k, upLeft));
            // A point just inside a tile's corner finds that tile.
            const Point nearTop{k.center.x, k.center.y - kHalfDiag + 2};
            CHECK(tiled.keyAt(nearTop)->id() == k.id());
        }
    }
    // Outside the rows, nothing.
    CHECK(!tiled.keyAt({0, -kHalfDiag * 2}));
    CHECK(!tiled.keyAt({0, 8 * kHalfDiag + kHalfDiag * 2}));
}

void testTiledRange() {
    Tuning t;
    t.edo = 17;
    t.originOctave = -1;   // so the lowest key, A-1, is the origin, step 0
    TiledLayout tiled(t, {"Wicki-Hayden", 3, 10}, 8);
    // Steps outside the tuning are drawn, but unplayable.
    CHECK(!tiled.keyAtLattice(-1, 0).playable);
    CHECK(tiled.keyAtLattice(0, 0).playable);
    CHECK(tiled.keyAtLattice(42, 0).playable);    // 126
    CHECK(!tiled.keyAtLattice(43, 0).playable);   // 129: past the 128 note numbers
    // keysIn returns every key in the area, playable or not.
    const auto keys = tiled.keysIn(tiled.bounds());
    std::set<int> steps;
    for (const auto& k : keys)
        if (k.playable) steps.insert(k.step);
    CHECK(int(steps.size()) == t.lastStep() + 1);   // every step reachable
    // keyForStep: the lowest row's.
    const auto k = tiled.keyForStep(13);
    CHECK(k && k->row == 1 && k->col == 1);
}

// Octave columns (docs/decisions.md): each column one degree in every
// octave, two octaves of columns, from the origin note in octave -1.
void testOctaveColumns() {
    // The author's examples, at 12-EDO with an A origin.
    {
        Tuning t;   // A0
        OctaveLayout oct(t);
        CHECK(oct.columns() == 24);
        const auto named = [&](const char* pc, int octave) {
            const int midi = (octave + 1) * 12 + pitchClassOf(pc);
            return oct.keyForStep(midi - t.originMidiNote());
        };
        const auto a1lowest = oct.keysIn(oct.bounds());
        std::map<std::pair<int, int>, Key> at;   // (column, level)
        for (const auto& k : a1lowest) at[{k.col, k.row}] = k;
        const auto lowest = named("A", -1);
        CHECK(lowest && lowest->col == 0 && lowest->row == 0 && lowest->note.number == 9);
        CHECK(lowest && near(lowest->center.x, kHalfDiag) && near(lowest->center.y, oct.bounds().bottom() - kHalfDiag));
        for (const auto& k : a1lowest) CHECK(k.step >= lowest->step);   // nothing below A-1
        CHECK((at[{0, 1}].step == named("A", 0)->step));   // straight up from A-1 is A0
        const auto a6 = named("A", 6), as6 = named("A#", 6), b6 = named("B", 6);
        CHECK(a6 && as6 && b6);
        CHECK(as6->col == a6->col + 1 && as6->row == a6->row);       // up-right
        CHECK(near(as6->center.y, a6->center.y - kHalfDiag));
        CHECK(b6->col == a6->col + 2 && near(b6->center.y, a6->center.y));   // right
        // After G#1 (the first G# column's) comes A1, in the second octave of columns.
        const Key gs1 = at[{11, 1}];
        CHECK(gs1.step == named("G#", 1)->step);
        bool found = false;
        for (const auto& k : a1lowest)
            if (k.col == 12 && k.step == gs1.step + 1) {
                found = true;
                CHECK(near(k.center.y, gs1.center.y + kHalfDiag));   // down-right of it
            }
        CHECK(found);
    }

    for (int edo = Tuning::kMinEdo; edo <= Tuning::kMaxEdo; ++edo) {
        for (int originOctave : {-1, 0, 1, 4}) {
            Tuning t;
            t.edo = edo;
            t.originPitchClass = 0;   // C
            t.originOctave = originOctave;
            const auto presets = generatorPresets(edo);
            CHECK(presets.back().name == "Octave columns" && presets.back().octaveColumns);   // last: KeyEDO's indexes stay
            LayoutConfig c;
            c.kind = LayoutKind::Tiled;
            c.generatorIndex = int(presets.size()) - 1;
            const auto made = makeLayout(c, t);
            const auto* oct = dynamic_cast<OctaveLayout*>(made.get());
            CHECK(oct != nullptr);
            if (!oct) continue;
            CHECK(oct->columns() == 2 * edo);
            checkConsistent(*oct);

            const auto keys = oct->keysIn(oct->bounds());
            std::map<std::pair<int, int>, Key> at;
            for (const auto& k : keys) at[{k.col, k.row}] = k;
            CHECK(int(keys.size()) == oct->columns() * oct->levels());
            for (const auto& [pos, k] : at) {
                const auto [col, row] = pos;
                CHECK(k.step == t.lowestStep() + col + edo * row);
                CHECK(k.playable == t.isPlayable(k.step));
                if (const auto up = at.find({col, row + 1}); up != at.end()) {
                    CHECK(up->second.step - k.step == edo);              // straight up, an octave
                    CHECK(near(up->second.center.x, k.center.x));
                }
                if (const auto right = at.find({col + 2, row}); right != at.end())
                    CHECK(right->second.step - k.step == 2 && near(right->second.center.y, k.center.y));
                // Up-right: the next degree, from an even column; from an odd
                // one, the next degree an octave up.
                if (const auto ur = at.find({col + 1, row + (col % 2)}); ur != at.end()) {
                    CHECK(ur->second.step - k.step == 1 + (col % 2) * edo);
                    CHECK(near(ur->second.center.y, k.center.y - kHalfDiag));
                }
            }
            // The bottom left is C-1, the origin note in octave -1; every
            // playable step is there.
            CHECK((at[{0, 0}].step == t.lowestStep()));
            CHECK(t.label(t.lowestStep()).pitchClass == 0 && t.label(t.lowestStep()).octave == -1);
            std::set<int> steps;
            for (const auto& k : keys)
                if (k.playable) steps.insert(k.step);
            CHECK(int(steps.size()) == t.lastStep() - std::max(t.firstStep(), t.lowestStep()) + 1);
        }
    }
}

// Tiled starts from the origin note in octave -1 too.
void testTiledLowest() {
    for (int edo : {12, 17, 31}) {
        Tuning t;
        t.edo = edo;
        for (const auto& g : generatorPresets(edo)) {
            if (g.octaveColumns) continue;
            TiledLayout tiled(t, g, 8);
            const auto k = tiled.keyForStep(t.lowestStep());
            CHECK(k && k->row == 0 && k->col == 0);
            for (const auto& key : tiled.keysIn(tiled.bounds())) {
                if (key.step < t.lowestStep()) CHECK(!key.playable);
                if (edo != 12 && key.step < 0) CHECK(!key.playable);   // below the origin: no note number
            }
            // At 12-EDO, A-1 (note 9) is playable and G#-1 isn't shown.
            if (edo == 12) CHECK(k && k->playable && k->note.number == 9);
        }
    }
}

// Custom layouts (decisions.md, "Custom layout").

// Converting keeps every playable key's place, shape and note: the custom
// layout plays exactly what the original did, and keys that shared a note
// still do.
void testConvertToCustom() {
    struct Case { int edo; LayoutKind kind; int preset; int originOctave; };
    std::vector<Case> cases = {{12, LayoutKind::Piano, 0, 0}, {17, LayoutKind::Piano, 0, 1},
                               {31, LayoutKind::Piano, 0, 0}, {5, LayoutKind::Piano, 0, 2}};
    for (int edo : {12, 19, 31})
        for (int i = 0; i < int(generatorPresets(edo).size()); ++i) cases.push_back({edo, LayoutKind::Tiled, i, 0});
    for (const auto& c : cases) {
        Tuning t;
        t.edo = c.edo;
        t.originOctave = c.originOctave;
        LayoutConfig config;
        config.kind = c.kind;
        config.generatorIndex = c.preset;
        const auto original = makeLayout(config, t);
        config.customKeys = convertToCustom(*original, t, c.kind, TileColor::Note);
        config.kind = LayoutKind::Custom;
        const auto made = makeLayout(config, t);
        const auto* custom = dynamic_cast<CustomLayout*>(made.get());
        CHECK(custom != nullptr);
        if (!custom) continue;

        // The 12-EDO piano becomes diamonds, in the diamond piano's places:
        // each piano key's note on one diamond, of its shade.
        if (c.kind == LayoutKind::Piano && c.edo == 12) {
            std::map<int, const Key*> byNumber;
            for (const Key& k : custom->keys()) {
                CHECK(k.shape == KeyShape::Diamond && k.playable);
                CHECK(byNumber.emplace(k.note.number, &k).second);   // one each
            }
            int keys = 0;
            for (const Key& k : original->keysIn(original->bounds())) {
                ++keys;
                const auto it = byNumber.find(k.note.number);
                CHECK(it != byNumber.end());
                if (it == byNumber.end()) continue;
                CHECK(it->second->shade == k.shade);
                CHECK(std::abs(it->second->note.pitch - k.note.pitch) < 1e-9);
            }
            CHECK(keys == int(custom->keys().size()));
            checkConsistent(*custom);
            continue;
        }

        std::map<int, int> idFor;   // the original's note ids -> the custom's
        int playable = 0;
        for (const Key& k : original->keysIn(original->bounds())) {
            if (!k.playable) continue;
            ++playable;
            const auto ck = custom->keyAt(k.center);
            CHECK(ck.has_value());
            if (!ck) continue;
            CHECK(ck->playable && ck->shape == k.shape && ck->shade == k.shade);
            CHECK(near(ck->halfWidth, k.halfWidth) && near(ck->halfHeight, k.halfHeight));
            CHECK(ck->note.number == k.note.number && ck->note.group == k.note.group &&
                  ck->note.channel == k.note.channel);
            CHECK(std::abs(ck->note.pitch - k.note.pitch) < 1e-9);
            CHECK(ck->note.needsPitchAttribute() == k.note.needsPitchAttribute());
            const auto [it, added] = idFor.emplace(k.note.id, ck->note.id);
            CHECK(it->second == ck->note.id);   // one note in, one note out
        }
        CHECK(int(custom->keys().size()) == playable);
        std::set<int> customIds;
        for (const auto& [from, to] : idFor) customIds.insert(to);
        CHECK(customIds.size() == idFor.size());   // and different notes stay different
        checkConsistent(*custom);
    }
}

void testCustomPitch() {
    Tuning t;   // origin A
    CustomKey k;
    k.edo = 17;
    k.octave = 3;
    k.degree = 5;   // five 17-EDO steps above A3 (57)
    CHECK(near(customPitch(k, t), 57 + 12.0 * 5 / 17));
    t.edo = 31;     // the layout's EDO doesn't change a key's pitch
    CHECK(near(customPitch(k, t), 57 + 12.0 * 5 / 17));
    t.originPitchClass = 0;   // its origin note does: degree 0 is C3
    CHECK(near(customPitch(k, t), 48 + 12.0 * 5 / 17));

    // A key whose pitch is past MIDI's range is drawn, unplayable.
    CustomKey high;
    high.octave = 10;
    high.degree = 0;   // A10, 141 semitones
    const CustomLayout l({k, high}, Tuning{});
    CHECK(l.keys()[0].playable && !l.keys()[1].playable);
    CHECK(!l.keyForStep(0));
}

void testSuggestNumber() {
    Tuning t;
    t.originPitchClass = 0;   // C
    auto key = [](int edo, int octave, int degree, int number) {
        CustomKey k;
        k.edo = edo;
        k.octave = octave;
        k.degree = degree;
        k.number = number;
        return k;
    };
    // Two 31-EDO keys by C4: the first has 60, so the second gets the free
    // number nearest its pitch (60.39: 61).
    std::vector<CustomKey> keys = {key(31, 4, 0, 60), key(31, 4, 1, 0)};
    CHECK(suggestNumber(keys, 0, t) == 60);
    CHECK(suggestNumber(keys, 1, t) == 61);
    // A key of the same pitch is the same note: the same number.
    keys.push_back(key(12, 4, 0, 0));
    CHECK(suggestNumber(keys, 2, t) == 60);
    // Another channel doesn't compete.
    keys[1].channel = 5;
    CHECK(suggestNumber(keys, 1, t) == 60);
    // Below the pitch when that's nearer: 31-EDO degree 30 of C4 is 71.61 -> 72, taken -> 71.
    keys = {key(12, 4, 12, 72), key(31, 4, 30, 0)};
    CHECK(suggestNumber(keys, 1, t) == 71);
    // All taken: the nearest 12-EDO note anyway.
    keys.clear();
    for (int n = 0; n < 128; ++n) keys.push_back(key(1, 20, n, n));   // numbers 0-127, pitches far above
    keys.push_back(key(12, 4, 3, 0));                                   // D#4, 63
    CHECK(suggestNumber(keys, 128, t) == 63);
}

// Overlapping keys: the later one is on top, and found first.
void testCustomOverlap() {
    CustomKey a, b;
    a.center = {0, 0};
    b.center = {20, 0};
    b.number = 61;
    const CustomLayout l({a, b}, Tuning{});
    CHECK(l.keyAt({10, 0})->note.number == 61);
    CHECK(l.keyAt({-30, 0})->note.number == 60);
    CHECK(!l.keyAt({200, 0}));
}

void testMakeLayout() {
    Tuning t;
    LayoutConfig c;
    CHECK(dynamic_cast<PianoLayout*>(makeLayout(c, t).get()));
    t.edo = 19;
    CHECK(dynamic_cast<DiamondLayout*>(makeLayout(c, t).get()));
    c.kind = LayoutKind::Tiled;
    CHECK(dynamic_cast<TiledLayout*>(makeLayout(c, t).get()));
    c.generatorIndex = 99;   // out of range: the first preset
    const auto tiled = makeLayout(c, t);
    CHECK(static_cast<TiledLayout*>(tiled.get())->generator() == generatorPresets(19)[0]);
}

} // namespace

int main() {
    testPiano();
    testDiamondSeventeen();
    testDiamondGaps();
    testDiamondAllEdos();
    testTiledIsomorphic();
    testTiledRange();
    testOctaveColumns();
    testTiledLowest();
    testConvertToCustom();
    testCustomPitch();
    testSuggestNumber();
    testCustomOverlap();
    testMakeLayout();
    return kedo::test::finish("Layout");
}
