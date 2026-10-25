#include "kedo/KeyStyle.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace kedo {

namespace {

Rgb grey(float v) { return {v, v, v}; }

// Pressed is KeyEDO's pink; latched is orange, so the two can be told apart.
const Rgb kPressed{1.0f, 0.42f, 0.62f};
const Rgb kLatched{1.0f, 0.68f, 0.28f};
// A scale's keys, in teal (neither pressed nor latched): pale on white keys,
// deep on black, stronger on the key note.
const Rgb kScaleWhite{0.62f, 0.88f, 0.92f}, kTonicWhite{0.25f, 0.75f, 0.85f};
const Rgb kScaleBlack{0.0f, 0.33f, 0.40f}, kTonicBlack{0.0f, 0.55f, 0.66f};
// How opaque a key with no note is.
const float kNoNote = 0.45f;

int mod(int a, int b) { return ((a % b) + b) % b; }

// KeyEDO's octave colouring: a hue per octave (seven, around the wheel), from
// pale at the octave's first step to strong at its last.
Rgb octaveColor(int step, int edo) {
    const int octave = mod(step >= 0 ? step / edo : (step - edo + 1) / edo, 7);
    const double h = octave / 7.0 * 6.0;
    const double f = h - std::floor(h);
    double r, g, b;
    switch (int(h) % 6) {
    case 0: r = 1; g = f; b = 0; break;
    case 1: r = 1 - f; g = 1; b = 0; break;
    case 2: r = 0; g = 1; b = f; break;
    case 3: r = 0; g = 1 - f; b = 1; break;
    case 4: r = f; g = 0; b = 1; break;
    default: r = 1; g = 0; b = 1 - f; break;
    }
    const double t = 0.18 + double(mod(step, edo)) / edo * (1.0 - 0.18);
    return {float(1 - t * (1 - r)), float(1 - t * (1 - g)), float(1 - t * (1 - b))};
}

std::string noteName(const StepLabel& l, bool withOctave) {
    std::string s = pitchClassName(l.pitchClass);
    if (withOctave) s += std::to_string(l.octave);
    return s;
}

} // namespace

std::string noteNameOf(double pitch) { return noteName(labelOfPitch(pitch), true); }

Rgb hsb(double h, double s, double b) {
    h = (h - std::floor(h)) * 6.0;
    const int i = int(h) % 6;
    const double f = h - std::floor(h);
    const double p = b * (1 - s), q = b * (1 - s * f), t = b * (1 - s * (1 - f));
    switch (i) {
    case 0: return {float(b), float(t), float(p)};
    case 1: return {float(q), float(b), float(p)};
    case 2: return {float(p), float(b), float(t)};
    case 3: return {float(p), float(q), float(b)};
    case 4: return {float(t), float(p), float(b)};
    default: return {float(b), float(p), float(q)};
    }
}

KeyLook keyLook(const Key& key, const Tuning& tuning, LayoutKind kind, TileColor tileColor,
                bool sounding, bool latched, ScaleMark scale) {
    KeyLook look;
    const StepLabel label = tuning.label(key.step);
    const bool black = key.shade == KeyShade::Black;

    if (kind == LayoutKind::Custom) {
        // Its own colour, or its shade as the diamond piano's; its own
        // labels; ink that shows on the fill.
        if (key.color)
            look.fill = {((*key.color >> 16) & 0xFF) / 255.0f, ((*key.color >> 8) & 0xFF) / 255.0f,
                         (*key.color & 0xFF) / 255.0f};
        else look.fill = black ? grey(0.0f) : grey(1.0f);
        if (!key.playable) look.fill = grey(0.28f), look.opacity = kNoNote;
        look.outline = grey(0.30f);
        const float light = 0.30f * look.fill.r + 0.59f * look.fill.g + 0.11f * look.fill.b;
        look.ink = light > 0.5f ? grey(0.12f) : grey(0.95f);
        look.title = key.label;
        look.detail = key.sublabel;
    } else if (kind == LayoutKind::Tiled) {
        // Colour by note or octave; name, octave and scale degree (1-based) on
        // every tile, the name greyer the further it is from 12-EDO.
        if (!key.playable) look.fill = grey(0.28f), look.opacity = kNoNote;
        else if (tileColor == TileColor::Note) look.fill = hsb(label.pitchClass / 12.0, 0.42, 0.92);
        else if (tileColor == TileColor::Octave) look.fill = octaveColor(key.step, tuning.edo);
        else look.fill = grey(0.88f);
        look.outline = grey(0.30f);
        look.ink = grey(float(0.10 + std::min(1.0, std::abs(label.cents) / 50.0) * 0.45));
        if (key.playable) {
            look.title = noteName(label, true);
            look.detail = std::to_string(mod(key.step, tuning.edo) + 1);
        }
    } else if (key.shape == KeyShape::Diamond) {
        // The diamond piano: black and white, the origin's note named with its
        // frequency (KeyEDO's EDOKeyView).
        look.fill = black ? grey(0.0f) : grey(1.0f);
        look.outline = {0.10f, 0.13f, 0.16f};   // dark slate: an edge on the background
        look.ink = black ? grey(1.0f) : grey(0.0f);
        if (mod(key.step, tuning.edo) == 0) {
            look.title = noteName(label, false);
            char buf[16];
            std::snprintf(buf, sizeof buf, "%.1f", tuning.frequency(key.step));
            look.detail = buf;
        }
    } else {
        // The piano: C named on each octave.
        look.fill = black ? grey(0.12f) : grey(0.90f);
        look.outline = grey(0.35f);
        look.ink = grey(0.35f);
        if (!black && label.pitchClass == 0) look.title = noteName(label, true);
    }

    if (kind == LayoutKind::Piano && scale != ScaleMark::Off)
        look.fill = scale == ScaleMark::Tonic ? (black ? kTonicBlack : kTonicWhite)
                                              : (black ? kScaleBlack : kScaleWhite);
    if (latched) look.fill = kLatched;
    else if (sounding) look.fill = kPressed;
    return look;
}

Rgb keyboardBackground(LayoutKind kind, const Tuning& tuning) {
    const Rgb slate{0.40f, 0.49f, 0.55f};   // between black and white keys, so both show
    switch (kind) {
    case LayoutKind::Piano: return tuning.edo != 12 ? slate : Rgb{0.17f, 0.22f, 0.27f};
    case LayoutKind::Tiled: return slate;
    case LayoutKind::Custom: return {0.11f, 0.14f, 0.18f};   // darker still, black keys show
    }
    return slate;
}

} // namespace kedo
