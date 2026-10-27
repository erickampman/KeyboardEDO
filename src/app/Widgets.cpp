#include "Widgets.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace kedo::app::ui {

namespace {

constexpr float kPi = 3.14159265f;
// How far a drag turns a knob from end to end, in points (a slider's travel
// is its own length).
constexpr float kKnobTravel = 160;
// The knob's 270-degree sweep, from bottom left round to bottom right.
constexpr float kFrom = 0.75f * kPi, kSweep = 1.5f * kPi;

ImVec4 alpha(ImVec4 c, float a) { return {c.x, c.y, c.z, a}; }
ImVec4 mix(ImVec4 a, ImVec4 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}
ImVec4 lighten(ImVec4 c, float t) { return mix(c, {1, 1, 1, c.w}, t); }
ImU32 u32(ImVec4 c) { return ImGui::ColorConvertFloat4ToU32(c); }

const ImVec4 kWhite{1, 1, 1, 1};
const ImVec4 kInk{0.88f, 0.90f, 0.93f, 1};   // the value boxes' text

// A disc from `inner` at radius r0 to `outer` at r1, solid inside r0: what
// NanoVG's radial gradient gives the plugins, from triangles.
void radial(ImDrawList* d, ImVec2 c, float r0, float r1, ImVec4 inner, ImVec4 outer) {
    const int n = std::clamp(int(r1 * 1.5f), 24, 96);
    const ImU32 in = u32(inner), out = u32(outer);
    const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    d->PrimReserve(n * 9, 1 + 2 * n);
    const auto base = ImDrawIdx(d->_VtxCurrentIdx);
    d->PrimWriteVtx(c, uv, in);
    for (int i = 0; i < n; ++i) {
        const float a = 2 * kPi * float(i) / float(n);
        const ImVec2 dir{std::cos(a), std::sin(a)};
        d->PrimWriteVtx({c.x + dir.x * r0, c.y + dir.y * r0}, uv, in);
        d->PrimWriteVtx({c.x + dir.x * r1, c.y + dir.y * r1}, uv, out);
    }
    for (int i = 0; i < n; ++i) {
        const auto j = ImDrawIdx((i + 1) % n);
        const auto i0 = ImDrawIdx(base + 1 + 2 * i), i1 = ImDrawIdx(i0 + 1);
        const auto j0 = ImDrawIdx(base + 1 + 2 * j), j1 = ImDrawIdx(j0 + 1);
        d->PrimWriteIdx(base); d->PrimWriteIdx(i0); d->PrimWriteIdx(j0);
        d->PrimWriteIdx(i0); d->PrimWriteIdx(i1); d->PrimWriteIdx(j1);
        d->PrimWriteIdx(i0); d->PrimWriteIdx(j1); d->PrimWriteIdx(j0);
    }
}

// PlainOldSynth's knob body (src/UI/Knob.hpp): a glow bright at the centre
// and fading out at the rim, a brighter core, a faint rim and a soft glint at
// the top left. `lit` brightens it, for hover and drag.
void sphere(ImDrawList* d, ImVec2 c, float r, ImVec4 accent, float lit) {
    radial(d, c, r * 0.15f, r, alpha(accent, 0.80f * lit), alpha(accent, 0));
    radial(d, c, 0, r * 0.62f, alpha(lighten(accent, 0.35f), std::min(1.0f, 0.85f * lit)), alpha(accent, 0));
    d->AddCircle(c, r, u32(alpha(kWhite, 0.14f)), 0, 1.2f);
    radial(d, {c.x - r * 0.30f, c.y - r * 0.34f}, 0, r * 0.26f, alpha(kWhite, 0.11f), alpha(kWhite, 0));
}

float litFor() { return ImGui::IsItemActive() ? 1.2f : ImGui::IsItemHovered() ? 1.1f : 1.0f; }

// Ten times finer with Ctrl or Cmd held, as the plugins' knobs are.
float fineness() {
    const ImGuiIO& io = ImGui::GetIO();
    return io.KeyCtrl || io.KeySuper ? 0.1f : 1.0f;
}

float quantize(float v, int steps) { return steps > 1 ? std::round(v * float(steps - 1)) / float(steps - 1) : v; }

} // namespace

