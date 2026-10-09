#pragma once

#include <map>
#include <vector>

#include "kedo/NoteSink.h"
#include "kedo/Tuning.h"
#include "uw/ump/MidiMessage.h"

// Turns NoteSink calls into UMP for one group and channel. MIDI 2.0 is the
// primary form; a MIDI 1.0 destination gets MT 0x2 messages made from it.
namespace kedo {

struct OutputConfig {
    std::uint8_t group   = 0;    // 0..15
    std::uint8_t channel = 0;    // 0..15

    // The destination's protocol. Midi2: everything at full resolution, notes
    // with Pitch 7.9. Midi1: notes at the nearest 12-EDO note, 7-bit values,
    // and per-note expression moves to the channel.
    uw::MidiProtocol protocol = uw::MidiProtocol::Midi2;

    // Pitch bend range in semitones, sent with setup(): registered controller
    // 0 (channel) and, for MIDI 2.0, 7 (per-note).
    double bendRange        = 2.0;
    double perNoteBendRange = 2.0;

    // MIDI 2.0: Per-Note Management with Reset before each note-on, so a
    // retriggered note starts with no per-note bend or controllers left over.
    bool perNoteManagement = true;

    bool operator==(const OutputConfig&) const = default;
};

class UmpOutput final : public NoteSink {
public:
    UmpOutput(const Tuning& tuning, const OutputConfig& config);

    void setTuning(const Tuning& tuning) { tuning_ = tuning; }
    void setConfig(const OutputConfig& config) { config_ = config; }
    const OutputConfig& config() const { return config_; }

    // Messages a receiver needs before playing: the bend ranges.
    void setup();

    // Note-off for every sounding note, then All Notes Off (CC 123), as
    // KeyEDO.
    void allNotesOff();

    // NoteSink
    void noteOn(int note, std::uint16_t velocity) override;
    void noteOff(int note) override;
    void glide(int note, int step) override;
    void pitchBend(std::optional<int> note, double value) override;
    void controller(std::optional<int> note, int number, double value) override;
    void pressure(std::optional<int> note, double value) override;

    // The UMP made since the last call. The app sends them, in order.
    std::vector<uw::MidiMessage> take();

private:
    bool midi2() const { return config_.protocol == uw::MidiProtocol::Midi2; }
    std::uint32_t word0(std::uint8_t status, std::uint8_t index1, std::uint8_t index2) const;
    void emit2(std::uint32_t w0, std::uint32_t w1);   // as MIDI 2.0, or downconverted
    void registeredController(std::uint8_t bank, std::uint8_t index, std::uint32_t value);

    Tuning tuning_;
    OutputConfig config_;
    struct Sounding {
        std::uint8_t  number;     // the note number sent
        std::uint16_t velocity;   // for a MIDI 1.0 glide's restrike
    };
    std::map<int, Sounding> sounding_;   // by note (the step it was struck on)
    std::vector<uw::MidiMessage> out_;
};

} // namespace kedo
