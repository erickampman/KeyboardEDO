// UmpOutput: the MIDI 2.0 messages, and the MIDI 1.0 fallback.

#include <vector>

#include "Check.h"
#include "kedo/Tuning.h"
#include "kedo/UmpOutput.h"
#include "uw/ump/Ump.h"

using namespace kedo;
namespace ump = uw::ump;

namespace {

Tuning seventeen() {
    Tuning t;
    t.edo = 17;
    return t;
}

const Tuning T = seventeen();   // most tests play 17-EDO from A1

OutputConfig config(uw::MidiProtocol p, bool pnm = false) {
    OutputConfig c;
    c.group = 2;
    c.channel = 5;
    c.protocol = p;
    c.perNoteManagement = pnm;
    return c;
}

void testNoteOnMidi2() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.noteOn(T.note(17), 0x8000);   // A2
    const auto m = out.take();
    CHECK(m.size() == 1);
    CHECK(ump::messageType(m[0]) == ump::MessageType::Midi2ChannelVoice);
    CHECK(ump::group(m[0]) == 2);
    CHECK(ump::channel(m[0]) == 5);
    CHECK(ump::isNoteOn(m[0]));
    CHECK(ump::noteNumber(m[0]) == 17);        // steps numbered from the origin
    CHECK((m[0].words[0] & 0xFF) == 0x03);     // Pitch 7.9 attribute
    CHECK((m[0].words[1] >> 16) == 0x8000);
    CHECK((m[0].words[1] & 0xFFFF) == (45u << 9));   // A2 is MIDI note 45
    CHECK(out.take().empty());
}

void testNoteOnTwelve() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.noteOn(Tuning{}.note(36), 0xFFFF);
    const auto m = out.take();
    CHECK(m.size() == 1);
    CHECK(ump::noteNumber(m[0]) == 69);        // the standard note number
    CHECK((m[0].words[0] & 0xFF) == 0);        // no attribute needed
}

void testVelocityFloor() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.noteOn(T.note(0), 0);
    const auto m = out.take();
    CHECK((m[0].words[1] >> 16) == 1);   // MIDI 2.0 note-on 0 isn't a note-off
}

void testPerNoteManagement() {
    UmpOutput out(config(uw::MidiProtocol::Midi2, true));
    out.noteOn(T.note(3), 0xFFFF);
    const auto m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::status(m[0]) == ump::Status::PerNoteManagement);
    CHECK(ump::noteNumber(m[0]) == 3);
    CHECK((m[0].words[0] & 0x01) == 0x01);    // reset
    CHECK(ump::isNoteOn(m[1]));
}

void testNoteOffAndAllNotesOff() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.noteOn(T.note(1), 0xFFFF);
    out.noteOn(T.note(2), 0xFFFF);
    out.take();
    out.noteOff(1);
    auto m = out.take();
    CHECK(m.size() == 1 && ump::isNoteOff(m[0]) && ump::noteNumber(m[0]) == 1);
    out.noteOff(1);   // not sounding: nothing
    CHECK(out.take().empty());
    out.allNotesOff();
    m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::isNoteOff(m[0]) && ump::noteNumber(m[0]) == 2);
    CHECK(ump::status(m[1]) == ump::Status::ControlChange);
    CHECK(((m[1].words[0] >> 8) & 0x7F) == 123);
}

void testPerNoteExpression() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.noteOn(T.note(4), 0xFFFF);
    out.take();

    out.pitchBend(4, 0.0);
    out.pitchBend(4, 1.0);
    out.pitchBend(4, -1.0);
    auto m = out.take();
    CHECK(m.size() == 3);
    CHECK(ump::status(m[0]) == ump::Status::PerNotePitchBend);
    CHECK(ump::noteNumber(m[0]) == 4);
    CHECK(m[0].words[1] == 0x80000000u);
    CHECK(m[1].words[1] == 0xFFFFFFFFu);
    CHECK(m[2].words[1] == 0);

    out.controller(4, 74, 1.0);
    out.pressure(4, 0.5);
    m = out.take();
    CHECK(ump::status(m[0]) == ump::Status::RegisteredPerNoteController);
    CHECK((m[0].words[0] & 0xFF) == 74);
    CHECK(m[0].words[1] == 0xFFFFFFFFu);
    CHECK(ump::status(m[1]) == ump::Status::PolyPressure);

    // Per-note messages for a note that isn't sounding go nowhere.
    out.pitchBend(9, 0.5);
    CHECK(out.take().empty());

    // Channel-wide: 32-bit channel messages.
    out.pitchBend(std::nullopt, 0.5);
    out.controller(std::nullopt, 1, 0.5);
    m = out.take();
    CHECK(ump::status(m[0]) == ump::Status::PitchBend);
    CHECK(ump::messageType(m[0]) == ump::MessageType::Midi2ChannelVoice);
    CHECK(ump::status(m[1]) == ump::Status::ControlChange);
}

void testGlide() {
    const Tuning t = seventeen();
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.noteOn(T.note(0), 0xFFFF);
    out.take();
    out.glide(0, t.semitones(5));
    const auto m = out.take();
    CHECK(m.size() == 1);
    CHECK(ump::status(m[0]) == ump::Status::RegisteredPerNoteController);
    CHECK(ump::noteNumber(m[0]) == 0);           // the note keeps its name
    CHECK((m[0].words[0] & 0xFF) == 3);          // Pitch 7.25
    CHECK(m[0].words[1] == encodePitch7_25(t.semitones(5)));
}

