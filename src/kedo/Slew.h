#pragma once

#include <functional>
#include <map>
#include <optional>

#include "kedo/NoteSink.h"

// Glide slew: a NoteSink between the Performer and the arpeggiator that slides
// a glide from the note's pitch to the key reached at a steady rate, given as
// the seconds an octave takes, instead of jumping there (decisions.md, "Glide
// slew"). It sends the pitch every kInterval while a note is on its way; a
// glide that comes before it gets there starts from where it is. Time 0
// passes glides straight through. Everything else passes as it came.
//
// Like the arpeggiator it has no timer: the caller says what time it is and
// calls advance() at nextEvent().
namespace kedo {

class Slew final : public NoteSink {
public:
    using Clock = std::function<double()>;   // seconds on a steady clock

    static constexpr double kInterval = 0.005;   // seconds between pitches sent while sliding
    static constexpr double kMaxTime  = 10;      // seconds an octave takes, the slowest

    Slew(NoteSink& out, Clock clock);

    // Seconds an octave takes, 0..kMaxTime: the larger, the slower. Notes on
    // their way when it changes go on at the new rate (0, Off: straight to
    // where they're going).
    void setTime(double secondsPerOctave);
    double time() const { return time_; }

    // Sends each sliding note's pitch now, and ends the slides that got there.
    void advance();
    // When advance() next has something to do; std::nullopt while no note slides.
    std::optional<double> nextEvent() const;
    bool running() const { return !sliding_.empty(); }

    // NoteSink
    void noteOn(const Note& note, std::uint16_t velocity) override;
    void noteOff(int id) override;
    void letGo(int id) override;
    void glide(int id, double pitch) override;
    void pitchBend(std::optional<int> id, double value) override { out_.pitchBend(id, value); }
    void controller(std::optional<int> id, int number, double value) override {
        out_.controller(id, number, value);
    }
    void pressure(std::optional<int> id, double value) override { out_.pressure(id, value); }

private:
    struct Slide {
        double target;   // semitones
        double at;       // when pitch_ was last brought up to date
    };

    // Brings `id`'s pitch up to `now`; true once it's at the target.
    bool move(int id, Slide& s, double now);
    void forget(int id);

    NoteSink& out_;
    Clock clock_;
    double time_ = 0;   // seconds an octave takes; 0: Off
    double rate_ = 0;   // semitones a second, 12 / time_ (0 while Off)
    std::map<int, double> pitch_;    // each sounding note's pitch, as last sent (semitones)
    std::map<int, Slide> sliding_;   // the notes on their way
};

} // namespace kedo
