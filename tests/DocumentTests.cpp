// Documents: KeyEDO's files open, ours keep KeyEDO's keys, and what this
// version doesn't use survives a save; .kedo bundles of several layouts.

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "Check.h"
#include "kedo/Document.h"
#include "kedo/Preferences.h"

using namespace kedo;

namespace {

// A KeyEDO document as KeyEDO writes it (one of the author's, from iCloud).
const char* const kKeyEdoDocument = R"({
  "arpBpm" : 120,
  "arpDivision" : "1\/16",
  "arpGate" : 0.5,
  "arpHold" : false,
  "arpIsEnabled" : true,
  "arpOctaveSpan" : 1,
  "arpPattern" : "Down-Up",
  "arpRateSource" : "Internal",
  "arpVelocitySource" : "As Played",
  "dragBehavior" : 0,
  "edoSteps" : 12,
  "horizontalControl" : 0,
  "hoverExpressionEnabled" : false,
  "isStrikeVelocityEnabled" : false,
  "keyLayoutViewType" : 1,
  "originNoteName" : "A",
  "originOctave" : 1,
  "selectedPresetIndex" : 0,
  "strikeVelocityMin" : 32,
  "tileColorMode" : "Note",
  "verticalCCNumber" : 1,
  "verticalControl" : 0
})";

void testOpensKeyEdo() {
    std::string error;
    const auto d = parseLayoutSetup(kKeyEdoDocument, &error);
    CHECK(d.has_value());
    if (!d) return;
    CHECK(d->tuning.edo == 12);
    CHECK(d->tuning.originPitchClass == 9);
    CHECK(d->tuning.originOctave == 1);
    CHECK(d->layout.kind == LayoutKind::Tiled);
    CHECK(d->tileColor == TileColor::Note);
    CHECK(d->play.drag == DragBehavior::Gliss);
    CHECK(d->play.strikeVelocityMin == 32);
    // Older KeyEDO documents have no MIDI fields: defaults.
    CHECK(d->output.channel == 0 && d->output.group == 0);
    CHECK(d->destination.empty());
    // The arpeggiator isn't here yet, but its settings are kept.
    CHECK(d->extras.count("arpPattern") && d->extras.at("arpPattern") == "\"Down-Up\"");
    CHECK(d->extras.count("arpIsEnabled") && d->extras.at("arpIsEnabled") == "true");
}

void testMidiFields() {
    const auto d = parseLayoutSetup(R"({"edoSteps": 31, "originNoteName": "C#", "verticalControl": 2,
        "dragBehavior": 1, "midiChannel": 9, "midiGroup": 3, "midiDestinationName": "IAC Driver Bus 1"})");
    CHECK(d && d->tuning.edo == 31 && d->tuning.originPitchClass == 1);
    CHECK(d->play.vertical == VerticalControl::Pressure);
    CHECK(d->play.drag == DragBehavior::Fixed);
    CHECK(d->output.channel == 9 && d->output.group == 3);
    CHECK(d->destination == "IAC Driver Bus 1");
}

void testRoundTrip() {
    LayoutSetup d;
    d.tuning.edo = 19;
    d.tuning.originPitchClass = 2;
    d.layout.kind = LayoutKind::Tiled;
    d.layout.generatorIndex = 2;
    d.tileColor = TileColor::Octave;
    d.play.drag = DragBehavior::Fixed;
    d.play.vertical = VerticalControl::Controller;
    d.play.verticalController = 74;
    d.play.glide = true;
    d.play.bendScope = ExpressionScope::Channel;
    d.play.pressureScope = ExpressionScope::Channel;
    d.output.channel = 4;
    d.output.perNoteManagement = false;
    d.output.perNoteBendRange = 48;
    d.output.midi1Expression = false;
    d.destination = "Synth:In";
    d.locked = true;
    d.extras["arpBpm"] = "96.0";
    const auto back = parseLayoutSetup(writeLayoutSetup(d));
    CHECK(back.has_value());
    if (back) {
        // tiledRows follows the window and isn't saved; KeyEDO's keys we
        // don't use come back as extras, with our own value kept.
        LayoutSetup expected = d;
        expected.layout.tiledRows = back->layout.tiledRows;
        expected.extras = back->extras;
        CHECK(*back == expected);
        CHECK(back->extras.count("arpBpm") && back->extras.at("arpBpm") == "96.0");
        CHECK(back->extras.count("arpPattern"));
    }
}

void testWritesKeyEdoKeys() {
    // Every key in KeyEDO's own document, so KeyEDO can open ours.
    const auto keyEdo = nlohmann::json::parse(kKeyEdoDocument);
    const auto ours = nlohmann::json::parse(writeLayoutSetup(LayoutSetup{}));
    for (const auto& [key, value] : keyEdo.items()) {
        CHECK(ours.contains(key));
        if (ours.contains(key)) CHECK(ours[key].type() == value.type() ||
                                      (ours[key].is_number() && value.is_number()));
    }
    // With KeyEDO's defaults for what we don't use.
    CHECK(ours["arpPattern"] == "Up");
    // No destination: no key, as KeyEDO's optional.
    CHECK(!ours.contains("midiDestinationName"));
}

