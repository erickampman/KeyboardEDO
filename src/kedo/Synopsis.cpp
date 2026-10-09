#include "kedo/Synopsis.h"

#include <vector>

namespace kedo {

namespace {

const char* const kVertical[] = {"", "CC", "pressure", "bend"};

} // namespace

std::string synopsis(const Document& d, const LiveState& live) {
    std::vector<std::string> parts;
    const Tuning& t = d.tuning;
    parts.push_back(std::to_string(t.edo) + "-EDO");

    // The origin matters to every layout but the 12-EDO piano, where the
    // keys are the standard ones (KeyEDO leaves it out there too).
    const bool standardPiano = d.layout.kind == LayoutKind::Piano && t.edo == 12;
    if (!standardPiano) parts.push_back(std::string(pitchClassName(t.originPitchClass)) + std::to_string(t.originOctave));

    if (d.layout.kind == LayoutKind::Tiled) {
        const auto presets = generatorPresets(t.edo);
        const int i = d.layout.generatorIndex;
        parts.push_back("Tiled: " + presets[i >= 0 && i < int(presets.size()) ? i : 0].name);
    } else {
        parts.push_back(standardPiano ? "Piano" : "Diamond piano");
    }

    const PlayConfig& p = d.play;
    if (p.drag == DragBehavior::Gliss) {
        parts.push_back(p.glide ? "Gliss with glide" : "Gliss");
    } else {
        std::string fixed = "Fixed";
        std::vector<std::string> axes;
        if (p.horizontal == HorizontalControl::PitchBend) axes.push_back("H bend");
        if (p.vertical == VerticalControl::Controller)
            axes.push_back("V CC " + std::to_string(p.verticalController));
        else if (p.vertical != VerticalControl::Off)
            axes.push_back(std::string("V ") + kVertical[int(p.vertical)]);
        if (!axes.empty()) {
            fixed += " (";
            for (size_t i = 0; i < axes.size(); ++i) fixed += (i ? ", " : "") + axes[i];
            fixed += p.perNoteExpression ? ", per note)" : ")";
        }
        parts.push_back(fixed);
    }
    if (p.strikeVelocity) parts.push_back("Strike Vel");
    if (live.latch) parts.push_back("Latch");
    if (d.locked) parts.push_back("Locked");

    if (!live.destination.empty())
        parts.push_back("to " + live.destination + (live.destinationFound ? "" : " (not found)"));
    if (live.publishing) parts.push_back("Published");

    std::string out;
    for (size_t i = 0; i < parts.size(); ++i) out += (i ? "  ·  " : "") + parts[i];
    return out;
}

} // namespace kedo
