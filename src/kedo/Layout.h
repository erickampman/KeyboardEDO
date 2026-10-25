#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "kedo/Generators.h"
#include "kedo/Tuning.h"

// Keyboard layouts as pure geometry: where each key is and which step it
// plays. No toolkit types (the view-model rule from Umpire's decisions.md): the
// drawing layer turns Keys into shapes, and turns pointer positions into calls
// to keyAt().
//
// Units are points at zoom 1, y down, as KeyEDO's SwiftUI views: a diamond is a
// 60-point square turned 45 degrees, a white piano key 36 x 180. The view
// scales and scrolls; the layout never changes with zoom.
namespace kedo {

struct Point {
    double x = 0;
    double y = 0;
    bool operator==(const Point&) const = default;
};

struct Rect {
    double x = 0, y = 0, w = 0, h = 0;

    double right() const { return x + w; }
    double bottom() const { return y + h; }
    bool contains(Point p) const { return p.x >= x && p.x < right() && p.y >= y && p.y < bottom(); }
    bool intersects(const Rect& r) const {
        return x < r.right() && r.x < right() && y < r.bottom() && r.y < bottom();
    }
    Rect unite(const Rect& r) const;
};

enum class KeyShape { Rectangle, Diamond };

// The piano colouring. Tiled keys are all White; the view colours them by
// note or octave (TileColor).
enum class KeyShade { White, Black };

struct Key {
    Note     note;                // what it plays
    int      step     = 0;        // the Tuning step it's on, for labels and colours
    bool     playable = false;    // false: drawn, but silent (outside Tuning's range)
    KeyShape shape    = KeyShape::Rectangle;
    KeyShade shade    = KeyShade::White;
    Point    center;
    double   halfWidth  = 0;      // a diamond's half-diagonals: halfWidth == halfHeight
    double   halfHeight = 0;
    int      col = 0;             // the layout's own coordinates: lattice (Tiled),
    int      row = 0;             // column and stack position (Diamond), white-key index (Piano),
                                  // index (Custom)
    int      computerKey = 0;     // the computer key that plays it (ComputerKeys.h), 0: none

    // A custom layout's key carries its own look (KeyStyle uses these for
    // LayoutKind::Custom; the other layouts derive theirs).
    std::optional<std::uint32_t> color;   // fill, 0xRRGGBB; else by shade
    std::string label;                    // shown on the key; may be empty
    std::string sublabel;                 // smaller, under it

    Rect bounds() const {
        return {center.x - halfWidth, center.y - halfHeight, 2 * halfWidth, 2 * halfHeight};
    }
    bool contains(Point p) const;

    // Where in the key `p` is, from its top (0) to its bottom (1): strike
    // velocity is louder near the top, as KeyEDO.
    double depth(Point p) const { return (p.y - (center.y - halfHeight)) / (2 * halfHeight); }

    // A stable identity for the key on screen. Tiled repeats steps, so two keys
    // can share a step but never an id.
    long id() const { return long(row) * 100000 + col; }
};

class Layout {
public:
    virtual ~Layout() = default;

    // The extent of the playable keys.
    virtual Rect bounds() const = 0;

    // Keys that intersect `area`, in drawing order (later keys draw on top).
    virtual std::vector<Key> keysIn(const Rect& area) const = 0;

    // The key under `p`, topmost first; std::nullopt between keys.
    virtual std::optional<Key> keyAt(Point p) const = 0;

    // A key that plays `step`, if any: on Tiled, which repeats steps, the one
    // in the lowest row.
    virtual std::optional<Key> keyForStep(int step) const = 0;
};

// --- The layouts --------------------------------------------------------------

// A standard piano, for 12-EDO. Keys from `lowest` to `highest` (MIDI notes);
// `lowest` moves down to a white key if it's black.
class PianoLayout final : public Layout {
public:
    static constexpr double kWhiteWidth  = 36;
    static constexpr double kWhiteHeight = 180;
    static constexpr double kBlackWidth  = kWhiteWidth * 0.64;
    static constexpr double kBlackHeight = kWhiteHeight * 0.62;

    PianoLayout(const Tuning& tuning, int lowest, int highest);

    Rect bounds() const override { return bounds_; }
    std::vector<Key> keysIn(const Rect& area) const override;
    std::optional<Key> keyAt(Point p) const override;
    std::optional<Key> keyForStep(int step) const override;

private:
    std::vector<Key> whites_, blacks_;
    Rect bounds_;
};

// Which 12-EDO semitone's column a diamond piano step goes in: the one at or
// below it (KeyEDO's), or the nearest, so 19-EDO's D (189 cents) is in D's
// column, not C#'s. A step halfway between goes in the lower one.
enum class DiamondColumns { Below, Nearest };

// KeyEDO's diamond hybrid, for any EDO but 12: one column per 12-EDO semitone,
// holding the steps that fall in it (`columns` says which), stacked upward. Black-key columns sit half
// a diamond higher, so the columns interlock as a piano's keys do, and an extra
// half-diamond gap separates E from F and B from C.
class DiamondLayout final : public Layout {
public:
    static constexpr double kKeySize = 60;     // the square's side

    explicit DiamondLayout(const Tuning& tuning, DiamondColumns columns = DiamondColumns::Below);

