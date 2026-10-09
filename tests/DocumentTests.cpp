// Documents: KeyEDO's files open, ours keep KeyEDO's keys, and what this
// version doesn't use survives a save.

#include <set>
#include <string>

#include <nlohmann/json.hpp>

#include "Check.h"
#include "kedo/Document.h"

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
    const auto d = parseDocument(kKeyEdoDocument, &error);
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
    const auto d = parseDocument(R"({"edoSteps": 31, "originNoteName": "C#", "verticalControl": 2,
        "dragBehavior": 1, "midiChannel": 9, "midiGroup": 3, "midiDestinationName": "IAC Driver Bus 1"})");
    CHECK(d && d->tuning.edo == 31 && d->tuning.originPitchClass == 1);
    CHECK(d->play.vertical == VerticalControl::Pressure);
    CHECK(d->play.drag == DragBehavior::Fixed);
    CHECK(d->output.channel == 9 && d->output.group == 3);
    CHECK(d->destination == "IAC Driver Bus 1");
}

void testRoundTrip() {
    Document d;
    d.tuning.edo = 19;
    d.tuning.originPitchClass = 2;
    d.layout.kind = LayoutKind::Tiled;
    d.layout.generatorIndex = 2;
    d.tileColor = TileColor::Octave;
    d.play.drag = DragBehavior::Fixed;
    d.play.vertical = VerticalControl::Controller;
    d.play.verticalController = 74;
    d.play.glide = true;
    d.play.perNoteExpression = false;
    d.output.channel = 4;
    d.output.perNoteManagement = false;
    d.output.perNoteBendRange = 48;
    d.destination = "Synth:In";
    d.publish = true;
    d.locked = true;
    d.extras["arpBpm"] = "96.0";
    const auto back = parseDocument(writeDocument(d));
    CHECK(back.has_value());
    if (back) {
        // tiledRows follows the window and isn't saved; KeyEDO's keys we
        // don't use come back as extras, with our own value kept.
        Document expected = d;
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
    const auto ours = nlohmann::json::parse(writeDocument(Document{}));
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

void testBadInput() {
    std::string error;
    CHECK(!parseDocument("not json", &error));
    CHECK(!error.empty());
    CHECK(!parseDocument("[1, 2]"));
    // Out of range or the wrong type: defaults or clamped.
    const auto d = parseDocument(R"({"edoSteps": 500, "originNoteName": "H", "keyLayoutViewType": 7,
        "verticalCCNumber": "seven", "midiChannel": 40})");
    CHECK(d && d->tuning.edo == Tuning::kMaxEdo);
    CHECK(d->tuning.originPitchClass == 9);
    CHECK(d->layout.kind == LayoutKind::Piano);
    CHECK(d->play.verticalController == 1);
    CHECK(d->output.channel == 15);
}

} // namespace

int main() {
    testOpensKeyEdo();
    testMidiFields();
    testRoundTrip();
    testWritesKeyEdoKeys();
    testBadInput();
    return kedo::test::finish("Document");
}
