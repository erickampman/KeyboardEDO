#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <vector>

#include "kedo/NoteSink.h"

// KeyEDO's arpeggiator (ArpeggiatorEngine.swift) as a NoteSink between the
// Performer and the output (decisions.md, "Arpeggiator"). Off, it passes every
// call straight through. On, the notes played are a pool it steps through in
// time, sending one note at a time to the sink after it.
//
// It has no timer of its own: the caller says what time it is (the clock) and
// calls advance() at nextEvent(), so the app can run it on a thread and the
// tests can step time by hand.
namespace kedo {

enum class ArpPattern { Up, Down, UpDown, DownUp, AsPlayed, Random };

enum class ArpDivision {
    Whole, Half, Quarter, Eighth, Sixteenth, ThirtySecond,
    HalfTriplet, QuarterTriplet, EighthTriplet, SixteenthTriplet,
};

enum class ArpVelocity { AsPlayed, Fixed, Random };

inline constexpr int kArpPatterns = 6;
inline constexpr int kArpDivisions = 10;
inline constexpr int kArpVelocities = 3;

// KeyEDO's names, as its documents store them ("Up-Down", "1/8T", "As Played").
const char* arpPatternName(ArpPattern p);
const char* arpDivisionName(ArpDivision d);
const char* arpVelocityName(ArpVelocity v);

// A step's length in quarter notes.
double quarterNotes(ArpDivision d);

struct ArpConfig {
    bool        enabled  = false;
    ArpPattern  pattern  = ArpPattern::Up;
    ArpDivision division = ArpDivision::Sixteenth;
    double      bpm      = 120;     // 1..999
    double      gate     = 0.5;     // how much of a step the note sounds, 0.01..1
    int         octaves  = 1;       // the pattern, then again an octave up each: 1..4
    ArpVelocity velocity = ArpVelocity::AsPlayed;
    int         fixedVelocity = 64; // 7-bit, for ArpVelocity::Fixed (KeyEDO's 0x7F00)
    bool        hold     = false;   // released notes stay in the pool

    // Seconds between steps.
    double stepLength() const;

    bool operator==(const ArpConfig&) const = default;
};

class Arpeggiator final : public NoteSink {
public:
    // Seconds on a steady clock.
    using Clock = std::function<double()>;
    // The note `octaves` octaves (1200 cents each) above `note`: the layout's
    // key for that pitch, so EDO note numbers stay right. The default adds
    // 12 to the pitch and the number.
    using OctaveFinder = std::function<Note(const Note& note, int octaves)>;

    Arpeggiator(NoteSink& out, Clock clock);

    // Turning it on or off empties the pool and silences its note; the
    // caller releases what's held first. Other changes apply from the next
    // step, the tempo keeping its phase.
    void setConfig(const ArpConfig& config);
    const ArpConfig& config() const { return config_; }

    void setOctaveFinder(OctaveFinder finder) { octave_ = std::move(finder); }
    // The default: 12 added to the pitch and the number, and an id of its own.
    static Note octaveAbove(const Note& note, int octaves);
    void seed(unsigned s) { random_.seed(s); }   // Random pattern and velocity

    // Sends what's due by now: note-offs at the gate, the next step's note.
    void advance();
    // When advance() next has something to do; std::nullopt while idle.
    std::optional<double> nextEvent() const;
    bool running() const { return running_ || active_.has_value(); }

    // Silences its note and empties the pool, Hold or not (All Notes Off,
    // Escape, a new layout).
    void stop();

    // For drawing, by Note::id: in the pool (played or held), held only by
    // Hold, and the note the step is sounding now.
    bool inPool(int id) const;
    bool heldByHold(int id) const;
    bool sounding(int id) const { return active_ && active_->out.id == id; }
    // ... and that note is an octave copy (Octaves above 1), not a pool note.
    bool soundingAbove(int id) const { return sounding(id) && active_->octave > 0; }

    // NoteSink
    void noteOn(const Note& note, std::uint16_t velocity) override;
    void noteOff(int id) override;
    void glide(int id, double pitch) override;
    void pitchBend(std::optional<int> id, double value) override;
    void controller(std::optional<int> id, int number, double value) override;
    void pressure(std::optional<int> id, double value) override;

private:
    // A note in the pool, and the per-note expression played on it, sent
    // again after each of its steps' note-ons (Per-Note Management resets it).
    struct Slot {
        Note          note;
        std::uint16_t velocity = 0;
        bool          pressed = true;   // false: Hold keeps it
        std::optional<double> bend, pressure;
        std::map<int, double> controllers;
    };
    struct Step {
        int id;        // the slot's Note::id
        int octave;    // above it
    };
    struct Active {
        int  from;     // the slot's Note::id
        int  octave;
        Note out;      // what was sent
    };

    bool on() const { return config_.enabled; }
    Slot* slot(int id);
    void rebuild();
    void fire(double at);
    void silence();
    void idle();
    std::uint16_t velocityFor(const Slot& s);
    void reapply(const Slot& s, int id);

    NoteSink& out_;
    Clock clock_;
    OctaveFinder octave_;
    ArpConfig config_;
    std::mt19937 random_;

    std::vector<Slot> pool_;        // in the order played
    std::vector<Step> sequence_;    // the pattern, octaves included
    std::size_t index_ = 0;         // the next step
    std::optional<Step> played_;    // the last one
    bool running_ = false;
    double last_ = 0;               // when the last step began
    double next_ = 0;               // when the next one begins
    std::optional<Active> active_;  // the note the arpeggiator is sounding
    double offAt_ = 0;              // when its gate ends
};

} // namespace kedo
