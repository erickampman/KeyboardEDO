#include "kedo/CustomLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>

namespace kedo {

namespace {

const double kHalfDiag = CustomLayout::kKeySize * std::sqrt(2.0) / 2;

int floorDiv(int a, int b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }
int mod(int a, int b) { return ((a % b) + b) % b; }

int nearestMidi(double pitch) { return std::clamp(int(std::lround(pitch)), 0, 127); }

std::uint32_t packed(const Rgb& c) {
    auto byte = [](float v) { return std::uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };
    return byte(c.r) << 16 | byte(c.g) << 8 | byte(c.b);
}

bool sameRoute(const CustomKey& a, const CustomKey& b) { return a.group == b.group && a.channel == b.channel; }

} // namespace

double customPitch(const CustomKey& key, const Tuning& tuning) {
    const int edo = std::max(1, key.edo);
    return (key.octave + 1) * 12 + tuning.originPitchClass + 12.0 * key.degree / edo;
}

std::string labelText(const CustomKey& key, LabelStyle style, const std::string& text, const Tuning& tuning) {
    const double pitch = customPitch(key, tuning);
    const StepLabel l = labelOfPitch(pitch);
    char buf[32];
    switch (style) {
    case LabelStyle::Text: return text;
    case LabelStyle::NameOctave: return pitchClassName(l.pitchClass) + std::to_string(l.octave);
    case LabelStyle::Name: return pitchClassName(l.pitchClass);
    case LabelStyle::Degree: return std::to_string(mod(key.degree, std::max(1, key.edo)) + 1);
    case LabelStyle::Frequency:
        std::snprintf(buf, sizeof buf, "%.1f", 440.0 * std::pow(2.0, (pitch - 69) / 12));
        return buf;
    }
    return text;
}

// --- CustomLayout ------------------------------------------------------------------

CustomLayout::CustomLayout(const std::vector<CustomKey>& keys, const Tuning& tuning) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const CustomKey& c = keys[i];
        Key k;
        k.note.number = std::uint8_t(std::clamp(c.number, 0, 127));
        k.note.group = c.group;
        k.note.channel = c.channel;
        k.note.pitch = customPitch(c, tuning);
        // One note per number, group and channel: an unset group or channel
        // (the layout's) counts as its own value, 16.
        k.note.id = (int(c.group.value_or(16)) * 17 + int(c.channel.value_or(16))) * 128 + k.note.number;
        k.playable = k.note.pitch >= 0 && k.note.pitch < 128 && c.number >= 0 && c.number <= 127;
        k.shape = KeyShape::Diamond;
        k.shade = c.shade;
        k.center = c.center;
        k.halfWidth = c.halfWidth > 0 ? c.halfWidth : kHalfDiag;
        k.halfHeight = c.halfHeight > 0 ? c.halfHeight : kHalfDiag;
        k.col = 0;
        k.row = int(i);
        k.color = c.color;
        k.computerKey = c.computerKey;
        k.label = labelText(c, c.labelStyle, c.label, tuning);
        k.sublabel = labelText(c, c.sublabelStyle, c.sublabel, tuning);
        bounds_ = bounds_.unite(k.bounds());
        keys_.push_back(std::move(k));
    }
}

std::vector<Key> CustomLayout::keysIn(const Rect& area) const {
    std::vector<Key> out;
    for (const auto& k : keys_)
        if (k.bounds().intersects(area)) out.push_back(k);
    return out;
}

std::optional<Key> CustomLayout::keyAt(Point p) const {
    for (auto it = keys_.rbegin(); it != keys_.rend(); ++it)
        if (it->contains(p)) return *it;
    return std::nullopt;
}

std::optional<Key> CustomLayout::keyForStep(int) const { return std::nullopt; }

// --- Numbers and converting ------------------------------------------------------------

int suggestNumber(const std::vector<CustomKey>& keys, std::size_t index, const Tuning& tuning) {
    const CustomKey& key = keys[index];
    const double pitch = customPitch(key, tuning);
    bool used[128] = {};
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i == index || !sameRoute(keys[i], key)) continue;
        if (std::abs(customPitch(keys[i], tuning) - pitch) < 1e-9) return keys[i].number;   // the same note
        if (keys[i].number >= 0 && keys[i].number <= 127) used[keys[i].number] = true;
    }
    int best = -1;
    for (int n = 0; n < 128; ++n)
        if (!used[n] && (best < 0 || std::abs(n - pitch) < std::abs(best - pitch) - 1e-9)) best = n;
    return best >= 0 ? best : nearestMidi(pitch);
}

std::vector<CustomKey> convertToCustom(const Layout& layout, const Tuning& tuning, LayoutKind kind,
                                       TileColor tileColor) {
    // The 12-EDO piano's rectangles become the diamond piano's arrangement of
    // the same keys: a diamond each, the black ones half a diamond up, each
    // keeping its computer key.
    std::vector<Key> keys = layout.keysIn(layout.bounds());
    if (std::any_of(keys.begin(), keys.end(), [](const Key& k) { return k.shape == KeyShape::Rectangle; })) {
        std::map<int, int> steps;   // step -> computer key
        for (const Key& k : keys)
            if (k.playable) steps[k.step] = k.computerKey;
        const DiamondLayout diamonds(tuning);
        keys.clear();
        for (Key k : diamonds.keysIn(diamonds.bounds()))
            if (const auto it = steps.find(k.step); it != steps.end()) {
                k.computerKey = it->second;
                keys.push_back(k);
            }
    }

    std::vector<CustomKey> out;
    for (const Key& k : keys) {
        if (!k.playable) continue;
        const KeyLook look = keyLook(k, tuning, kind, tileColor, false, false);
        CustomKey c;
        c.center = k.center;
        c.halfWidth = k.halfWidth;
        c.halfHeight = k.halfHeight;
        c.shade = k.shade;
        if (kind == LayoutKind::Tiled) c.color = packed(look.fill);   // the tiles' colouring, kept
        // Its labels as the layout's, in styles that follow its pitch: Tiled's
        // name and degree on every tile, the diamond piano's name and
        // frequency on the origin's note (the 12-EDO piano is drawn as the
        // diamond piano now). A label left empty stays empty.
        if (kind == LayoutKind::Tiled) {
            c.labelStyle = LabelStyle::NameOctave;
            c.sublabelStyle = LabelStyle::Degree;
        } else if (!look.title.empty()) {
            c.labelStyle = LabelStyle::Name;
            c.sublabelStyle = LabelStyle::Frequency;
        }
        // Its step as octave and degree of the tuning's EDO above the origin note.
        c.edo = tuning.edo;
        c.octave = tuning.originOctave + floorDiv(k.step, tuning.edo);
        c.degree = mod(k.step, tuning.edo);
        c.number = k.note.number;
        c.group = k.note.group;
        c.channel = k.note.channel;
        c.computerKey = k.computerKey;
        out.push_back(c);
    }
    return out;
}

} // namespace kedo