    Rect bounds() const override { return bounds_; }
    std::vector<Key> keysIn(const Rect& area) const override;
    std::optional<Key> keyAt(Point p) const override;
    std::optional<Key> keyForStep(int step) const override;

private:
    std::vector<Key> keys_;
    Rect bounds_;
};

// An isomorphic lattice of diamonds. Tile (col, row) plays
//     lowest + col * generator.right + row * generator.upRight
// (lowest: Tuning::lowestStep(), the origin note in octave -1) and sits at
// x = (2 * col + row) * h, so every row is shifted half a tile right of the
// one below: moving up-right always adds upRight, and up-left always adds
// upRight - right, wherever you start. Rows run from 0 (bottom) to rows - 1.
// Columns are unbounded; tiles below `lowest` or outside the tuning's range
// are unplayable.
//
// KeyEDO alternated the shift between even and odd rows instead, so up-right
// added upRight from an even row but right + upRight from an odd one, and
// chord shapes changed with the row (docs/decisions.md, "Tiled lattice").
class TiledLayout final : public Layout {
public:
    static constexpr double kKeySize = 60;

    TiledLayout(const Tuning& tuning, const Generator& generator, int rows);

    Rect bounds() const override { return bounds_; }
    std::vector<Key> keysIn(const Rect& area) const override;
    std::optional<Key> keyAt(Point p) const override;
    std::optional<Key> keyForStep(int step) const override;

    Key keyAtLattice(int col, int row) const;
    int rows() const { return rows_; }
    const Generator& generator() const { return generator_; }

private:
    Point center(int col, int row) const;

    Tuning tuning_;
    Generator generator_;
    int rows_;
    int lowest_;
    int zCol_ = 0, zRow_ = 0;   // the tile Z plays
    Rect bounds_;
};

// Octave columns (docs/decisions.md): each column one degree in every octave,
// going up, two octaves of columns side by side. Column c at level o plays
//     lowest + c + EDO * o
// (lowest: Tuning::lowestStep()), so the zigzag along the bottom is
// chromatic, and every other column sits half a tile higher, as the diamond
// piano's black keys. Not isomorphic: up-right adds 1 from an even column but
// EDO + 1 from an odd one. Tiles without a note number are drawn, unplayable.
class OctaveLayout final : public Layout {
public:
    static constexpr double kKeySize = 60;

    explicit OctaveLayout(const Tuning& tuning);

    Rect bounds() const override { return bounds_; }
    std::vector<Key> keysIn(const Rect& area) const override;
    std::optional<Key> keyAt(Point p) const override;
    std::optional<Key> keyForStep(int step) const override;   // the lowest level's

    int columns() const { return columns_; }
    int levels() const { return levels_; }

private:
    std::vector<Key> keys_;   // by column, then level
    int columns_ = 0, levels_ = 0;
    Rect bounds_;
};

// --- Choosing one -------------------------------------------------------------

enum class LayoutKind { Piano, Tiled, Custom };

// What a custom key's label (or the line under it) shows: the text typed, or
// something worked out from its pitch when it's drawn, so it follows a pitch
// change (decisions.md, "Custom layout").
enum class LabelStyle { Text, NameOctave, Name, Degree, Frequency };

// One key of a custom layout as the player places it (decisions.md, "Custom
// layout"). Its pitch is `degree` steps of `edo` above the layout's origin
// note in `octave` (with the origin at A, 17 / 3 / 5 is five 17-EDO steps above
// A3), so keys of different EDOs can share a layout, and changing the
// layout's EDO leaves them be.
//
// Every custom key is a diamond (the author, 2026-10-13): a 12-EDO piano
// converts to the diamond piano's arrangement, keeping its black and white.
struct CustomKey {
    Point    center;
    double   halfWidth  = 0;      // half-diagonals; 0: the standard diamond's
    double   halfHeight = 0;
    KeyShade shade = KeyShade::White;
    std::optional<std::uint32_t> color;   // fill, 0xRRGGBB, instead of the shade's
    std::string label, sublabel;         // the text, for LabelStyle::Text
    LabelStyle labelStyle = LabelStyle::Text, sublabelStyle = LabelStyle::Text;

    int edo = 12, octave = 4, degree = 0;

    // The MIDI 2.0 note number. Suggested (the nearest free one: CustomLayout's
    // suggestNumber) unless `numberChosen`, when the player set it.
    int  number = 60;
    bool numberChosen = false;
    std::optional<std::uint8_t> group, channel;   // std::nullopt: the layout's

    int computerKey = 0;   // the computer key that plays it (ComputerKeys.h), 0: none

    bool operator==(const CustomKey&) const = default;
};

struct LayoutConfig {
    LayoutKind kind       = LayoutKind::Piano;
    int  generatorIndex   = 0;     // into generatorPresets(tuning.edo)
    int  tiledRows        = 8;
    int  pianoLowest      = 24;    // C1
    int  pianoHighest     = 108;   // C8
    DiamondColumns diamondColumns = DiamondColumns::Below;
    std::vector<CustomKey> customKeys;   // LayoutKind::Custom's

    bool operator==(const LayoutConfig&) const = default;
};

// Piano at 12-EDO is PianoLayout and at any other EDO DiamondLayout; Tiled is
// TiledLayout at every EDO, or OctaveLayout for the Octave columns preset;
// Custom is CustomLayout.
std::unique_ptr<Layout> makeLayout(const LayoutConfig& config, const Tuning& tuning);

} // namespace kedo
