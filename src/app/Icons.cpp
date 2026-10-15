#include "Icons.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "imgui.h"

namespace kedo::app {

namespace {

// Draws `icon` in the square at `o` with side `s`, lines `w` thick. Shapes are
// laid out on a 24-unit grid, as SF Symbols and most icon sets are.
void draw(ImDrawList* d, Icon icon, ImVec2 o, float s, ImU32 c, float w) {
    const float u = s / 24.0f;
    auto p = [&](float x, float y) { return ImVec2(o.x + x * u, o.y + y * u); };
    const float pi = 3.14159265f;

    switch (icon) {
    case Icon::Open: {
        // Folder: tab on the top left, body below.
        const ImVec2 pts[] = {p(3, 7), p(3, 19), p(21, 19), p(21, 9), p(12, 9), p(10, 6), p(4, 6)};
        d->AddPolyline(pts, 7, c, ImDrawFlags_Closed, w);
        d->AddLine(p(3, 11), p(21, 11), c, w);
        break;
    }
    case Icon::Save: {
        // Tray, and an arrow down into it.
        const ImVec2 tray[] = {p(4, 14), p(4, 20), p(20, 20), p(20, 14)};
        d->AddPolyline(tray, 4, c, 0, w);
        d->AddLine(p(12, 3), p(12, 15), c, w);
        const ImVec2 head[] = {p(7.5f, 10.5f), p(12, 15), p(16.5f, 10.5f)};
        d->AddPolyline(head, 3, c, 0, w);
        break;
    }
    case Icon::Routing: {
        // Antenna: a dot, a mast, and two pairs of arcs radiating from it.
        d->AddCircleFilled(p(12, 10), 1.8f * u, c);
        d->AddLine(p(12, 12), p(12, 21), c, w);
        for (int side = -1; side <= 1; side += 2) {
            for (float r : {5.0f, 9.0f}) {
                const float mid = side < 0 ? pi : 0.0f;
                d->PathArcTo(p(12, 10), r * u, mid - 0.7f, mid + 0.7f, 10);
                d->PathStroke(c, 0, w);
            }
        }
        break;
    }
    case Icon::Locked:
    case Icon::Unlocked: {
        // Body, and the shackle: closed over the body, or swung up and left.
        d->AddRect(p(5, 11), p(19, 21), c, 2 * u, 0, w);
        d->AddCircleFilled(p(12, 16), 1.5f * u, c);
        const float x = icon == Icon::Locked ? 12.0f : 6.5f;
        const float top = icon == Icon::Locked ? 7.0f : 6.0f;
        d->PathArcTo(p(x, top + 0.5f), 4.5f * u, pi, 2 * pi, 12);
        d->PathStroke(c, 0, w);
        // The right leg goes into the body; the left one does too when locked,
        // and hangs free when open.
        d->AddLine(p(x - 4.5f, top + 0.5f), p(x - 4.5f, icon == Icon::Locked ? 11.0f : 9.0f), c, w);
        d->AddLine(p(x + 4.5f, top + 0.5f), p(x + 4.5f, 11), c, w);
        break;
    }
    case Icon::Latch: {
        // Pushpin: head, collar, and the point.
        const ImVec2 head[] = {p(8, 3), p(16, 3), p(15, 5), p(15, 10), p(18, 13), p(6, 13), p(9, 10), p(9, 5)};
        d->AddPolyline(head, 8, c, ImDrawFlags_Closed, w);
        d->AddLine(p(12, 13), p(12, 21), c, w);
        break;
    }
    case Icon::Arpeggiator:
        // Four notes rising, one after another: a dot and a stem each.
        for (int i = 0; i < 4; ++i) {
            const float x = 4.5f + 4.6f * i, y = 18.5f - 3.6f * i;
            d->AddCircleFilled(p(x, y), 2.0f * u, c);
            d->AddLine(p(x + 1.8f, y), p(x + 1.8f, y - 6.5f), c, w * 0.8f);
        }
        break;
    case Icon::Panic:
        // Stop: a filled square in a circle.
        d->AddCircle(p(12, 12), 9 * u, c, 24, w);
        d->AddRectFilled(p(8.5f, 8.5f), p(15.5f, 15.5f), c, 1 * u);
        break;
    case Icon::Monitor:
        // Lines of text with a dot before each.
        for (float y : {7.0f, 12.0f, 17.0f}) {
            d->AddCircleFilled(p(5, y), 1.4f * u, c);
            d->AddLine(p(9, y), p(y == 12.0f ? 17.0f : 20.0f, y), c, w);
        }
        break;
    case Icon::Configure:
        // Three sliders, each knob in a different place.
        for (auto [y, knob] : {std::pair{6.0f, 15.0f}, std::pair{12.0f, 8.0f}, std::pair{18.0f, 13.0f}}) {
            d->AddLine(p(3, y), p(21, y), c, w);
            d->AddCircleFilled(p(knob, y), 2.6f * u, ImGui::GetColorU32(ImGuiCol_WindowBg));
            d->AddCircle(p(knob, y), 2.6f * u, c, 16, w);
        }
        break;
    }
}

} // namespace

bool iconButton(const char* id, Icon icon, float size, const char* tooltip, bool on) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton(id, {size, size});
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    ImDrawList* d = ImGui::GetWindowDrawList();

    const ImU32 back = held      ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
                       : on      ? ImGui::GetColorU32(ImGuiCol_Button)
                       : hovered ? ImGui::GetColorU32(ImGuiCol_ButtonHovered, 0.6f)
                                 : 0;
    if (back) d->AddRectFilled(at, {at.x + size, at.y + size}, back, size * 0.2f);
    const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text, on ? 1.0f : 0.8f);
    const float pad = size * 0.14f;
    draw(d, icon, {at.x + pad, at.y + pad}, size - 2 * pad, ink, std::max(1.2f, size * 0.06f));

    if (tooltip && hovered) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

} // namespace kedo::app
