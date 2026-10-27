#pragma once

#include <string>

#include "imgui.h"

// The app's own controls, in the plugins' look (decisions.md, "Look and
// feel"): PlainOldSynth's glowing knobs, a linear slider to go with them, value
// boxes and section boxes, in KeyboardEDO's colours. ImGui only, as the
// plugins' are NanoVG: what's shared is the design, not the code.
namespace kedo::app::ui {

// KeyboardEDO's colours: the keyboards' slates, the wordmark's two blues and
// a pressed key's pink.
namespace colour {
inline const ImVec4 kPanel{28 / 255.f, 36 / 255.f, 46 / 255.f, 1};      // Custom's background
inline const ImVec4 kSection{37 / 255.f, 48 / 255.f, 60 / 255.f, 1};    // toward the piano's slate
inline const ImVec4 kWell{20 / 255.f, 26 / 255.f, 33 / 255.f, 1};       // value boxes, slots: the keys' outline slate, darker
inline const ImVec4 kSlate{102 / 255.f, 125 / 255.f, 140 / 255.f, 1};   // the diamonds' background
inline const ImVec4 kSteel{159 / 255.f, 191 / 255.f, 224 / 255.f, 1};   // "Keyboard"
inline const ImVec4 kAccent{66 / 255.f, 150 / 255.f, 250 / 255.f, 1};   // "EDO", ImGui's dark accent
inline const ImVec4 kPink{1.0f, 0.42f, 0.62f, 1};                         // a pressed key
inline const ImVec4 kTeal{0.25f, 0.75f, 0.85f, 1};                         // a scale's key note
} // namespace colour

// The panels' ImGui style: the slates for backgrounds and frames, the accent
// for what's on or active. Colours only; sizes are main.cpp's.
void applyTheme(ImGuiStyle& style);

// A glowing knob, `diameter` across, 0..1: dragging up turns it up, wherever
// the pointer goes (Ctrl or Cmd: ten times finer); a double click sets it to
// `start`. `steps` above 1: it stops at that many evenly spaced places.
// Returns true when it changed.
bool knob(const char* id, float& value, float start, float diameter, const ImVec4& accent, int steps = 0);

// A linear slider from `lo` to `hi`, `size` across and high: vertical when
// taller than wide. A recessed slot, lit from `rest` to the value, and a
// glowing thumb. A drag moves it as far as the pointer moves, wherever it's
// grabbed, so a click doesn't make it jump. `spring`: back to `rest` when let
// go (a bend wheel). Returns true when it changed.
bool slider(const char* id, float& value, float lo, float hi, float rest, ImVec2 size, const ImVec4& accent,
            bool spring = false);

// A horizontal slider in a settings row, as ImGui's SliderInt is laid out: the
// slider and a value box, the item width, then `label` (its text up to "##").
bool sliderRow(const char* label, int& value, int lo, int hi, const ImVec4& accent);

// A caption centred in the next `width` points, tinted toward `accent`.
void caption(const char* text, float width, const ImVec4& accent);

// A value box: `text` in a dark rounded box, centred in the next `width`
// points. Returns true when clicked (to type a value).
bool valueBox(const char* id, const std::string& text, float width);

// A section of a panel, as the plugins' group boxes: a rounded box a shade
// lighter than the panel, its title in `accent`. Collapsible when
// `collapsible` (Settings), and then returns whether it's open; call
// endSection() whatever it returns. With `open`, collapsible and open as it
// says, and a click on the title changes it.
bool beginSection(const char* title, const ImVec4& accent, bool collapsible, bool* open = nullptr);
void endSection();

} // namespace kedo::app::ui