void testScopes() {
    // An earlier KeyboardEDO's single switch, off: everything for the channel.
    auto d = parseLayoutSetup(R"({"perNoteExpression": false})");
    CHECK(d && d->play.bendScope == ExpressionScope::Channel && d->play.controllerScope == ExpressionScope::Channel &&
          d->play.pressureScope == ExpressionScope::Channel);
    // Each on its own.
    d = parseLayoutSetup(R"({"bendScope": "channel", "controllerScope": "note", "pressureScope": "bogus"})");
    CHECK(d && d->play.bendScope == ExpressionScope::Channel && d->play.controllerScope == ExpressionScope::Note &&
          d->play.pressureScope == ExpressionScope::Note);
    const auto j = nlohmann::json::parse(writeLayoutSetup(*d));
    CHECK(j["bendScope"] == "channel" && j["controllerScope"] == "note" && !j.contains("perNoteExpression"));
}

void testBadInput() {
    std::string error;
    CHECK(!parseLayoutSetup("not json", &error));
    CHECK(!error.empty());
    CHECK(!parseLayoutSetup("[1, 2]"));
    // Out of range or the wrong type: defaults or clamped.
    const auto d = parseLayoutSetup(R"({"edoSteps": 500, "originNoteName": "H", "keyLayoutViewType": 7,
        "verticalCCNumber": "seven", "midiChannel": 40})");
    CHECK(d && d->tuning.edo == Tuning::kMaxEdo);
    CHECK(d->tuning.originPitchClass == 9);
    CHECK(d->layout.kind == LayoutKind::Piano);
    CHECK(d->play.verticalController == 1);
    CHECK(d->output.channel == 15);
}

void testPreferences() {
    const std::string path = "kedo-test-prefs.json";   // in the test's working directory
    std::remove(path.c_str());
    CHECK(!loadPreferences(path).publish);   // none yet: defaults
    CHECK(loadPreferences(path).panelOpen && !loadPreferences(path).monitorOpen);
    CHECK(savePreferences(path, {.publish = true, .panelOpen = false, .monitorOpen = true}));
    const Preferences back = loadPreferences(path);
    CHECK(back.publish && !back.panelOpen && back.monitorOpen);
    std::remove(path.c_str());
}

namespace fs = std::filesystem;

// Equal but for extras: KeyEDO's keys we don't use come back from a layout
// file as extras (testRoundTrip).
bool sameDocument(const std::optional<Document>& a, const Document& b) {
    if (!a) return false;
    Document x = *a, y = b;
    for (auto* d : {&x, &y})
        for (auto& l : d->layouts) l.extras.clear();
    return x == y;
}

// Bundles, in the test's working directory.
void testBundleRoundTrip() {
    const std::string path = "kedo-test.kedo";
    fs::remove_all(path);
    Document d;
    d.layouts[0].name = "Wicki 31";
    d.layouts[0].tuning.edo = 31;
    d.layouts[0].layout.kind = LayoutKind::Tiled;
    LayoutSetup piano;
    piano.name = "Piano";
    piano.output.channel = 3;
    d.layouts.push_back(piano);
    LayoutSetup same;   // a second "Piano": its own file
    same.name = "Piano";
    same.tuning.edo = 19;
    d.layouts.push_back(same);
    d.current = 1;
    d.destination = "Synth";

    std::string error;
    CHECK(saveDocument(path, d, &error));
    CHECK(error.empty());
    CHECK(fs::is_regular_file(fs::path(path) / "document.json"));
    CHECK(fs::is_regular_file(fs::path(path) / "layouts" / "wicki-31.json"));
    CHECK(fs::is_regular_file(fs::path(path) / "layouts" / "piano.json"));
    CHECK(fs::is_regular_file(fs::path(path) / "layouts" / "piano-2.json"));
    CHECK(!fs::exists(path + ".tmp") && !fs::exists(path + ".old"));
    const auto back = loadDocument(path, &error);
    CHECK(sameDocument(back, d));

    // A layout file is a KeyEDO document: it opens on its own, without the
    // document's destination.
    const auto alone = loadLayoutSetup(path + "/layouts/piano.json");
    CHECK(alone && alone->name == "Piano" && alone->output.channel == 3 && alone->destination.empty());

    // Saving again replaces it, and a save that never finished (a stale .tmp)
    // doesn't get in the way.
    fs::create_directories(path + ".tmp/layouts");
    d.layouts.pop_back();
    CHECK(saveDocument(path, d, &error));
    CHECK(!fs::exists(path + ".tmp") && !fs::exists(fs::path(path) / "layouts" / "piano-2.json"));
    CHECK(sameDocument(loadDocument(path), d));

    // A layout file copied in from elsewhere comes after the listed ones.
    LayoutSetup copied;
    copied.tuning.edo = 22;
    std::ofstream(path + "/layouts/zz-copied.json") << writeLayoutSetup(copied);
    const auto withCopy = loadDocument(path);
    CHECK(withCopy && withCopy->layouts.size() == 3);
    CHECK(withCopy && withCopy->layouts[2].tuning.edo == 22 && withCopy->layouts[2].name == "zz-copied");
    fs::remove_all(path);
}

