#include "kedo/Document.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
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

// A custom layout's key (decisions.md, "Custom layout"). Group and channel
// are 0-based, as midiGroup and midiChannel, and left out when they're the
// layout's; a fill colour is "#rrggbb".
json customKeyJson(const CustomKey& k) {
    json j = {{"x", k.center.x}, {"y", k.center.y},
              {"shape", k.shape == KeyShape::Diamond ? "diamond" : "rectangle"},
              {"shade", k.shade == KeyShade::Black ? "black" : "white"},
              {"edo", k.edo}, {"octave", k.octave}, {"degree", k.degree},
              {"number", k.number}, {"numberChosen", k.numberChosen}};
    if (k.halfWidth > 0) j["halfWidth"] = k.halfWidth;
    if (k.halfHeight > 0) j["halfHeight"] = k.halfHeight;
    if (k.color) {
        char hex[8];
        std::snprintf(hex, sizeof hex, "#%06x", unsigned(*k.color & 0xFFFFFF));
        j["color"] = hex;
    }
    if (!k.label.empty()) j["label"] = k.label;
    if (!k.sublabel.empty()) j["sublabel"] = k.sublabel;
    if (k.group) j["group"] = *k.group;
    if (k.channel) j["channel"] = *k.channel;
    return j;
}

CustomKey customKeyFrom(const json& j) {
    CustomKey k;
    if (!j.is_object()) return k;
    read(j, "x", k.center.x);
    read(j, "y", k.center.y);
    std::string text;
    read(j, "shape", text);
    k.shape = text == "rectangle" ? KeyShape::Rectangle : KeyShape::Diamond;
    text.clear();
    read(j, "shade", text);
    k.shade = text == "black" ? KeyShade::Black : KeyShade::White;
    read(j, "halfWidth", k.halfWidth);
    read(j, "halfHeight", k.halfHeight);
    k.halfWidth = std::clamp(k.halfWidth, 0.0, 10000.0);
    k.halfHeight = std::clamp(k.halfHeight, 0.0, 10000.0);
    text.clear();
    read(j, "color", text);
    unsigned rgb = 0;
    if (text.size() == 7 && text[0] == '#' && std::sscanf(text.c_str() + 1, "%6x", &rgb) == 1) k.color = rgb;
    read(j, "label", k.label);
    read(j, "sublabel", k.sublabel);
    read(j, "edo", k.edo);
    k.edo = std::clamp(k.edo, 1, 1200);
    read(j, "octave", k.octave);
    read(j, "degree", k.degree);
    read(j, "number", k.number);
    k.number = std::clamp(k.number, 0, 127);
    read(j, "numberChosen", k.numberChosen);
    int route = -1;
    read(j, "group", route);
    if (route >= 0 && route < 16) k.group = std::uint8_t(route);
    route = -1;
    read(j, "channel", route);
    if (route >= 0 && route < 16) k.channel = std::uint8_t(route);
    return k;
}

} // namespace

std::optional<LayoutSetup> parseLayoutSetup(const std::string& text, std::string* error) {
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

    LayoutSetup d;
    read(j, "name", d.name);

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
    // A custom layout says it's Tiled, for KeyEDO; its keys make it custom.
    if (const auto it = j.find("customKeys"); it != j.end() && it->is_array()) {
        d.layout.kind = LayoutKind::Custom;
        for (const auto& key : *it) d.layout.customKeys.push_back(customKeyFrom(key));
    }
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
        "name", "edoSteps", "originNoteName", "originOctave", "keyLayoutViewType", "selectedPresetIndex",
        "pianoLowest", "pianoHighest", "tileColorMode", "locked", "dragBehavior", "horizontalControl",
        "verticalControl", "verticalCCNumber", "isStrikeVelocityEnabled", "strikeVelocityMin", "glide",
        "perNoteExpression", "bendScope", "controllerScope", "pressureScope", "midiChannel", "midiGroup", "perNoteManagement", "bendRange",
        "perNoteBendRange", "midiDestinationName", "midi1Expression", "customKeys",
    };
    for (const auto& [key, value] : j.items())
        if (!known.count(key)) d.extras[key] = value.dump();
    return d;
}

std::string writeLayoutSetup(const LayoutSetup& d) {
    json j = keyEdoDefaults();
    for (const auto& [key, text] : d.extras) {
        try {
            j[key] = json::parse(text);
        } catch (const json::exception&) {
        }
    }

    if (d.name.empty()) j.erase("name");
    else j["name"] = d.name;
    j["edoSteps"] = d.tuning.edo;
    j["originNoteName"] = pitchClassName(d.tuning.originPitchClass);
    j["originOctave"] = d.tuning.originOctave;

    j["keyLayoutViewType"] = d.layout.kind == LayoutKind::Custom ? int(LayoutKind::Tiled) : int(d.layout.kind);
    if (d.layout.kind == LayoutKind::Custom) {
        json keys = json::array();
        for (const auto& k : d.layout.customKeys) keys.push_back(customKeyJson(k));
        j["customKeys"] = keys;
    }
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

std::optional<LayoutSetup> loadLayoutSetup(const std::string& path, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "can't open " + path;
        return std::nullopt;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parseLayoutSetup(text.str(), error);
}

bool saveLayoutSetup(const std::string& path, const LayoutSetup& document, std::string* error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << writeLayoutSetup(document);
    if (!out) {
        if (error) *error = "can't write " + path;
        return false;
    }
    return true;
}

// --- Documents: .kedo bundles ------------------------------------------------------

namespace {

namespace fs = std::filesystem;

bool readFile(const fs::path& path, std::string& text, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "can't open " + path.string();
        return false;
    }
    std::ostringstream all;
    all << in.rdbuf();
    text = all.str();
    return true;
}

bool writeFile(const fs::path& path, const std::string& text, std::string* error) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) out << text;
    if (!out) {
        if (error) *error = "can't write " + path.string();
        return false;
    }
    return true;
}

