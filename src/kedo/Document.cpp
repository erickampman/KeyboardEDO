#include "kedo/Document.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

namespace kedo {

namespace {

using json = nlohmann::json;

// KeyEDO's keys that this version doesn't use, with KeyEDO's defaults: written
// when a document has no value of its own, because KeyEDO's decoder expects
// every key.
const json& keyEdoDefaults() {
    static const json defaults = {
        {"arpBpm", 120.0},
        {"arpDivision", "1/16"},
        {"arpGate", 0.5},
        {"arpHold", false},
        {"arpIsEnabled", false},
        {"arpOctaveSpan", 1},
        {"arpPattern", "Up"},
        {"arpRateSource", "Internal"},
        {"arpVelocitySource", "As Played"},
        {"hoverExpressionEnabled", false},
    };
    return defaults;
}

const char* const kTileColors[] = {"Off", "Note", "Octave"};

// Reads `key` into `value` if it's there with the right type; otherwise
// leaves the default.
template <typename T>
void read(const json& j, const char* key, T& value) {
    const auto it = j.find(key);
    if (it == j.end() || it->is_null()) return;
    try {
        value = it->get<T>();
    } catch (const json::exception&) {
    }
}

template <typename E>
void readEnum(const json& j, const char* key, E& value, int count) {
    int i = int(value);
    read(j, key, i);
    if (i >= 0 && i < count) value = E(i);
}

} // namespace

std::optional<Document> parseDocument(const std::string& text, std::string* error) {
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        if (error) *error = e.what();
        return std::nullopt;
    }
    if (!j.is_object()) {
        if (error) *error = "not a JSON object";
        return std::nullopt;
    }

    Document d;
    // Tuning
    read(j, "edoSteps", d.tuning.edo);
    d.tuning.edo = std::clamp(d.tuning.edo, Tuning::kMinEdo, Tuning::kMaxEdo);
    std::string origin = pitchClassName(d.tuning.originPitchClass);
    read(j, "originNoteName", origin);
    if (const int pc = pitchClassOf(origin); pc >= 0) d.tuning.originPitchClass = pc;
    read(j, "originOctave", d.tuning.originOctave);
    d.tuning.originOctave = std::clamp(d.tuning.originOctave, -1, 7);

    // Layout
    readEnum(j, "keyLayoutViewType", d.layout.kind, 2);
    read(j, "selectedPresetIndex", d.layout.generatorIndex);
    read(j, "pianoLowest", d.layout.pianoLowest);
    read(j, "pianoHighest", d.layout.pianoHighest);
    std::string tileColor = kTileColors[int(d.tileColor)];
    read(j, "tileColorMode", tileColor);
    for (int i = 0; i < 3; ++i)
        if (tileColor == kTileColors[i]) d.tileColor = TileColor(i);
    read(j, "locked", d.locked);

    // Playing
    readEnum(j, "dragBehavior", d.play.drag, 2);
    readEnum(j, "horizontalControl", d.play.horizontal, 2);
    readEnum(j, "verticalControl", d.play.vertical, 4);   // KeyEDO's order: off, cc, pressure, bend
    read(j, "verticalCCNumber", d.play.verticalController);
    d.play.verticalController = std::clamp(d.play.verticalController, 0, 127);
    read(j, "isStrikeVelocityEnabled", d.play.strikeVelocity);
    read(j, "strikeVelocityMin", d.play.strikeVelocityMin);
    d.play.strikeVelocityMin = std::clamp(d.play.strikeVelocityMin, 1, 126);
    read(j, "glide", d.play.glide);
    // Each expression message's scope, "note" or "channel". An earlier
    // KeyboardEDO wrote one switch for all three, perNoteExpression.
    bool perNote = true;
    read(j, "perNoteExpression", perNote);
    if (!perNote)
        d.play.bendScope = d.play.controllerScope = d.play.pressureScope = ExpressionScope::Channel;
    auto scope = [&](const char* key, ExpressionScope& value) {
        std::string text;
        read(j, key, text);
        if (text == "note") value = ExpressionScope::Note;
        else if (text == "channel") value = ExpressionScope::Channel;
    };
    scope("bendScope", d.play.bendScope);
    scope("controllerScope", d.play.controllerScope);
    scope("pressureScope", d.play.pressureScope);

