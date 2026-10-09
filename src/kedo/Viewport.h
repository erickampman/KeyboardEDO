#pragma once

#include "kedo/Layout.h"

// Which part of a layout is on screen, and at what zoom: the conversion
// between screen points and layout units. The view owns one; the Performer
// only ever sees layout coordinates.
namespace kedo {

struct Viewport {
    static constexpr double kMinZoom = 0.5;   // KeyEDO's pinch range
    static constexpr double kMaxZoom = 3.0;

    Rect   screen;          // the keyboard's area on screen, in points
    double zoom = 1.0;      // screen points per layout unit
    Point  scroll;          // the layout point at the top-left of `screen`

    Point toLayout(Point s) const {
        return {(s.x - screen.x) / zoom + scroll.x, (s.y - screen.y) / zoom + scroll.y};
    }
    Point toScreen(Point l) const {
        return {(l.x - scroll.x) * zoom + screen.x, (l.y - scroll.y) * zoom + screen.y};
    }
    // The part of the layout on screen.
    Rect visible() const { return {scroll.x, scroll.y, screen.w / zoom, screen.h / zoom}; }

    // Scroll by a distance in screen points.
    void scrollBy(double dx, double dy) {
        scroll.x += dx / zoom;
        scroll.y += dy / zoom;
    }

    // Zoom by `factor`, keeping the layout point under `anchor` (screen) still.
    void zoomAbout(Point anchor, double factor);

    // Keep `content` in view. In each direction: centred if it fits, otherwise
    // no empty space past either edge.
    void clampTo(const Rect& content);
};

} // namespace kedo