void applyTheme(ImGuiStyle& style) {
    using namespace colour;
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = {0.88f, 0.90f, 0.93f, 1};
    c[ImGuiCol_TextDisabled] = lighten(kSlate, 0.15f);
    c[ImGuiCol_WindowBg] = kPanel;
    c[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg] = alpha(kPanel, 0.98f);
    c[ImGuiCol_Border] = alpha(kSlate, 0.35f);
    c[ImGuiCol_FrameBg] = kWell;
    c[ImGuiCol_FrameBgHovered] = mix(kWell, kSlate, 0.25f);
    c[ImGuiCol_FrameBgActive] = mix(kWell, kSlate, 0.40f);
    c[ImGuiCol_TitleBg] = kWell;
    c[ImGuiCol_TitleBgActive] = kWell;
    c[ImGuiCol_TitleBgCollapsed] = kWell;
    c[ImGuiCol_MenuBarBg] = kPanel;
    c[ImGuiCol_ScrollbarBg] = alpha(kWell, 0.5f);
    c[ImGuiCol_ScrollbarGrab] = alpha(kSlate, 0.45f);
    c[ImGuiCol_ScrollbarGrabHovered] = alpha(kSlate, 0.70f);
    c[ImGuiCol_ScrollbarGrabActive] = kAccent;
    c[ImGuiCol_CheckMark] = kAccent;
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = lighten(kAccent, 0.25f);
    c[ImGuiCol_Button] = alpha(kSlate, 0.30f);
    c[ImGuiCol_ButtonHovered] = alpha(kSlate, 0.55f);
    c[ImGuiCol_ButtonActive] = kAccent;
    c[ImGuiCol_Header] = alpha(kAccent, 0.30f);
    c[ImGuiCol_HeaderHovered] = alpha(kAccent, 0.45f);
    c[ImGuiCol_HeaderActive] = alpha(kAccent, 0.60f);
    c[ImGuiCol_Separator] = alpha(kSlate, 0.35f);
    c[ImGuiCol_SeparatorHovered] = alpha(kAccent, 0.6f);
    c[ImGuiCol_SeparatorActive] = kAccent;
    c[ImGuiCol_ResizeGrip] = alpha(kSlate, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = alpha(kAccent, 0.6f);
    c[ImGuiCol_ResizeGripActive] = kAccent;
    c[ImGuiCol_TextSelectedBg] = alpha(kAccent, 0.35f);
    c[ImGuiCol_NavCursor] = kAccent;
    style.FrameRounding = 3;
    style.GrabRounding = 3;
    style.PopupRounding = 4;
    style.ChildRounding = 6;
    style.FrameBorderSize = 1;
}

bool knob(const char* id, float& value, float start, float diameter, const ImVec4& accent, int steps) {
    const float before = value;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, {diameter, diameter});
    // While dragged it turns smoothly, and stops at its steps: the unstepped
    // value is kept between frames.
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetItemID();
    if (ImGui::IsItemActivated()) storage->SetFloat(key, value);
    if (ImGui::IsItemActive()) {
        const float turned = std::clamp(
            storage->GetFloat(key, value) - ImGui::GetIO().MouseDelta.y / kKnobTravel * fineness(), 0.0f, 1.0f);
        storage->SetFloat(key, turned);
        value = quantize(turned, steps);
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) value = start;

    ImDrawList* d = ImGui::GetWindowDrawList();
    const float r = diameter / 2;
    const ImVec2 c{at.x + r, at.y + r};
    sphere(d, c, r, accent, litFor());
    // The position: a bright dot near the rim.
    const float angle = kFrom + kSweep * value;
    d->AddCircleFilled({c.x + std::cos(angle) * r * 0.72f, c.y + std::sin(angle) * r * 0.72f},
                       std::max(2.0f, r * 0.13f), IM_COL32(244, 247, 250, 255));
    return value != before;
}

bool slider(const char* id, float& value, float lo, float hi, float rest, ImVec2 size, const ImVec4& accent,
            bool spring) {
    const float before = value;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const bool vertical = size.y > size.x;
    const float thumb = vertical ? size.x : size.y;     // its diameter
    const float length = vertical ? size.y : size.x;
    const float travel = std::max(1.0f, length - thumb);
    if (ImGui::IsItemActive()) {
        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        const float moved = (vertical ? -delta.y : delta.x) / travel * (hi - lo) * fineness();
        value = std::clamp(value + moved, lo, hi);
    } else if (spring) {
        value = rest;
    }

    // Where a value is along it: bottom to top, or left to right.
    auto place = [&](float v) {
        const float t = (std::clamp(v, lo, hi) - lo) / (hi - lo);
        return vertical ? ImVec2{at.x + thumb / 2, at.y + size.y - thumb / 2 - t * travel}
                        : ImVec2{at.x + thumb / 2 + t * travel, at.y + thumb / 2};
    };
    ImDrawList* d = ImGui::GetWindowDrawList();
    const float half = std::max(2.0f, thumb * 0.15f);   // half the slot's width
    const ImVec2 a = place(lo), b = place(hi);
    const ImVec2 slotMin = vertical ? ImVec2{a.x - half, b.y - half} : ImVec2{a.x - half, a.y - half};
    const ImVec2 slotMax = vertical ? ImVec2{a.x + half, a.y + half} : ImVec2{b.x + half, a.y + half};
    d->AddRectFilled(slotMin, slotMax, u32(colour::kWell), half);
    d->AddRect(slotMin, slotMax, u32(alpha(colour::kSlate, 0.45f)), half);

    // Lit from the rest to the value, with a faint glow either side.
    const ImVec2 r = place(rest), v = place(value);
    const ImVec2 litMin{std::min(r.x, v.x), std::min(r.y, v.y)}, litMax{std::max(r.x, v.x), std::max(r.y, v.y)};
    const float glow = half * 1.8f;
    const float inset = 1.5f;
    if (vertical) {
        d->AddRectFilled({litMin.x - glow, litMin.y}, {litMax.x + glow, litMax.y}, u32(alpha(accent, 0.12f)), glow);
        d->AddRectFilled({litMin.x - half + inset, litMin.y}, {litMax.x + half - inset, litMax.y},
                         u32(alpha(accent, 0.80f)), half);
    } else {
        d->AddRectFilled({litMin.x, litMin.y - glow}, {litMax.x, litMax.y + glow}, u32(alpha(accent, 0.12f)), glow);
        d->AddRectFilled({litMin.x, litMin.y - half + inset}, {litMax.x, litMax.y + half - inset},
                         u32(alpha(accent, 0.80f)), half);
    }
    // The rest marked across the slot, when it's not at an end.
    if (rest > lo && rest < hi) {
        const float tick = half + thumb * 0.22f;
        const ImU32 mark = u32(alpha(colour::kSlate, 0.9f));
        if (vertical) d->AddLine({r.x - tick, r.y}, {r.x + tick, r.y}, mark, 1.5f);
        else d->AddLine({r.x, r.y - tick}, {r.x, r.y + tick}, mark, 1.5f);
    }

    // The thumb: a small knob body over the slot, with a dot at its centre.
    const float radius = thumb / 2;
    d->AddCircleFilled(v, radius * 0.95f, u32(colour::kSection));
    sphere(d, v, radius, accent, litFor());
    d->AddCircleFilled(v, std::max(2.0f, radius * 0.17f), IM_COL32(244, 247, 250, 255));
    return value != before;
}

