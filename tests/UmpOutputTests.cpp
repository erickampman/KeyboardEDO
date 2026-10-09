// UmpOutput: the MIDI 2.0 messages, and the MIDI 1.0 fallback.

#include <vector>

#include "Check.h"
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

OutputConfig config(uw::MidiProtocol p, bool pnm = false) {
    OutputConfig c;
    c.group = 2;
    c.channel = 5;
    c.protocol = p;
    c.perNoteManagement = pnm;
    return c;
}

void testNoteOnMidi2() {
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi2));
    out.noteOn(17, 0x8000);   // A2
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
    UmpOutput out(Tuning{}, config(uw::MidiProtocol::Midi2));
    out.noteOn(36, 0xFFFF);
    const auto m = out.take();
    CHECK(m.size() == 1);
    CHECK(ump::noteNumber(m[0]) == 69);        // the standard note number
    CHECK((m[0].words[0] & 0xFF) == 0);        // no attribute needed
}

void testVelocityFloor() {
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi2));
    out.noteOn(0, 0);
    const auto m = out.take();
    CHECK((m[0].words[1] >> 16) == 1);   // MIDI 2.0 note-on 0 isn't a note-off
}

void testPerNoteManagement() {
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi2, true));
    out.noteOn(3, 0xFFFF);
    const auto m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::status(m[0]) == ump::Status::PerNoteManagement);
    CHECK(ump::noteNumber(m[0]) == 3);
    CHECK((m[0].words[0] & 0x01) == 0x01);    // reset
    CHECK(ump::isNoteOn(m[1]));
}

void testNoteOffAndAllNotesOff() {
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi2));
    out.noteOn(1, 0xFFFF);
    out.noteOn(2, 0xFFFF);
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
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi2));
    out.noteOn(4, 0xFFFF);
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
    UmpOutput out(t, config(uw::MidiProtocol::Midi2));
    out.noteOn(0, 0xFFFF);
    out.take();
    out.glide(0, 5);
    const auto m = out.take();
    CHECK(m.size() == 1);
    CHECK(ump::status(m[0]) == ump::Status::RegisteredPerNoteController);
    CHECK(ump::noteNumber(m[0]) == 0);           // the note keeps its name
    CHECK((m[0].words[0] & 0xFF) == 3);          // Pitch 7.25
    CHECK(m[0].words[1] == encodePitch7_25(t.semitones(5)));
}

void testMidi1Fallback() {
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi1, true));
    out.noteOn(17, 0xFFFF);   // A2
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
    out.glide(17, 18);   // 1 step up: 70.6 cents, nearest A#2 (46)
    m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::isNoteOff(m[0]) && ump::noteNumber(m[0]) == 45);
    CHECK(ump::isNoteOn(m[1]) && ump::noteNumber(m[1]) == 46);
    out.noteOff(17);
    m = out.take();
    CHECK(m.size() == 1 && ump::noteNumber(m[0]) == 46);
}

void testSetup() {
    UmpOutput out(seventeen(), config(uw::MidiProtocol::Midi2));
    out.setup();
    auto m = out.take();
    CHECK(m.size() == 2);
    CHECK(ump::status(m[0]) == ump::Status::RegisteredController);
    CHECK(((m[0].words[0] >> 8) & 0x7F) == 0 && (m[0].words[0] & 0x7F) == 0);
    CHECK(m[0].words[1] == (2u << 25));
    CHECK((m[1].words[0] & 0x7F) == 7);   // per-note pitch bend sensitivity

    UmpOutput out1(seventeen(), config(uw::MidiProtocol::Midi1));
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
    testSetup();
    return kedo::test::finish("UmpOutput");
}
