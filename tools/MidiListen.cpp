// Prints the UMP a MIDI source sends, as the app's monitor does: a check
// that notes really leave KeyboardEDO.
//
//   kedo-midi-listen [SOURCE] [SECONDS]
//   kedo-midi-listen --as-destination NAME 1|2 [SECONDS]
//
// SOURCE is a source's id or name (default "KeyboardEDO", the app's published
// source); it waits for it to appear. With --as-destination it is a
// destination instead, named NAME, of MIDI 1.0 or 2.0, for the app's "Send
// to". SECONDS (default 10) is how long to listen. Silent: it only receives.

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "kedo/UmpText.h"
#include "uw/midi/PlatformMidiInput.h"

namespace {

struct Printer final : uw::MidiSink {
    std::mutex lock;
    std::vector<uw::MidiMessage> pending;
    void onMidi(uw::MidiSourceHandle, const uw::TimedMidiMessage& m) noexcept override {
        std::lock_guard<std::mutex> g(lock);
        pending.push_back(m.message);
    }
    void print() {
        std::vector<uw::MidiMessage> taken;
        {
            std::lock_guard<std::mutex> g(lock);
            taken.swap(pending);
        }
        for (const auto& m : taken) std::printf("%s\n", kedo::describeUmp(m).c_str());
        std::fflush(stdout);
    }
};

} // namespace

int main(int argc, char** argv) {
    const bool asDestination = argc > 1 && std::string(argv[1]) == "--as-destination";
    const int arg = asDestination ? 2 : 1;
    const std::string wanted = argc > arg ? argv[arg] : "KeyboardEDO";
    const bool midi1 = asDestination && argc > 3 && std::string(argv[3]) == "1";
    const int secondsArg = asDestination ? 4 : 2;
    const double seconds = argc > secondsArg ? std::atof(argv[secondsArg]) : 10.0;

    auto input = uw::PlatformMidiInput::create("kedo-midi-listen");
    if (!input) return 1;
    Printer printer;
    input->setSink(&printer);

    if (asDestination) {
        const auto protocol = midi1 ? uw::MidiProtocol::Midi1 : uw::MidiProtocol::Midi2;
        if (input->openVirtual(wanted, protocol) == uw::kInvalidMidiSource) return 1;
        std::printf("destination '%s' (%s)\n", wanted.c_str(), midi1 ? "MIDI 1.0" : "MIDI 2.0");
        std::fflush(stdout);
        const auto stop = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (std::chrono::steady_clock::now() < stop) {
            printer.print();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        printer.print();
        return 0;
    }

    const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
    uw::MidiSourceHandle handle = uw::kInvalidMidiSource;
    while (std::chrono::steady_clock::now() < end) {
        if (handle == uw::kInvalidMidiSource) {
            for (const auto& s : input->enumerate()) {
                if (s.id != wanted && s.name != wanted) continue;
                handle = input->open(s.id);
                if (handle != uw::kInvalidMidiSource)
                    std::printf("listening to '%s' (%s)\n", s.id.c_str(),
                                s.protocol == uw::MidiProtocol::Midi2 ? "MIDI 2.0" : "MIDI 1.0");
                std::fflush(stdout);
            }
        }
        printer.print();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    printer.print();
    if (handle == uw::kInvalidMidiSource) {
        std::fprintf(stderr, "kedo-midi-listen: no source '%s' appeared\n", wanted.c_str());
        return 1;
    }
    return 0;
}
