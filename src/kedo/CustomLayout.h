#pragma once

#include <cstddef>
#include <vector>

#include "kedo/KeyStyle.h"
#include "kedo/Layout.h"

// Custom layouts (decisions.md, "Custom layout"): keys the player places, each
// with its own pitch, note number and look.
namespace kedo {

// A custom key's pitch, in 12-EDO semitones above MIDI note 0: `degree` steps
// of its EDO above the layout's origin note in the key's octave.
double customPitch(const CustomKey& key, const Tuning& tuning);

// What `key` shows for a label of `style`: `text` for LabelStyle::Text,
// otherwise its nearest 12-EDO note's name (with its octave or not), its
// degree counted from 1, or its frequency in Hz to a tenth.
std::string labelText(const CustomKey& key, LabelStyle style, const std::string& text, const Tuning& tuning);

// A layout of CustomKeys. Each becomes a Key carrying its Note: keys of one
// note number, group and channel are one note (as Tiled's repeated steps),
// and a key is playable while its pitch is within MIDI's range. Later keys
// draw on top, and are found first.
class CustomLayout final : public Layout {
public:
    static constexpr double kKeySize = 60;   // the standard diamond's side

    CustomLayout(const std::vector<CustomKey>& keys, const Tuning& tuning);

    Rect bounds() const override { return bounds_; }
    std::vector<Key> keysIn(const Rect& area) const override;
    std::optional<Key> keyAt(Point p) const override;
    std::optional<Key> keyForStep(int step) const override;   // none: custom keys have no steps

    const std::vector<Key>& keys() const { return keys_; }

private:
    std::vector<Key> keys_;
    Rect bounds_;
};

// The note number to suggest for keys[index] (decisions.md, "Custom layout"):
// another key's on the same group and channel if it has the same pitch (the
// same note); otherwise the nearest free one, the nearest 12-EDO note unless
// a key there has it, then the unused number closest to the pitch (the lower
// on a tie). Its nearest 12-EDO note if all 128 are taken.
int suggestNumber(const std::vector<CustomKey>& keys, std::size_t index, const Tuning& tuning);

// Every playable key of a generated `layout` (made by makeLayout from `kind`,
// Piano or Tiled, and `tuning`) as a custom key: its place, size, look,
// pitch and note number, so the custom layout plays exactly what `layout`
// did. The 12-EDO piano's keys are arranged as the diamond piano's (black
// and white diamonds, the black half a diamond up), as custom keys are all
// diamonds.
std::vector<CustomKey> convertToCustom(const Layout& layout, const Tuning& tuning, LayoutKind kind,
                                       TileColor tileColor);

} // namespace kedo