void testMidi1Fallback() {
    UmpOutput out(config(uw::MidiProtocol::Midi1, true));
    out.noteOn(T.note(17), 0xFFFF);   // A2
    auto m = out.take();
    CHECK(m.size() == 1);     // no Per-Note Management in MIDI 1.0
    CHECK(ump::messageType(m[0]) == ump::MessageType::Midi1ChannelVoice);
    CHECK(ump::noteNumber(m[0]) == 45);           // nearest 12-EDO note
    CHECK((m[0].words[0] & 0x7F) == 127);

    // Per-note bend becomes channel bend; 14-bit centre is 0x2000: LSB 0
    // first, then MSB 0x40.
    out.pitchBend(17, 0.0);
    m = out.take();
    CHECK(m.size() == 1 && ump::status(m[0]) == ump::Status::PitchBend);
    CHECK(((m[0].words[0] >> 8) & 0x7F) == 0x00 && (m[0].words[0] & 0x7F) == 0x40);

    // A glide restrikes at the nearest note, and the note-off follows it.
    out.glide(17, T.semitones(18));   // 1 step up: 70.6 cents, nearest A#2 (46)
    m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::isNoteOff(m[0]) && ump::noteNumber(m[0]) == 45);
    CHECK(ump::isNoteOn(m[1]) && ump::noteNumber(m[1]) == 46);
    out.noteOff(17);
    m = out.take();
    CHECK(m.size() == 1 && ump::noteNumber(m[0]) == 46);
}

// Notes no tuning would make: the custom layout's (decisions.md, "Keys carry
// the note they play").
void testAnyNote() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    Note n;
    n.id = 1000;
    n.number = 60;
    n.pitch = 61.5;   // a quarter tone above C#4, on note number 60
    out.noteOn(n, 0xFFFF);
    auto m = out.take();
    CHECK(m.size() == 1 && ump::noteNumber(m[0]) == 60);
    CHECK((m[0].words[0] & 0xFF) == 0x03);
    CHECK((m[0].words[1] & 0xFFFF) == encodePitch7_9(61.5));

    // Its own pitch on its own number: a plain note-on, at any EDO.
    Note plain;
    plain.id = 1001;
    plain.number = 64;
    plain.pitch = 64.0;
    out.noteOn(plain, 0xFFFF);
    m = out.take();
    CHECK(m.size() == 1 && (m[0].words[0] & 0xFF) == 0);

    // Its own group and channel, for the note and everything addressed to it.
    Note routed;
    routed.id = 1002;
    routed.number = 70;
    routed.pitch = 70.25;
    routed.group = 3;
    routed.channel = 9;
    out.noteOn(routed, 0xFFFF);
    out.pitchBend(1002, 0.5);
    out.glide(1002, 72.0);
    out.noteOff(1002);
    m = out.take();
    CHECK(m.size() == 4);
    for (const auto& msg : m) CHECK(ump::group(msg) == 3 && ump::channel(msg) == 9 && ump::noteNumber(msg) == 70);
    // Channel-wide expression stays on the output's channel.
    out.pitchBend(std::nullopt, 0.5);
    m = out.take();
    CHECK(ump::group(m[0]) == 2 && ump::channel(m[0]) == 5);

    // All Notes Off reaches every channel a note used, and the output's.
    out.noteOn(routed, 0xFFFF);
    out.take();
    out.allNotesOff();
    m = out.take();
    int cc123 = 0;
    bool routedOff = false;
    for (const auto& msg : m) {
        if (ump::status(msg) == ump::Status::ControlChange) ++cc123;
        if (ump::isNoteOff(msg) && ump::channel(msg) == 9) routedOff = true;
    }
    CHECK(cc123 == 2);
    CHECK(routedOff);

    // MIDI 1.0: the nearest 12-EDO note to the pitch, not the number.
    UmpOutput out1(config(uw::MidiProtocol::Midi1));
    out1.noteOn(n, 0xFFFF);
    m = out1.take();
    CHECK(m.size() == 1 && ump::noteNumber(m[0]) == 62);   // 61.5 rounds to 62
}

void testSetup() {
    UmpOutput out(config(uw::MidiProtocol::Midi2));
    out.setup();
    auto m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::status(m[0]) == ump::Status::RegisteredController);
    CHECK(((m[0].words[0] >> 8) & 0x7F) == 0 && (m[0].words[0] & 0x7F) == 0);
    CHECK(m[0].words[1] == (2u << 25));
    CHECK((m[1].words[0] & 0x7F) == 7);   // per-note pitch bend sensitivity

    UmpOutput out1(config(uw::MidiProtocol::Midi1));
    out1.setup();
    m = out1.take();
    CHECK(m.size() == 4);   // CC 101, 100, 6, 38
}

} // namespace

int main() {
    testNoteOnMidi2();
    testNoteOnTwelve();
    testVelocityFloor();
    testPerNoteManagement();
    testNoteOffAndAllNotesOff();
    testPerNoteExpression();
    testGlide();
    testMidi1Fallback();
    testAnyNote();
    testSetup();
    return kedo::test::finish("UmpOutput");
}