bool isBundle(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec) && fs::is_regular_file(path / "document.json", ec);
}

// A file name for a layout: its name, lower case, with anything but letters
// and digits as one '-' ("Wicki 31" -> "wicki-31"); "layout" if that leaves
// nothing.
std::string slug(const std::string& name) {
    std::string out;
    for (const unsigned char c : name) {
        if (std::isalnum(c)) out += char(std::tolower(c));
        else if (!out.empty() && out.back() != '-') out += '-';
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "layout" : out;
}

std::optional<Document> loadBundle(const fs::path& path, std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = path.filename().string() + ": " + why;
        return std::nullopt;
    };
    std::string text;
    if (!readFile(path / "document.json", text, error)) return std::nullopt;
    json j;
    try {
        j = json::parse(text);
    } catch (const json::exception& e) {
        return fail(std::string("document.json: ") + e.what());
    }
    if (!j.is_object() || j.value("format", std::string()) != kDocumentFormat)
        return fail("not a KeyboardEDO document");
    const int version = j.value("version", 0);
    if (version > kDocumentVersion)
        return fail("made by a newer KeyboardEDO (format version " + std::to_string(version) + ")");

    // The listed layouts in order, then any others in layouts/, by name.
    std::vector<std::string> files;
    if (const auto it = j.find("layouts"); it != j.end() && it->is_array())
        for (const auto& f : *it)
            if (f.is_string()) files.push_back(f.get<std::string>());
    std::vector<std::string> unlisted;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(path / "layouts", ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        const std::string file = "layouts/" + entry.path().filename().string();
        if (std::find(files.begin(), files.end(), file) == files.end()) unlisted.push_back(file);
    }
    std::sort(unlisted.begin(), unlisted.end());
    files.insert(files.end(), unlisted.begin(), unlisted.end());

    Document d;
    d.layouts.clear();
    for (const auto& file : files) {
        std::string layoutText, why;
        if (!readFile(path / file, layoutText, error)) return std::nullopt;
        auto layout = parseLayoutSetup(layoutText, &why);
        if (!layout) return fail(file + ": " + why);
        if (layout->name.empty()) layout->name = fs::path(file).stem().string();
        layout->destination.clear();   // the document's, below
        d.layouts.push_back(std::move(*layout));
    }
    if (d.layouts.empty()) return fail("no layouts");
    d.current = std::clamp(j.value("current", 0), 0, int(d.layouts.size()) - 1);
    d.destination = j.value("midiDestinationName", std::string());
    return d;
}

} // namespace

bool isBundlePath(const std::string& path) {
    return fs::path(path).extension() == ".kedo" || isBundle(path);
}

std::optional<Document> loadDocument(const std::string& path, std::string* error) {
    std::error_code ec;
    if (fs::is_directory(path, ec)) {
        if (!isBundle(path)) {
            if (error) *error = path + ": not a KeyboardEDO document (no document.json)";
            return std::nullopt;
        }
        return loadBundle(path, error);
    }
    // A KeyEDO document: one layout.
    auto layout = loadLayoutSetup(path, error);
    if (!layout) return std::nullopt;
    if (layout->name.empty()) layout->name = fs::path(path).stem().string();
    Document d;
    d.destination = layout->destination;
    layout->destination.clear();
    d.layouts = {std::move(*layout)};
    return d;
}

bool saveDocument(const std::string& pathText, const Document& document, std::string* error) {
    const fs::path path(pathText);
    std::error_code ec;
    if (fs::exists(path, ec) && !isBundle(path)) {
        if (error) *error = pathText + " exists and isn't a KeyboardEDO document: not replaced";
        return false;
    }
    if (document.layouts.empty()) {
        if (error) *error = "a document needs a layout";
        return false;
    }
    const fs::path tmp = pathText + ".tmp", old = pathText + ".old";
    fs::remove_all(tmp, ec);   // a save that didn't finish
    if (!fs::create_directories(tmp / "layouts", ec)) {
        if (error) *error = "can't make " + tmp.string() + ": " + ec.message();
        return false;
    }

    json j;
    j["format"] = kDocumentFormat;
    j["version"] = kDocumentVersion;
    j["current"] = std::clamp(document.current, 0, int(document.layouts.size()) - 1);
    if (!document.destination.empty()) j["midiDestinationName"] = document.destination;
    json files = json::array();
    std::set<std::string> used;
    for (const auto& layout : document.layouts) {
        std::string file = slug(layout.name);
        for (int n = 2; used.count(file); ++n) file = slug(layout.name) + "-" + std::to_string(n);
        used.insert(file);
        file = "layouts/" + file + ".json";
        LayoutSetup written = layout;
        written.destination.clear();   // the document's
        if (!writeFile(tmp / file, writeLayoutSetup(written), error)) return false;
        files.push_back(file);
    }
    j["layouts"] = files;
    if (!writeFile(tmp / "document.json", j.dump(2) + "\n", error)) return false;

    // The swap: the old bundle aside, the new one in, the old one gone.
    fs::remove_all(old, ec);
    const bool replacing = fs::exists(path, ec);
    if (replacing) fs::rename(path, old, ec);
    if (!ec) fs::rename(tmp, path, ec);
    if (ec) {
        if (error) *error = "can't replace " + pathText + ": " + ec.message();
        return false;
    }
    if (replacing) fs::remove_all(old, ec);
    return true;
}

} // namespace kedo
