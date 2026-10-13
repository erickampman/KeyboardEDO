#include "kedo/Layout.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace kedo {

namespace {

const double kSqrt2 = std::sqrt(2.0);

int floorDiv(int a, int b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }
int ceilDiv(int a, int b) { return -floorDiv(-a, b); }

} // namespace

Rect Rect::unite(const Rect& r) const {
    if (w <= 0 || h <= 0) return r;
    if (r.w <= 0 || r.h <= 0) return *this;
    const double l = std::min(x, r.x), t = std::min(y, r.y);
    return {l, t, std::max(right(), r.right()) - l, std::max(bottom(), r.bottom()) - t};
}

bool Key::contains(Point p) const {
    const double dx = std::abs(p.x - center.x), dy = std::abs(p.y - center.y);
    if (shape == KeyShape::Diamond) return dx + dy <= halfWidth;
    return dx <= halfWidth && dy <= halfHeight;
}

// --- PianoLayout ----------------------------------------------------------------

PianoLayout::PianoLayout(const Tuning& tuning, int lowest, int highest) {
    // Position of each white key within its octave, and each black key's left
    // edge in white-key widths from the octave's C (KeyboardPianoView.swift).
    static constexpr int kWhiteIndex[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    static constexpr double kBlackEdge[12] = {0, 0.60, 0, 1.65, 0, 0, 3.55, 0, 4.60, 0, 5.70, 0};

    lowest = std::clamp(lowest, 0, 127);
    highest = std::clamp(highest, lowest, 127);
    if (isBlackPitchClass(lowest % 12) && lowest > 0) --lowest;
    if (isBlackPitchClass(highest % 12) && highest > lowest) --highest;

    auto octaveX = [](int note) { return (note / 12) * 7 * kWhiteWidth; };
    const double originX = octaveX(lowest) + kWhiteIndex[lowest % 12] * kWhiteWidth;

    for (int note = lowest; note <= highest; ++note) {
        const int pc = note % 12;
        Key k;
        k.step = note - tuning.originMidiNote();
        k.note = tuning.note(k.step);
        k.playable = tuning.isPlayable(k.step);
        k.shape = KeyShape::Rectangle;
        if (!isBlackPitchClass(pc)) {
            const double left = octaveX(note) + kWhiteIndex[pc] * kWhiteWidth - originX;
            k.shade = KeyShade::White;
            k.center = {left + kWhiteWidth / 2, kWhiteHeight / 2};
            k.halfWidth = kWhiteWidth / 2;
            k.halfHeight = kWhiteHeight / 2;
            k.col = int(whites_.size());
            k.row = 0;
            whites_.push_back(k);
        } else {
            const double left = octaveX(note) + kBlackEdge[pc] * kWhiteWidth - originX;
            k.shade = KeyShade::Black;
            k.center = {left + kBlackWidth / 2, kBlackHeight / 2};
            k.halfWidth = kBlackWidth / 2;
            k.halfHeight = kBlackHeight / 2;
            k.col = int(whites_.size());   // the white key to its right
            k.row = 1;
            blacks_.push_back(k);
        }
    }
    bounds_ = {0, 0, whites_.size() * kWhiteWidth, kWhiteHeight};
}

std::vector<Key> PianoLayout::keysIn(const Rect& area) const {
    std::vector<Key> out;
    for (const auto* keys : {&whites_, &blacks_})
        for (const auto& k : *keys)
            if (k.bounds().intersects(area)) out.push_back(k);
    return out;
}

std::optional<Key> PianoLayout::keyAt(Point p) const {
    for (const auto* keys : {&blacks_, &whites_})
        for (const auto& k : *keys)
            if (k.contains(p)) return k;
    return std::nullopt;
}

std::optional<Key> PianoLayout::keyForStep(int step) const {
    for (const auto* keys : {&whites_, &blacks_})
        for (const auto& k : *keys)
            if (k.step == step) return k;
    return std::nullopt;
}

// --- DiamondLayout ----------------------------------------------------------------

DiamondLayout::DiamondLayout(const Tuning& tuning) {
    const double h = kKeySize * kSqrt2 / 2;   // half-diagonal

    // Each step goes in the column of the 12-EDO semitone it falls in. A step
    // exactly on a semitone (24-EDO's even steps) starts that semitone's column.
    std::map<int, std::vector<int>> columns;   // semitone above the origin -> steps
    for (int step = tuning.firstStep(); step <= tuning.lastStep(); ++step) {
        const int semitone = int(std::floor(12.0 * step / tuning.edo + 1e-9));
        columns[semitone].push_back(step);
    }
    if (columns.empty()) return;

    // Columns advance by half a diamond, plus half again before F and C, so E-F
    // and B-C touch corner to corner like adjacent white keys. Semitones with no
    // step (EDOs under 12) keep their place, so the piano's shape survives.
    const int lastColumn = columns.rbegin()->first;
    double x = h;
    double maxTop = 0;   // highest point above the baseline
    for (int col = 0; col <= lastColumn; ++col) {
        const int pc = (tuning.originPitchClass + col) % 12;
        if (col > 0) x += (pc == 5 || pc == 0) ? 2 * h : h;
        const bool black = isBlackPitchClass(pc);
        const auto it = columns.find(col);
        if (it == columns.end()) continue;
        for (int i = 0; i < int(it->second.size()); ++i) {
            Key k;
            k.step = it->second[i];
            k.note = tuning.note(k.step);
            k.playable = true;
            k.shape = KeyShape::Diamond;
            k.shade = black ? KeyShade::Black : KeyShade::White;
            k.halfWidth = k.halfHeight = h;
            k.col = col;
            k.row = i;
            const double up = (black ? h : 0) + i * 2 * h;   // centre above the baseline
            k.center = {x, -up};
            maxTop = std::max(maxTop, up + h);
            keys_.push_back(k);
        }
    }
    // y down from the top of the tallest column.
    for (auto& k : keys_) k.center.y += maxTop;
    bounds_ = {0, 0, x + h, maxTop + h};
}

std::vector<Key> DiamondLayout::keysIn(const Rect& area) const {
    std::vector<Key> out;
    for (const auto& k : keys_)
        if (k.bounds().intersects(area)) out.push_back(k);
    return out;
}

std::optional<Key> DiamondLayout::keyAt(Point p) const {
    for (const auto& k : keys_)
        if (k.contains(p)) return k;
    return std::nullopt;
}

std::optional<Key> DiamondLayout::keyForStep(int step) const {
    for (const auto& k : keys_)
        if (k.step == step) return k;
    return std::nullopt;
}

// --- TiledLayout ----------------------------------------------------------------

TiledLayout::TiledLayout(const Tuning& tuning, const Generator& generator, int rows)
    : tuning_(tuning), generator_(generator), rows_(std::max(1, rows)), lowest_(tuning.lowestStep()) {
    generator_.right = std::max(1, generator_.right);
    const double h = kKeySize * kSqrt2 / 2;
    const int r1 = generator_.right, ur = generator_.upRight;
    // The keys shown run from lowest_ up; at EDOs other than 12, those below
    // the origin are among them, but grey.
    for (int row = 0; row < rows_; ++row) {
        const int lo = ceilDiv(-row * ur, r1);
        const int hi = floorDiv(tuning_.lastStep() - lowest_ - row * ur, r1);
        if (lo > hi) continue;
        const Point a = center(lo, row), b = center(hi, row);
        bounds_ = bounds_.unite({a.x - h, a.y - h, b.x - a.x + 2 * h, 2 * h});
    }
}

Point TiledLayout::center(int col, int row) const {
    const double h = kKeySize * kSqrt2 / 2;
    return {(2 * col + row) * h, (rows_ - row) * h};
}

Key TiledLayout::keyAtLattice(int col, int row) const {
    const double h = kKeySize * kSqrt2 / 2;
    Key k;
    k.step = lowest_ + col * generator_.right + row * generator_.upRight;
    k.note = tuning_.note(k.step);
    k.playable = k.step >= lowest_ && tuning_.isPlayable(k.step);
    k.shape = KeyShape::Diamond;
    k.shade = KeyShade::White;
    k.center = center(col, row);
    k.halfWidth = k.halfHeight = h;
    k.col = col;
    k.row = row;
    return k;
}

std::vector<Key> TiledLayout::keysIn(const Rect& area) const {
    const double h = kKeySize * kSqrt2 / 2;
    std::vector<Key> out;
    for (int row = 0; row < rows_; ++row) {
        const double y = center(0, row).y;
        if (y + h <= area.y || y - h >= area.bottom()) continue;
        const int lo = int(std::ceil(((area.x - h) / h - row) / 2));
        const int hi = int(std::floor(((area.right() + h) / h - row) / 2));
        for (int col = lo; col <= hi; ++col) out.push_back(keyAtLattice(col, row));
    }
    return out;
}

std::optional<Key> TiledLayout::keyAt(Point p) const {
    // In the lattice's own diagonal axes a tile is a unit square:
    //   (x - yUp) / 2h = col,  (x + yUp) / 2h = col + row
    // where yUp is the height above row 0's centre.
    const double h = kKeySize * kSqrt2 / 2;
    const double yUp = rows_ * h - p.y;
    const int col = int(std::lround((p.x - yUp) / (2 * h)));
    const int row = int(std::lround((p.x + yUp) / (2 * h))) - col;
    if (row < 0 || row >= rows_) return std::nullopt;
    return keyAtLattice(col, row);
}

std::optional<Key> TiledLayout::keyForStep(int step) const {
    // The lowest row that has it.
    for (int row = 0; row < rows_; ++row) {
        const int rest = step - lowest_ - row * generator_.upRight;
        if (rest % generator_.right == 0) return keyAtLattice(rest / generator_.right, row);
    }
    return std::nullopt;
}

// --- OctaveLayout --------------------------------------------------------------

OctaveLayout::OctaveLayout(const Tuning& tuning) {
    const double h = kKeySize * kSqrt2 / 2;   // half-diagonal
    const int lowest = tuning.lowestStep(), edo = tuning.edo;
    columns_ = 2 * edo;
    // Levels enough for the first column to reach the last step; the columns
    // after it run out sooner, and their top tiles are grey.
    levels_ = std::max(1, floorDiv(tuning.lastStep() - lowest, edo) + 1);

    const double top = 2 * h * levels_ + h;   // the odd columns' top corner, the highest point
    for (int col = 0; col < columns_; ++col) {
        for (int level = 0; level < levels_; ++level) {
            Key k;
            k.step = lowest + col + edo * level;
            k.note = tuning.note(k.step);
            k.playable = tuning.isPlayable(k.step);
            k.shape = KeyShape::Diamond;
            k.shade = KeyShade::White;
            k.halfWidth = k.halfHeight = h;
            k.col = col;
            k.row = level;
            const double up = (col % 2 ? 2 * h : h) + level * 2 * h;   // centre above the bottom
            k.center = {h + col * h, top - up};
            keys_.push_back(k);
        }
    }
    bounds_ = {0, 0, (columns_ + 1) * h, top};
}

std::vector<Key> OctaveLayout::keysIn(const Rect& area) const {
    std::vector<Key> out;
    for (const auto& k : keys_)
        if (k.bounds().intersects(area)) out.push_back(k);
    return out;
}

std::optional<Key> OctaveLayout::keyAt(Point p) const {
    for (const auto& k : keys_)
        if (k.contains(p)) return k;
    return std::nullopt;
}

std::optional<Key> OctaveLayout::keyForStep(int step) const {
    std::optional<Key> found;
    for (const auto& k : keys_)
        if (k.step == step && (!found || k.row < found->row)) found = k;
    return found;
}

// --- makeLayout ---------------------------------------------------------------------

std::unique_ptr<Layout> makeLayout(const LayoutConfig& config, const Tuning& tuning) {
    if (config.kind == LayoutKind::Tiled) {
        const auto presets = generatorPresets(tuning.edo);
        // A preset index from another EDO may be out of range: use the first.
        const int i = config.generatorIndex;
        const auto& g = presets[i >= 0 && i < int(presets.size()) ? i : 0];
        if (g.octaveColumns) return std::make_unique<OctaveLayout>(tuning);
        return std::make_unique<TiledLayout>(tuning, g, config.tiledRows);
    }
    if (tuning.edo == 12)
        return std::make_unique<PianoLayout>(tuning, config.pianoLowest, config.pianoHighest);
    return std::make_unique<DiamondLayout>(tuning);
}

} // namespace kedo
