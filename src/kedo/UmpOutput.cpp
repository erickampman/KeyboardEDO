#include "kedo/UmpOutput.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include "kedo/Tuning.h"
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

UmpOutput::UmpOutput(const OutputConfig& config) : config_(config) {}

void UmpOutput::emit2(std::uint8_t group, std::uint8_t channel, std::uint8_t status, std::uint8_t index1,
                      std::uint8_t index2, std::uint32_t data, bool asMidi2) {
    uw::MidiMessage m;
    m.words[0] = (0x4u << 28) | (std::uint32_t(group & 0xF) << 24) | (std::uint32_t(status & 0xF) << 20) |
                 (std::uint32_t(channel & 0xF) << 16) | (std::uint32_t(index1) << 8) | index2;
    m.words[1] = data;
    m.wordCount = 2;
    if (asMidi2) {
        out_.push_back(m);
        return;
    }
    uw::MidiMessage converted[uw::ump::kMaxTranslatedMessages];
    const auto n = uw::ump::downconvert(m, converted, uw::ump::kMaxTranslatedMessages);
    out_.insert(out_.end(), converted, converted + n);
}

// Channel-wide messages are all expression or setup.
void UmpOutput::emitChannel(std::uint8_t status, std::uint8_t index1, std::uint8_t index2, std::uint32_t data) {
    emit2(config_.group, config_.channel, status, index1, index2, data, midi2Expression());
}

const UmpOutput::Sounding* UmpOutput::sounding(int id) const {
    const auto it = sounding_.find(id);
    return it == sounding_.end() ? nullptr : &it->second;
}

void UmpOutput::registeredController(std::uint8_t bank, std::uint8_t index, std::uint32_t value) {
    emitChannel(st(Status::RegisteredController), bank, index, value);
}

void UmpOutput::setup() {
    registeredController(0, 0, bendRangeValue(config_.bendRange));
    if (midi2Expression()) registeredController(0, 7, bendRangeValue(config_.perNoteBendRange));
}

void UmpOutput::noteOn(const Note& note, std::uint16_t velocity) {
    velocity = std::max<std::uint16_t>(velocity, 1);   // MIDI 2.0: velocity 0 isn't a note-off
    Sounding s;
    s.group = note.group.value_or(config_.group);
    s.channel = note.channel.value_or(config_.channel);
    s.number = midi2() ? note.number : note.nearestMidi1Note();
    s.velocity = velocity;
    sounding_[note.id] = s;

    if (!midi2()) {
        emit2(s.group, s.channel, st(Status::NoteOn), s.number, 0, std::uint32_t(velocity) << 16);
        return;
    }
    if (config_.perNoteManagement)
        emit2(s.group, s.channel, st(Status::PerNoteManagement), s.number, kResetFlag, 0);
    // A note whose number is its pitch (12-EDO) needs no attribute.
    if (note.needsPitchAttribute())
        emit2(s.group, s.channel, st(Status::NoteOn), s.number, kPitch7_9Attribute,
              (std::uint32_t(velocity) << 16) | encodePitch7_9(note.pitch));
    else
        emit2(s.group, s.channel, st(Status::NoteOn), s.number, 0, std::uint32_t(velocity) << 16);
}

void UmpOutput::noteOff(int id) {
    const Sounding* s = sounding(id);
    if (!s) return;
    emit2(s->group, s->channel, st(Status::NoteOff), s->number, 0, 0);
    sounding_.erase(id);
}

void UmpOutput::glide(int id, double pitch) {
    const auto it = sounding_.find(id);
    if (it == sounding_.end()) return;
    Sounding& s = it->second;
    if (midi2()) {
        emit2(s.group, s.channel, st(Status::RegisteredPerNoteController), s.number, kPitch7_25Controller,
              encodePitch7_25(pitch));
        return;
    }
    // MIDI 1.0 has no way to move one note's pitch: strike the new one, as
    // KeyEDO's Gliss mode, but keep the note's id.
    const auto nn = std::uint8_t(std::clamp(std::lround(pitch), 0L, 127L));
    if (nn == s.number) return;
    emit2(s.group, s.channel, st(Status::NoteOff), s.number, 0, 0);
    emit2(s.group, s.channel, st(Status::NoteOn), nn, 0, std::uint32_t(s.velocity) << 16);
    s.number = nn;
}

void UmpOutput::pitchBend(std::optional<int> id, double value) {
    if (id && midi2Expression()) {
        if (const Sounding* s = sounding(*id))
            emit2(s->group, s->channel, st(Status::PerNotePitchBend), s->number, 0, bipolar32(value));
        return;
    }
    emitChannel(st(Status::PitchBend), 0, 0, bipolar32(value));
}

void UmpOutput::controller(std::optional<int> id, int number, double value) {
    if (id && midi2Expression()) {
        if (const Sounding* s = sounding(*id))
            emit2(s->group, s->channel, st(Status::RegisteredPerNoteController), s->number,
                  std::uint8_t(std::clamp(number, 0, 255)), unipolar32(value));
        return;
    }
    emitChannel(st(Status::ControlChange), std::uint8_t(std::clamp(number, 0, 127)), 0, unipolar32(value));
}

void UmpOutput::pressure(std::optional<int> id, double value) {
    if (id) {
        // Polyphonic pressure exists in both protocols.
        if (const Sounding* s = sounding(*id))
            emit2(s->group, s->channel, st(Status::PolyPressure), s->number, 0, unipolar32(value),
                  midi2Expression());
        return;
    }
    emitChannel(st(Status::ChannelPressure), 0, 0, unipolar32(value));
}

void UmpOutput::allNotesOff() {
    std::set<std::pair<std::uint8_t, std::uint8_t>> channels{{config_.group, config_.channel}};
    for (const auto& [id, s] : sounding_) channels.insert({s.group, s.channel});
    while (!sounding_.empty()) noteOff(sounding_.begin()->first);
    for (const auto& [group, channel] : channels)
        emit2(group, channel, st(Status::ControlChange), 123, 0, 0, midi2Expression());   // a controller
}

std::vector<uw::MidiMessage> UmpOutput::take() {
    std::vector<uw::MidiMessage> taken;
    taken.swap(out_);
    return taken;
}

} // namespace kedo