bool sliderRow(const char* label, int& value, int lo, int hi, const ImVec4& accent) {
    const float width = ImGui::CalcItemWidth();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float box = ImGui::CalcTextSize("0000").x + 2 * style.FramePadding.x;
    const float height = ImGui::GetFrameHeight();
    ImGui::PushID(label);
    // The unrounded value is kept between frames, so a slow drag still moves.
    ImGuiStorage* storage = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetID("value");
    float f = storage->GetFloat(key, float(value));
    if (int(std::lround(f)) != value) f = float(value);
    const int before = value;
    slider("##slider", f, float(lo), float(hi), float(lo), {std::max(height, width - box - style.ItemInnerSpacing.x), height},
           accent);
    storage->SetFloat(key, f);
    value = int(std::lround(f));
    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    valueBox("##box", std::to_string(value), box);
    ImGui::PopID();
    const char* end = std::strstr(label, "##");
    ImGui::SameLine(0, style.ItemInnerSpacing.x);
    ImGui::TextUnformatted(label, end);
    return value != before;
}

void caption(const char* text, float width, const ImVec4& accent) {
    const float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(x + std::max(0.0f, (width - ImGui::CalcTextSize(text).x) / 2));
    ImGui::TextColored(lighten(accent, 0.45f), "%s", text);
}

bool valueBox(const char* id, const std::string& text, float width) {
    const float height = ImGui::GetFrameHeight();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float boxWidth = std::min(width, ImGui::CalcTextSize(text.c_str()).x + 2.5f * ImGui::GetFontSize());
    ImGui::SetCursorScreenPos({at.x + (width - boxWidth) / 2, at.y});
    const bool clicked = ImGui::InvisibleButton(id, {boxWidth, height});
    const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
    ImDrawList* d = ImGui::GetWindowDrawList();
    d->AddRectFilled(min, max, u32(colour::kWell), 3);
    d->AddRect(min, max, u32(alpha(colour::kSlate, ImGui::IsItemHovered() ? 0.7f : 0.4f)), 3);
    const ImVec2 extent = ImGui::CalcTextSize(text.c_str());
    d->AddText({(min.x + max.x - extent.x) / 2, (min.y + max.y - extent.y) / 2}, u32(kInk), text.c_str());
    return clicked;
}

bool beginSection(const char* title, const ImVec4& accent, bool collapsible, bool* open) {
    const float em = ImGui::GetFontSize();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, colour::kSection);
    ImGui::PushStyleColor(ImGuiCol_Border, alpha(colour::kSlate, 0.30f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.6f * em, 0.45f * em});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.5f * em);
    ImGui::BeginChild(title, {0, 0},
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    bool shown = true;
    if (collapsible || open) {
        ImGui::PushStyleColor(ImGuiCol_Text, accent);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{0, 0, 0, 0});
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, alpha(accent, 0.12f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, alpha(accent, 0.22f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        if (open) ImGui::SetNextItemOpen(*open);
        shown = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
        // Only a click changes it: a header scrolled out of sight reads closed.
        if (open && ImGui::IsItemToggledOpen()) *open = shown;
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    } else {
        ImGui::TextColored(accent, "%s", title);
    }
    return shown;
}

void endSection() {
    ImGui::EndChild();
    ImGui::Dummy({0, ImGui::GetStyle().ItemSpacing.y * 0.5f});
}

} // namespace kedo::app::ui
