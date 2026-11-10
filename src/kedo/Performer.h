#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

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

// Where an expression message goes: the finger's own note (MIDI 2.0 per-note
// pitch bend, a registered per-note controller, polyphonic pressure), or the
// whole channel (pitch bend, CC, channel pressure), driven by the first
// finger down, as KeyEDO.
enum class ExpressionScope { Note, Channel };

struct PlayConfig {
    DragBehavior drag = DragBehavior::Gliss;

    // Gliss: move the struck note's pitch with Pitch 7.25 instead of striking
    // each key passed over (MIDI 2.0; UmpOutput restrikes for MIDI 1.0).
    bool glide = false;

    // Fixed: what dragging sends; also what hover expression sends, in
    // either drag mode. A full deflection is `expressionDistance`
    // layout units from where the pointer went down.
    HorizontalControl horizontal = HorizontalControl::Off;
    VerticalControl   vertical   = VerticalControl::Off;
    int               verticalController = 1;      // CC, or per-note controller
    double            expressionDistance = 150;

    // The scope of each kind of message, chosen separately. Per-note pitch
    // bend and controllers are MIDI 2.0 only; polyphonic pressure is in both.
    ExpressionScope bendScope       = ExpressionScope::Note;
    ExpressionScope controllerScope = ExpressionScope::Note;
    ExpressionScope pressureScope   = ExpressionScope::Note;

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

    // Computer keys, already mapped to a note (the QWERTY mapping, which only
    // passes playable ones). `key` identifies the physical key.
    void keyDown(int key, const Note& note, Modifiers mods = {});
    void keyUp(int key, Modifiers mods = {});

    // Hover expression (decisions.md, "Computer keyboard"): while Ctrl is held,
    // the mouse drives the expression of the notes computer keys hold, latched
    // notes and hoverAlso's (each once; not a pointer's), as a
    // Fixed drag would, measured from where it was when Ctrl went down. Ending
    // it, or releasing a key, resets that expression.
    void hoverBegin(Point p);
    void hoverMove(Point p);
    void hoverEnd();
    bool hovering() const { return hoverStart_.has_value(); }
    // More notes hover expression drives, by Note::id, besides computer
    // keys' and latched ones: the app gives the arpeggiator's, while its
    // Hold keeps them. Notes a pointer holds are left to the pointer.
    void setHoverAlso(std::function<std::vector<int>()> also) { hoverAlso_ = std::move(also); }

    void releaseAllLatched();
    void releaseAll();

    // For drawing, by Note::id: whether a note is sounding (under a pointer or
    // key, or latched), and whether it's latched. After a glide, the note
    // glided to shows as sounding, not the one struck.
    bool isSounding(int id) const;
    bool isLatched(int id) const;
    bool hasLatched() const { return !latched_.empty(); }

    // The latched notes in the order latched, by the Note::id struck, with
    // their velocities: the
    // app's session saves them, and latch()es them again when it's restored.
    std::vector<std::pair<int, std::uint16_t>> latchedNotes() const;
    void latch(const Note& note, std::uint16_t velocity);

private:
    struct Touch {
        Point start;
        std::optional<Note> note;  // what it struck; none after unlatching a key
        int  shows = 0;            // the Note::id it sounds now (differs after a glide)
        long keyId = -1;           // the key it is over, for Gliss
    };

    void press(Touch& t, const Key& key, Point p);
    void strike(Touch& t, const Note& note, std::uint16_t velocity);
    void finish(Touch& t, bool latch);
    void expression(int id, Touch& t, Point p);
    // Sends expression, `right` and `up` in full deflections, where `to`
    // says each scope goes (false: nowhere).
    template <typename To> void send(double right, double up, To&& to);
    std::vector<int> hoverTargets(int exceptKey = -1) const;
    void hoverSend(double right, double up, const std::vector<int>& ids, bool channel = true);
    bool target(ExpressionScope scope, int id, const Touch& t, std::optional<int>& out) const;
    void resetExpression(int id, const Touch& t);
    std::uint16_t velocityFor(const Key& key, Point p) const;

    void acquire(const Note& note, std::uint16_t velocity);
    void release(int id, bool letGo = false);   // letGo: NoteSink::letGo

    NoteSink& sink_;
    const Layout* layout_ = nullptr;
    PlayConfig config_;
    bool latchMode_ = false;

    std::map<int, Touch> touches_;       // by pointer id
    std::map<int, Touch> keys_;          // by computer key
    std::map<int, int> latched_;         // Note::id -> the id it sounds as
    std::map<int, int> holds_;           // Note::id -> touches, keys and latches holding it
    std::vector<int> latchOrder_;        // latched_'s keys, in the order latched
    std::map<int, std::uint16_t> velocities_;   // Note::id -> what it was struck with, while held
    std::optional<int> expressionOwner_; // channel-scope expression: the pointer driving it
    std::optional<Point> hoverStart_;    // where the mouse was when hover expression began
    bool hoverChannelSet_ = false;       // hover left channel expression off centre
    std::function<std::vector<int>()> hoverAlso_;
};

} // namespace kedo