void testBundleRefusals() {
    std::string error;
    // Not over a file that isn't a bundle, nor a folder.
    const std::string file = "kedo-test-file.kedo";
    std::ofstream(file) << "precious";
    CHECK(!saveDocument(file, Document{}, &error) && !error.empty());
    std::ifstream in(file);
    std::string kept;
    in >> kept;
    CHECK(kept == "precious");
    std::remove(file.c_str());
    fs::create_directories("kedo-test-folder");
    CHECK(!saveDocument("kedo-test-folder", Document{}));
    CHECK(!loadDocument("kedo-test-folder"));
    fs::remove_all("kedo-test-folder");

    // A newer version, or another format, is refused, not guessed at.
    const std::string path = "kedo-test-newer.kedo";
    fs::remove_all(path);
    CHECK(saveDocument(path, Document{}));
    std::ofstream(path + "/document.json") << R"({"format": "keyboardedo-document", "version": 99})";
    error.clear();
    CHECK(!loadDocument(path, &error) && error.find("newer") != std::string::npos);
    std::ofstream(path + "/document.json") << R"({"format": "stagehand-rack", "version": 1})";
    CHECK(!loadDocument(path));
    fs::remove_all(path);
}

// A custom layout's keys, in its layout file; KeyEDO is told it's Tiled.
void testCustomKeys() {
    LayoutSetup d;
    d.layout.kind = LayoutKind::Custom;
    CustomKey a;
    a.center = {42.5, -10};
    a.edo = 31;
    a.octave = 3;
    a.degree = 17;
    a.number = 99;
    a.numberChosen = true;
    a.color = 0x12ab34;
    a.label = "D#";
    a.sublabel = "17";
    a.group = 2;
    a.channel = 9;
    CustomKey b;
    b.shade = KeyShade::Black;
    b.halfWidth = 11.5;
    b.halfHeight = 55;
    d.layout.customKeys = {a, b};
    const std::string text = writeLayoutSetup(d);
    const auto j = nlohmann::json::parse(text);
    CHECK(j["keyLayoutViewType"] == 1);
    CHECK(j["customKeys"].size() == 2 && j["customKeys"][0]["color"] == "#12ab34");
    CHECK(!j["customKeys"][1].contains("group") && !j["customKeys"][1].contains("color"));
    const auto back = parseLayoutSetup(text);
    CHECK(back && back->layout.kind == LayoutKind::Custom && back->layout.customKeys == d.layout.customKeys);

    // Without customKeys, a Tiled file stays Tiled; generated layouts write none.
    LayoutSetup tiled;
    tiled.layout.kind = LayoutKind::Tiled;
    CHECK(!nlohmann::json::parse(writeLayoutSetup(tiled)).contains("customKeys"));
    CHECK(parseLayoutSetup(writeLayoutSetup(tiled))->layout.kind == LayoutKind::Tiled);
    // Bad values are clamped, and a key that isn't an object is a default one.
    const auto bad = parseLayoutSetup(R"({"customKeys": [{"number": 400, "channel": 99, "edo": 0}, 7]})");
    CHECK(bad && bad->layout.customKeys.size() == 2);
    CHECK(bad && bad->layout.customKeys[0].number == 127 && !bad->layout.customKeys[0].channel &&
          bad->layout.customKeys[0].edo == 1);
}

void testImportsKeyEdo() {
    // A KeyEDO file: a document of one layout, named after the file, its
    // destination the document's.
    const std::string path = "Kedo Test Import.json";
    std::ofstream(path) << kKeyEdoDocument;
    const auto d = loadDocument(path);
    CHECK(d && d->layouts.size() == 1 && d->current == 0);
    CHECK(d && d->layout().name == "Kedo Test Import" && d->layout().destination.empty());
    const auto setup = parseLayoutSetup(kKeyEdoDocument);
    CHECK(d && setup && d->destination == setup->destination);
    std::remove(path.c_str());
}

} // namespace

int main() {
    testOpensKeyEdo();
    testMidiFields();
    testRoundTrip();
    testWritesKeyEdoKeys();
    testScopes();
    testBadInput();
    testPreferences();
    testBundleRoundTrip();
    testBundleRefusals();
    testImportsKeyEdo();
    testCustomKeys();
    return kedo::test::finish("Document");
}
