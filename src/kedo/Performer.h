#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>

#include "kedo/Layout.h"
#include "kedo/NoteSink.h"

// The playing logic of KeyEDO's three keyboard views, in one place and without
// a toolkit: pointers (each finger or the mouse, by id) and computer keys go
// in, NoteSink calls come out. Every pointer is tracked on its own, so
// multitouch works the same on every layout.
namespace kedo {

// What dragging a pressed key does (KeyEDO's DragBehavior).
enum class DragBehavior {
    Gliss,   // the note follows the pointer from key to key
    Fixed,   // the note stays where it was struck; dragging sends expression
};

enum class HorizontalControl { Off, PitchBend };
enum class VerticalControl { Off, Controller, Pressure, PitchBend };

struct PlayConfig {
    DragBehavior drag = DragBehavior::Gliss;

    // Gliss: move the struck note's pitch with Pitch 7.25 instead of striking
    // each key passed over (MIDI 2.0; UmpOutput restrikes for MIDI 1.0).
    bool glide = false;

    // Fixed: what dragging sends. A full deflection is `expressionDistance`
    // layout units from where the pointer went down.
    HorizontalControl horizontal = HorizontalControl::Off;
    VerticalControl   vertical   = VerticalControl::Off;
    int               verticalController = 1;      // CC, or per-note controller
    double            expressionDistance = 150;

    // Each pointer's expression goes to its own note (MIDI 2.0 per-note
    // messages). Off: the first pointer down drives the whole channel, as KeyEDO.
    bool perNoteExpression = true;

    // Velocity: fixed, or from where in the key it was struck (top loudest).
    std::uint16_t velocity = 0xFFFF;
    bool          strikeVelocity = false;
    int           strikeVelocityMin = 32;   // 7-bit, as KeyEDO's setting

    bool operator==(const PlayConfig&) const = default;
};

struct Modifiers {
    bool latch = false;    // Shift: a released note keeps sounding
};

class Performer {
public:
    explicit Performer(NoteSink& sink);

    // The layout keys are found in. Releases everything if it changes.
    void setLayout(const Layout* layout);
    void setConfig(const PlayConfig& config) { config_ = config; }
    const PlayConfig& config() const { return config_; }

    // The toolbar Latch toggle: every release latches, as if Shift were held.
    // Turning it off releases the latched notes, sustain-pedal style.
    void setLatchMode(bool on);
    bool latchMode() const { return latchMode_; }

    // Pointers, in layout coordinates. `id` is any value unique among the
    // pointers down at once (a touch id, or 0 for the mouse).
    void pointerDown(int id, Point p, Modifiers mods = {});
    void pointerMove(int id, Point p);
    void pointerUp(int id, Modifiers mods = {});
    void pointerCancel(int id);        // released without latching

    // Computer keys, already mapped to a step (QwertyMap). `key` identifies
    // the physical key.
    void keyDown(int key, int step, Modifiers mods = {});
    void keyUp(int key, Modifiers mods = {});

    void releaseAllLatched();
    void releaseAll();

    // For drawing: whether `step` is sounding (under a pointer or key, or
    // latched), and whether it's latched.
    bool isSounding(int step) const;
    bool isLatched(int step) const;

private:
    struct Touch {
        Point start;
        std::optional<int> note;   // the step it struck; none after unlatching a key
        int  step  = 0;            // the step it sounds now (differs after a glide)
        long keyId = -1;           // the key it is over, for Gliss
    };

    void press(Touch& t, const Key& key, Point p);
    void strike(Touch& t, int step, std::uint16_t velocity);
    void finish(Touch& t, bool latch);
    void expression(int id, Touch& t, Point p);
    void resetExpression(int id, const Touch& t);
    std::uint16_t velocityFor(const Key& key, Point p) const;

    void acquire(int note, std::uint16_t velocity);
    void release(int note);

    NoteSink& sink_;
    const Layout* layout_ = nullptr;
    PlayConfig config_;
    bool latchMode_ = false;

    std::map<int, Touch> touches_;       // by pointer id
    std::map<int, Touch> keys_;          // by computer key
    std::map<int, int> latched_;         // note -> the step it sounds
    std::map<int, int> holds_;           // note -> touches, keys and latches holding it
    std::optional<int> expressionOwner_; // channel-wide expression: the pointer driving it
};

} // namespace kedo
