#include "kedo/Viewport.h"

#include <algorithm>

namespace kedo {

void Viewport::zoomAbout(Point anchor, double factor) {
    const Point fixed = toLayout(anchor);
    zoom = std::clamp(zoom * factor, kMinZoom, kMaxZoom);
    scroll.x = fixed.x - (anchor.x - screen.x) / zoom;
    scroll.y = fixed.y - (anchor.y - screen.y) / zoom;
}

void Viewport::clampTo(const Rect& content) {
    const Rect v = visible();
    auto clamp1 = [](double pos, double size, double lo, double extent) {
        if (extent <= size) return lo - (size - extent) / 2;   // centre
        return std::clamp(pos, lo, lo + extent - size);
    };
    scroll.x = clamp1(scroll.x, v.w, content.x, content.w);
    scroll.y = clamp1(scroll.y, v.h, content.y, content.h);
}

} // namespace kedo