    // MIDI
    int channel = d.output.channel, group = d.output.group;
    read(j, "midiChannel", channel);
    read(j, "midiGroup", group);
    d.output.channel = std::uint8_t(std::clamp(channel, 0, 15));
    d.output.group = std::uint8_t(std::clamp(group, 0, 15));
    read(j, "perNoteManagement", d.output.perNoteManagement);
    read(j, "midi1Expression", d.output.midi1Expression);
    read(j, "bendRange", d.output.bendRange);
    read(j, "perNoteBendRange", d.output.perNoteBendRange);
    read(j, "midiDestinationName", d.destination);

    // Everything else, kept as it was.
    static const std::set<std::string> known = {
        "edoSteps", "originNoteName", "originOctave", "keyLayoutViewType", "selectedPresetIndex",
        "pianoLowest", "pianoHighest", "tileColorMode", "locked", "dragBehavior", "horizontalControl",
        "verticalControl", "verticalCCNumber", "isStrikeVelocityEnabled", "strikeVelocityMin", "glide",
        "perNoteExpression", "bendScope", "controllerScope", "pressureScope", "midiChannel", "midiGroup", "perNoteManagement", "bendRange",
        "perNoteBendRange", "midiDestinationName", "midi1Expression",
    };
    for (const auto& [key, value] : j.items())
        if (!known.count(key)) d.extras[key] = value.dump();
    return d;
}

std::string writeDocument(const Document& d) {
    json j = keyEdoDefaults();
    for (const auto& [key, text] : d.extras) {
        try {
            j[key] = json::parse(text);
        } catch (const json::exception&) {
        }
    }

    j["edoSteps"] = d.tuning.edo;
    j["originNoteName"] = pitchClassName(d.tuning.originPitchClass);
    j["originOctave"] = d.tuning.originOctave;

    j["keyLayoutViewType"] = int(d.layout.kind);
    j["selectedPresetIndex"] = d.layout.generatorIndex;
    j["pianoLowest"] = d.layout.pianoLowest;
    j["pianoHighest"] = d.layout.pianoHighest;
    j["tileColorMode"] = kTileColors[int(d.tileColor)];
    j["locked"] = d.locked;

    j["dragBehavior"] = int(d.play.drag);
    j["horizontalControl"] = int(d.play.horizontal);
    j["verticalControl"] = int(d.play.vertical);
    j["verticalCCNumber"] = d.play.verticalController;
    j["isStrikeVelocityEnabled"] = d.play.strikeVelocity;
    j["strikeVelocityMin"] = d.play.strikeVelocityMin;
    j["glide"] = d.play.glide;
    auto scopeName = [](ExpressionScope s) { return s == ExpressionScope::Note ? "note" : "channel"; };
    j["bendScope"] = scopeName(d.play.bendScope);
    j["controllerScope"] = scopeName(d.play.controllerScope);
    j["pressureScope"] = scopeName(d.play.pressureScope);

    j["midiChannel"] = d.output.channel;
    j["midiGroup"] = d.output.group;
    j["perNoteManagement"] = d.output.perNoteManagement;
    j["midi1Expression"] = d.output.midi1Expression;
    j["bendRange"] = d.output.bendRange;
    j["perNoteBendRange"] = d.output.perNoteBendRange;
    if (d.destination.empty()) j.erase("midiDestinationName");
    else j["midiDestinationName"] = d.destination;

    return j.dump(2) + "\n";
}

std::optional<Document> loadDocument(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "can't open " + path;
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parseDocument(text.str(), error);
}

bool saveDocument(const std::string& path, const Document& document, std::string* error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << writeDocument(document);
    if (!out) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

} // namespace kedo
