// The three layouts: positions, hit-testing, and the Tiled lattice's isomorphism.

#include <cmath>
#include <set>

#include "Check.h"
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
    TiledLayout tiled(t, {"Wicki-Hayden", 3, 10}, 8);
    // Steps outside the tuning are drawn, but unplayable.
    CHECK(!tiled.keyAtLattice(-1, 0).playable);
    CHECK(tiled.keyAtLattice(0, 0).playable);
    CHECK(!tiled.keyAtLattice(36, 0).playable);   // 108 > 106
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
    testMakeLayout();
    return kedo::test::finish("Layout");
}
