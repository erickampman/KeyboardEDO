#include "kedo/UmpOutput.h"

#include <algorithm>
#include <cmath>

#include "uw/ump/Ump.h"

namespace kedo {

namespace {

using uw::ump::Status;

constexpr std::uint8_t st(Status s) { return std::uint8_t(s); }

// MIDI 2.0 bipolar 32-bit value: 0x80000000 is centre.
std::uint32_t bipolar32(double v) {
    v = std::clamp(v, -1.0, 1.0);
    const double span = v >= 0 ? double(0x7FFFFFFF) : double(0x80000000u);
    return std::uint32_t(std::llround(double(0x80000000u) + v * span));
}

std::uint32_t unipolar32(double v) {
    return std::uint32_t(std::llround(std::clamp(v, 0.0, 1.0) * double(0xFFFFFFFFu)));
}

// Registered controller 0 (and 7) data: semitones in bits 31-25, cents in
// 24-18, as the MIDI 2.0 translation of CC 6 and CC 38.
std::uint32_t bendRangeValue(double semitones) {
    semitones = std::clamp(semitones, 0.0, 127.0);
    const int whole = int(semitones);
    const int cents = std::clamp(int(std::lround((semitones - whole) * 100)), 0, 99);
    return (std::uint32_t(whole) << 25) | (std::uint32_t(cents) << 18);
}

constexpr std::uint8_t kPitch7_9Attribute = std::uint8_t(uw::ump::NoteAttribute::Pitch7_9);
constexpr std::uint8_t kPitch7_25Controller = 3;   // registered per-note controller
constexpr std::uint8_t kResetFlag = 0x01;          // Per-Note Management "S"

} // namespace

UmpOutput::UmpOutput(const Tuning& tuning, const OutputConfig& config)
    : tuning_(tuning), config_(config) {}

std::uint32_t UmpOutput::word0(std::uint8_t status, std::uint8_t index1, std::uint8_t index2) const {
    return (0x4u << 28) | (std::uint32_t(config_.group & 0xF) << 24) |
           (std::uint32_t(status & 0xF) << 20) | (std::uint32_t(config_.channel & 0xF) << 16) |
           (std::uint32_t(index1) << 8) | index2;
}

void UmpOutput::emit2(std::uint32_t w0, std::uint32_t w1) {
    uw::MidiMessage m;
    m.words[0] = w0;
    m.words[1] = w1;
    m.wordCount = 2;
    if (midi2()) {
        out_.push_back(m);
        return;
    }
    uw::MidiMessage converted[uw::ump::kMaxTranslatedMessages];
    const auto n = uw::ump::downconvert(m, converted, uw::ump::kMaxTranslatedMessages);
    out_.insert(out_.end(), converted, converted + n);
}

void UmpOutput::registeredController(std::uint8_t bank, std::uint8_t index, std::uint32_t value) {
    emit2(word0(st(Status::RegisteredController), bank, index), value);
}

void UmpOutput::setup() {
    registeredController(0, 0, bendRangeValue(config_.bendRange));
    if (midi2()) registeredController(0, 7, bendRangeValue(config_.perNoteBendRange));
}

void UmpOutput::noteOn(int note, std::uint16_t velocity) {
    velocity = std::max<std::uint16_t>(velocity, 1);   // MIDI 2.0: velocity 0 isn't a note-off
    if (!midi2()) {
        const auto nn = tuning_.nearestMidi1Note(note);
        sounding_[note] = {nn, velocity};
        emit2(word0(st(Status::NoteOn), nn, 0), std::uint32_t(velocity) << 16);
        return;
    }
    const auto nn = tuning_.noteNumber(note);
    sounding_[note] = {nn, velocity};
    if (config_.perNoteManagement)
        emit2(word0(st(Status::PerNoteManagement), nn, kResetFlag), 0);
    // At 12-EDO the note number is the pitch, so there's no attribute to send.
    if (tuning_.edo == 12) {
        emit2(word0(st(Status::NoteOn), nn, 0), std::uint32_t(velocity) << 16);
    } else {
        emit2(word0(st(Status::NoteOn), nn, kPitch7_9Attribute),
              (std::uint32_t(velocity) << 16) | encodePitch7_9(tuning_.semitones(note)));
    }
}

void UmpOutput::noteOff(int note) {
    const auto it = sounding_.find(note);
    if (it == sounding_.end()) return;
    emit2(word0(st(Status::NoteOff), it->second.number, 0), 0);
    sounding_.erase(it);
}

void UmpOutput::glide(int note, int step) {
    const auto it = sounding_.find(note);
    if (it == sounding_.end()) return;
    if (midi2()) {
        emit2(word0(st(Status::RegisteredPerNoteController), it->second.number, kPitch7_25Controller),
              encodePitch7_25(tuning_.semitones(step)));
        return;
    }
    // MIDI 1.0 has no way to move one note's pitch: strike the new one, as
    // KeyEDO's Gliss mode, but keep the note's name.
    const auto nn = tuning_.nearestMidi1Note(step);
    if (nn == it->second.number) return;
    emit2(word0(st(Status::NoteOff), it->second.number, 0), 0);
    emit2(word0(st(Status::NoteOn), nn, 0), std::uint32_t(it->second.velocity) << 16);
    it->second.number = nn;
}

void UmpOutput::pitchBend(std::optional<int> note, double value) {
    if (note && midi2()) {
        const auto it = sounding_.find(*note);
        if (it == sounding_.end()) return;
        emit2(word0(st(Status::PerNotePitchBend), it->second.number, 0), bipolar32(value));
        return;
    }
    emit2(word0(st(Status::PitchBend), 0, 0), bipolar32(value));
}

void UmpOutput::controller(std::optional<int> note, int number, double value) {
    if (note && midi2()) {
        const auto it = sounding_.find(*note);
        if (it == sounding_.end()) return;
        emit2(word0(st(Status::RegisteredPerNoteController), it->second.number,
                    std::uint8_t(std::clamp(number, 0, 255))),
              unipolar32(value));
        return;
    }
    emit2(word0(st(Status::ControlChange), std::uint8_t(std::clamp(number, 0, 127)), 0),
          unipolar32(value));
}

void UmpOutput::pressure(std::optional<int> note, double value) {
    if (note) {
        // Polyphonic pressure exists in both protocols.
        const auto it = sounding_.find(*note);
        if (it == sounding_.end()) return;
        emit2(word0(st(Status::PolyPressure), it->second.number, 0), unipolar32(value));
        return;
    }
    emit2(word0(st(Status::ChannelPressure), 0, 0), unipolar32(value));
}

void UmpOutput::allNotesOff() {
    while (!sounding_.empty()) noteOff(sounding_.begin()->first);
    emit2(word0(st(Status::ControlChange), 123, 0), 0);
}

std::vector<uw::MidiMessage> UmpOutput::take() {
    std::vector<uw::MidiMessage> taken;
    taken.swap(out_);
    return taken;
}

} // namespace kedo
