#pragma once

#include <map>
#include <vector>

#include "kedo/NoteSink.h"
#include "uw/ump/MidiMessage.h"

// Turns NoteSink calls into UMP. MIDI 2.0 is the primary form; a MIDI 1.0
// destination gets MT 0x2 messages made from it. Everything it sends comes
// from the Notes it's given, so it doesn't know the tuning or the layout.
namespace kedo {

struct OutputConfig {
    // Where a note goes unless it names its own group or channel, and where
    // channel-wide messages go.
    std::uint8_t group   = 0;    // 0..15
    std::uint8_t channel = 0;    // 0..15

    // The destination's protocol. Midi2: everything at full resolution, notes
    // with Pitch 7.9 when their pitch isn't their number. Midi1: notes at the
    // nearest 12-EDO note, 7-bit values, and per-note expression moves to
    // the channel.
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
    explicit UmpOutput(const OutputConfig& config);

    void setConfig(const OutputConfig& config) { config_ = config; }
    const OutputConfig& config() const { return config_; }

    // Messages a receiver needs before playing: the bend ranges.
    void setup();

    // Note-off for every sounding note, then All Notes Off (CC 123) on each
    // group and channel used, as KeyEDO.
    void allNotesOff();

    // NoteSink
    void noteOn(const Note& note, std::uint16_t velocity) override;
    void noteOff(int id) override;
    void glide(int id, double pitch) override;
    void pitchBend(std::optional<int> id, double value) override;
    void controller(std::optional<int> id, int number, double value) override;
    void pressure(std::optional<int> id, double value) override;

    // The UMP made since the last call. The app sends them, in order.
    std::vector<uw::MidiMessage> take();

private:
    struct Sounding {
        std::uint8_t  group;
        std::uint8_t  channel;
        std::uint8_t  number;     // the note number sent
        std::uint16_t velocity;   // for a MIDI 1.0 glide's restrike
    };

    bool midi2() const { return config_.protocol == uw::MidiProtocol::Midi2; }
    void emit2(std::uint8_t group, std::uint8_t channel, std::uint8_t status, std::uint8_t index1,
               std::uint8_t index2, std::uint32_t data);   // as MIDI 2.0, or downconverted
    void emitChannel(std::uint8_t status, std::uint8_t index1, std::uint8_t index2, std::uint32_t data);
    void registeredController(std::uint8_t bank, std::uint8_t index, std::uint32_t value);
    const Sounding* sounding(int id) const;

    OutputConfig config_;
    std::map<int, Sounding> sounding_;   // by Note::id
    std::vector<uw::MidiMessage> out_;
};

} // namespace kedo
