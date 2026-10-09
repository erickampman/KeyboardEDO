// Draws layouts to SVG, to check their geometry by eye before there's a UI.
//
//   kedo-layout-svg <out-dir>      (made if it doesn't exist)
//
// writes piano-12.svg, diamond-<edo>.svg and tiled-<edo>-<preset>.svg for a
// few EDOs. Labels and colours follow KeyEDO: note name and step, and on Tiled
// a hue per 12-EDO pitch class.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "kedo/Layout.h"

using namespace kedo;

namespace {

std::string hsl(int pc, bool playable) {
    if (!playable) return "#474747";
    char buf[48];
    std::snprintf(buf, sizeof buf, "hsl(%d,45%%,72%%)", pc * 30);
    return buf;
}

std::string slug(std::string s) {
    for (auto& c : s)
        if (c == ' ' || c == '(' || c == ')') c = '-';
    return s;
}

void write(const std::string& path, const Layout& layout, const Tuning& tuning, bool colour) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) {
        std::perror(path.c_str());
        return;
    }
    const Rect b = layout.bounds();
    const double pad = 10;
    std::fprintf(f, "<svg xmlns='http://www.w3.org/2000/svg' viewBox='%g %g %g %g' width='%g' height='%g'>\n",
                 b.x - pad, b.y - pad, b.w + 2 * pad, b.h + 2 * pad, b.w + 2 * pad, b.h + 2 * pad);
    std::fprintf(f, "<rect x='%g' y='%g' width='%g' height='%g' fill='#f2f2f2'/>\n",
                 b.x - pad, b.y - pad, b.w + 2 * pad, b.h + 2 * pad);
    for (const auto& k : layout.keysIn(b)) {
        const auto label = tuning.label(k.step);
        const bool black = k.shade == KeyShade::Black;
        const std::string fill = colour ? hsl(label.pitchClass, k.playable) : (black ? "#000" : "#fff");
        const double cx = k.center.x, cy = k.center.y;
        if (k.shape == KeyShape::Diamond) {
            const double h = k.halfWidth;
            std::fprintf(f, "<path d='M%g %gL%g %gL%g %gL%g %gZ' fill='%s' stroke='#888' stroke-width='0.8'/>\n",
                         cx, cy - h, cx + h, cy, cx, cy + h, cx - h, cy, fill.c_str());
        } else {
            std::fprintf(f, "<rect x='%g' y='%g' width='%g' height='%g' fill='%s' stroke='#888'/>\n",
                         cx - k.halfWidth, cy - k.halfHeight, 2 * k.halfWidth, 2 * k.halfHeight, fill.c_str());
        }
        if (!k.playable) continue;
        const char* ink = black && !colour ? "#fff" : "#000";
        const double ty = k.shape == KeyShape::Rectangle ? cy + k.halfHeight - 22 : cy - 2;
        std::fprintf(f, "<text x='%g' y='%g' font-size='11' font-family='sans-serif' text-anchor='middle' fill='%s'>%s%d</text>\n",
                     cx, ty, ink, pitchClassName(label.pitchClass), label.octave);
        std::fprintf(f, "<text x='%g' y='%g' font-size='9' font-family='sans-serif' text-anchor='middle' fill='%s'>%d</text>\n",
                     cx, ty + 12, ink, k.step);
    }
    std::fprintf(f, "</svg>\n");
    std::fclose(f);
    std::printf("%s\n", path.c_str());
}

} // namespace

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        std::fprintf(stderr, "%s: %s\n", dir.c_str(), ec.message().c_str());
        return 1;
    }

    Tuning twelve;
    write(dir + "/piano-12.svg", PianoLayout(twelve, 36, 72), twelve, false);

    for (int edo : {17, 19, 31, 7}) {
        Tuning t;
        t.edo = edo;
        t.originOctave = edo > 24 ? 3 : 1;   // fewer keys for the wide ones
        write(dir + "/diamond-" + std::to_string(edo) + ".svg", DiamondLayout(t), t, false);
        for (const auto& g : generatorPresets(edo)) {
            TiledLayout tiled(t, g, 6);
            write(dir + "/tiled-" + std::to_string(edo) + "-" + slug(g.name) + ".svg", tiled, t, true);
        }
    }
    return 0;
}
