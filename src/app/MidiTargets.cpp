#include "MidiTargets.h"

#include "kedo/UmpText.h"
#include "uw/midi/PlatformMidiOutput.h"

namespace kedo::app {

MidiTargets::MidiTargets(const OutputConfig& config)
    : config_(config), dest_(config), pub_(withProtocol(uw::MidiProtocol::Midi2)), preview_(config) {
#if UW_HAVE_MIDI_OUTPUT
    midi_ = uw::PlatformMidiOutput::create("KeyboardEDO");
#endif
    // No SysEx manufacturer ID of our own: device identity zero but for the
    // version (0.1.0 as 0, 1, 0, 0). Sends MIDI 2.0 only, as published.
    identity_.name     = kPublishedName;
    identity_.revision = {0, 1, 0, 0};
    identity_.midi1    = false;
    identity_.midi2    = true;
    identity_.blocks   = {{"Keyboard", 0, 1, uw::ump::FunctionBlock::Direction::Output, true}};
}

MidiTargets::~MidiTargets() {
    // Nothing left sounding on a synth after we've gone.
    allNotesOff();
    std::deque<std::string> discard;
    flush(discard, 0);
    closeDestination();
    if (midi_ && publishing()) midi_->close(pubHandle_);
}

OutputConfig MidiTargets::withProtocol(uw::MidiProtocol p) const {
    OutputConfig c = config_;
    c.protocol = p;
    return c;
}

template <typename F>
void MidiTargets::Tee::each(F f) {
    if (t.destinationOpen()) f(t.dest_);
    if (t.publishing()) f(t.pub_);
    if (!t.destinationOpen() && !t.publishing()) f(t.preview_);
}

const std::vector<uw::MidiDestinationInfo>& MidiTargets::destinations() {
    if (midi_ && midi_->changes() != listedAt_) {
        listedAt_ = midi_->changes();
        list_ = midi_->enumerate();
    }
    return list_;
}

void MidiTargets::closeDestination() {
    if (midi_ && destinationOpen()) midi_->close(destHandle_);
    destHandle_ = uw::kInvalidMidiDestination;
}

void MidiTargets::openWanted() {
    if (!midi_ || wanted_.empty() || destinationOpen()) return;
    for (const auto& d : destinations()) {
        if (d.id != wanted_ && d.name != wanted_) continue;
        destHandle_ = midi_->open(d.id);
        if (!destinationOpen()) return;
        dest_.setConfig(withProtocol(midi_->protocol(destHandle_)));
        dest_.setup();
        return;
    }
}

bool MidiTargets::setDestination(const std::string& idOrName) {
    if (idOrName == wanted_ && (destinationOpen() || idOrName.empty())) return true;
    dest_.allNotesOff();
    std::deque<std::string> discard;
    flush(discard, 0);
    closeDestination();
    wanted_ = idOrName;
    openWanted();
    return wanted_.empty() || destinationOpen();
}

bool MidiTargets::setPublish(bool on) {
    if (!midi_ || on == publishing()) return on == publishing();
    if (!on) {
        pub_.allNotesOff();
        std::deque<std::string> discard;
        flush(discard, 0);
        midi_->close(pubHandle_);
        pubHandle_ = uw::kInvalidMidiDestination;
        return true;
    }
    // Always MIDI 2.0: CoreMIDI or the ALSA sequencer converts for MIDI 1.0
    // receivers. A UMP endpoint where the platform has them for apps.
    pubHandle_ = midi_->openEndpoint(identity_, uw::MidiProtocol::Midi2, kPublishedId);
    if (publishing()) {
        pub_.setup();
        answered_ = midi_->inquiries(pubHandle_);
    }
    return publishing();
}

void MidiTargets::setGroups(std::uint8_t first, std::uint8_t count) {
    auto& block = identity_.blocks.front();
    if (block.firstGroup == first && block.groupCount == count) return;
    block.firstGroup = first;
    block.groupCount = count;
    if (!midi_ || !publishing()) return;
    // A new endpoint, on CoreMIDI: nothing held carries over to it, and a
    // receiver that reconnects needs the setup.
    pub_.allNotesOff();
    std::deque<std::string> discard;
    flush(discard, 0);
    midi_->setFunctionBlocks(pubHandle_, identity_.blocks);
    pub_.setup();
}

void MidiTargets::setConfig(const OutputConfig& config) {
    allNotesOff();
    config_ = config;
    dest_.setConfig(withProtocol(destinationOpen() ? midi_->protocol(destHandle_) : config.protocol));
    pub_.setConfig(withProtocol(uw::MidiProtocol::Midi2));
    preview_.setConfig(config);
    // Only targets in use get messages; one opened later gets its setup then.
    tee_.each([](UmpOutput& o) { o.setup(); });
}

void MidiTargets::allNotesOff() {
    tee_.each([](UmpOutput& o) { o.allNotesOff(); });
}

void MidiTargets::flush(std::deque<std::string>& monitor, std::size_t monitorLines) {
    auto send = [&](UmpOutput& o, uw::MidiDestinationHandle handle, const char* tag) {
        const auto messages = o.take();
        if (messages.empty()) return;
        if (midi_ && handle != uw::kInvalidMidiDestination) midi_->send(handle, messages.data(), messages.size());
        if (monitorLines == 0) return;
        for (const auto& m : messages) {
            monitor.push_back(std::string(tag) + describeUmp(m));
            while (monitor.size() > monitorLines) monitor.pop_front();
        }
    };
    send(dest_, destHandle_, "dest  ");
    send(pub_, pubHandle_, "pub   ");
    send(preview_, uw::kInvalidMidiDestination, "      ");
}

void MidiTargets::poll() {
    if (midi_ && publishing() && midi_->inquiries(pubHandle_) != answered_) {
        answered_ = midi_->inquiries(pubHandle_);
        pub_.setup();
    }
    if (!midi_ || midi_->changes() == listedAt_) return;
    destinations();   // refresh the list
    if (destinationOpen() && !midi_->sync(destHandle_)) {
        // Gone: forget its notes, drop it, and wait for it to come back.
        dest_.allNotesOff();
        dest_.take();
        closeDestination();
    }
    openWanted();
}

} // namespace kedo::app
