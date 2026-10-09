#include "kedo/UmpText.h"

#include <cstdarg>
#include <cstdio>

#include "uw/ump/Ump.h"

namespace kedo {

namespace {

namespace ump = uw::ump;

std::string format(const char* fmt, ...) {
    char buf[160];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    return buf;
}

double percent(std::uint32_t value, double full) { return 100.0 * value / full; }

// A bipolar 32-bit value as -100..+100%.
double bipolar(std::uint32_t value) {
    return (double(value) - 2147483648.0) / (value >= 0x80000000u ? 2147483647.0 : 2147483648.0) * 100.0;
}

std::string hexWords(const uw::MidiMessage& m) {
    std::string s;
    for (int i = 0; i < m.wordCount; ++i) s += format("%s%08X", i ? " " : "", m.words[i]);
    return s;
}

std::string midi2(const uw::MidiMessage& m) {
    const std::uint32_t w0 = m.words[0], w1 = m.words[1];
    const unsigned i1 = (w0 >> 8) & 0xFF, i2 = w0 & 0xFF;
    switch (ump::status(m)) {
    case ump::Status::NoteOn:
    case ump::Status::NoteOff: {
        std::string s = format("%-12s n%-3u v%5.1f%%", ump::isNoteOn(m) ? "Note On" : "Note Off", i1,
                               percent(w1 >> 16, 65535.0));
        if (i2 == std::uint8_t(ump::NoteAttribute::Pitch7_9))
            s += format("  pitch %.3f", (w1 & 0xFFFF) / 512.0);
        else if (i2 != 0)
            s += format("  attr %u:%04X", i2, w1 & 0xFFFF);
        return s;
    }
    case ump::Status::PerNotePitchBend:
        return format("%-12s n%-3u %+6.1f%%", "Note Bend", i1, bipolar(w1));
    case ump::Status::RegisteredPerNoteController:
        if (i2 == 3) return format("%-12s n%-3u pitch %.4f", "Note Pitch", i1, w1 / double(1u << 25));
        return format("%-12s n%-3u #%-3u %5.1f%%", "Note RC", i1, i2, percent(w1, 4294967295.0));
    case ump::Status::AssignablePerNoteController:
        return format("%-12s n%-3u #%-3u %5.1f%%", "Note AC", i1, i2, percent(w1, 4294967295.0));
    case ump::Status::PerNoteManagement:
        return format("%-12s n%-3u%s%s", "Note Mgmt", i1, (i2 & 2) ? " detach" : "", (i2 & 1) ? " reset" : "");
    case ump::Status::PolyPressure:
        return format("%-12s n%-3u %5.1f%%", "Poly Press", i1, percent(w1, 4294967295.0));
    case ump::Status::ControlChange:
        return format("%-12s #%-3u %5.1f%%", "CC", i1, percent(w1, 4294967295.0));
    case ump::Status::RegisteredController:
        return format("%-12s %u/%u  %08X", "RPN", i1, i2, w1);
    case ump::Status::AssignableController:
        return format("%-12s %u/%u  %08X", "NRPN", i1, i2, w1);
    case ump::Status::ChannelPressure:
        return format("%-12s %5.1f%%", "Ch Press", percent(w1, 4294967295.0));
    case ump::Status::PitchBend:
        return format("%-12s %+6.1f%%", "Pitch Bend", bipolar(w1));
    case ump::Status::ProgramChange:
        return format("%-12s %u", "Program", (w1 >> 24) & 0x7F);
    default:
        return hexWords(m);
    }
}

std::string midi1(const uw::MidiMessage& m) {
    const std::uint32_t w0 = m.words[0];
    const unsigned d1 = (w0 >> 8) & 0x7F, d2 = w0 & 0x7F;
    switch (ump::status(m)) {
    case ump::Status::NoteOn: return format("%-12s n%-3u v%u", "Note On", d1, d2);
    case ump::Status::NoteOff: return format("%-12s n%-3u v%u", "Note Off", d1, d2);
    case ump::Status::PolyPressure: return format("%-12s n%-3u %u", "Poly Press", d1, d2);
    case ump::Status::ControlChange: return format("%-12s #%-3u %u", "CC", d1, d2);
    case ump::Status::ProgramChange: return format("%-12s %u", "Program", d1);
    case ump::Status::ChannelPressure: return format("%-12s %u", "Ch Press", d1);
    case ump::Status::PitchBend: return format("%-12s %d", "Pitch Bend", int((d2 << 7) | d1) - 8192);
    default: return hexWords(m);
    }
}

} // namespace

std::string describeUmp(const uw::MidiMessage& m) {
    const auto type = ump::messageType(m);
    const unsigned g = ump::group(m) + 1;
    if (type == ump::MessageType::Midi2ChannelVoice)
        return format("2.0 g%-2u ch%-2u ", g, ump::channel(m) + 1u) + midi2(m);
    if (type == ump::MessageType::Midi1ChannelVoice)
        return format("1.0 g%-2u ch%-2u ", g, ump::channel(m) + 1u) + midi1(m);
    return "    " + hexWords(m);
}

} // namespace kedo
