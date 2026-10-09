#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "kedo/NoteSink.h"
#include "kedo/UmpOutput.h"
#include "uw/midi/MidiOutput.h"

// Where the keyboard's notes go: a chosen destination, our own published
// source, or both (KeyEDO's routing menu). Each has its own UmpOutput, made
// for its protocol, and one NoteSink (sink()) feeds them all. With neither,
// a preview output still runs, so the monitor shows what would be sent.
//
// It knows uw::MidiOutput but not SDL or ImGui.
namespace kedo::app {

class MidiTargets {
public:
    // The name other apps see our published source by.
    static constexpr const char* kPublishedName = "KeyboardEDO";

    explicit MidiTargets(const OutputConfig& config);
    ~MidiTargets();

    // False where uw-core has no MIDI output yet (Linux, until AlsaSeqOutput).
    bool available() const { return midi_ != nullptr; }

    // The destinations to offer, refreshed when the system says they changed.
    const std::vector<uw::MidiDestinationInfo>& destinations();

    // Choose a destination by id or name ("" for none; KeyEDO documents
    // name it). Ends the old one's notes. False if it isn't there now; it's
    // still remembered, and opened if it appears.
    bool setDestination(const std::string& idOrName);
    const std::string& destination() const { return wanted_; }
    bool destinationOpen() const { return destHandle_ != uw::kInvalidMidiDestination; }
    uw::MidiProtocol destinationProtocol() const { return dest_.config().protocol; }

    bool setPublish(bool on);
    bool publishing() const { return pubHandle_ != uw::kInvalidMidiDestination; }

    // Group, channel and the rest; the protocol is each target's own.
    void setConfig(const OutputConfig& config);

    NoteSink& sink() { return tee_; }
    void allNotesOff();

    // Sends what the outputs made since the last call, and adds each message
    // to `monitor` as text, tagged with where it went.
    void flush(std::deque<std::string>& monitor, std::size_t monitorLines);

    // Hotplug: reopens the destination when it comes back. Call once a frame.
    void poll();

private:
    // Forwards every call to the outputs in use.
    struct Tee final : NoteSink {
        MidiTargets& t;
        explicit Tee(MidiTargets& targets) : t(targets) {}
        template <typename F> void each(F f);
        void noteOn(const Note& n, std::uint16_t v) override { each([&](UmpOutput& o) { o.noteOn(n, v); }); }
        void noteOff(int id) override { each([&](UmpOutput& o) { o.noteOff(id); }); }
        void glide(int id, double p) override { each([&](UmpOutput& o) { o.glide(id, p); }); }
        void pitchBend(std::optional<int> id, double v) override { each([&](UmpOutput& o) { o.pitchBend(id, v); }); }
        void controller(std::optional<int> id, int c, double v) override {
            each([&](UmpOutput& o) { o.controller(id, c, v); });
        }
        void pressure(std::optional<int> id, double v) override { each([&](UmpOutput& o) { o.pressure(id, v); }); }
    };

    void openWanted();
    void closeDestination();
    OutputConfig withProtocol(uw::MidiProtocol p) const;

    std::unique_ptr<uw::MidiOutput> midi_;
    OutputConfig config_;
    UmpOutput dest_, pub_, preview_;
    Tee tee_{*this};

    std::string wanted_;   // the destination asked for: id or name
    uw::MidiDestinationHandle destHandle_ = uw::kInvalidMidiDestination;
    uw::MidiDestinationHandle pubHandle_ = uw::kInvalidMidiDestination;

    std::vector<uw::MidiDestinationInfo> list_;
    std::uint64_t listedAt_ = ~std::uint64_t(0);
};

} // namespace kedo::app
