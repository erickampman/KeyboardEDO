#pragma once

#include <deque>
#include <map>
#include <memory>
#include <string>

#include <SDL3/SDL.h>

#include "kedo/KeyStyle.h"
#include "kedo/Layout.h"
#include "kedo/Performer.h"
#include "kedo/UmpOutput.h"
#include "kedo/Viewport.h"

struct ImDrawList;
struct ImFont;

// The app's one window: a settings bar, the keyboard, and a MIDI monitor.
// This file and its .cpp are the only code that knows SDL and ImGui; the
// keyboard itself is kedo::core.
namespace kedo::app {

class KeyboardApp {
public:
    // How to start: from the command line (main.cpp).
    struct Start {
        Tuning     tuning;
        LayoutKind layout = LayoutKind::Piano;
    };

    // `monoFont`: the MIDI monitor's font.
    KeyboardApp(SDL_Window* window, ImFont* monoFont, const Start& start);

    // Called for every SDL event, after ImGui has seen it.
    void handleEvent(const SDL_Event& e);

    // Builds the frame's ImGui windows and keyboard drawing.
    void frame();

    // True while something is moving (a pointer is down), so the loop keeps
    // drawing at the display's rate instead of waiting for events.
    bool animating() const { return !pointers_.empty() || mouseDown_; }

private:
    struct Settings {
        Tuning       tuning;
        LayoutConfig layout;
        PlayConfig   play;
        OutputConfig output;
        TileColor    tileColor = TileColor::Note;
        bool         locked = false;

        bool operator==(const Settings&) const = default;
    };

    void rebuild(bool resetView);
    int  tiledRowsFor(double height) const;
    void drawSettings();
    void drawMonitor();
    void drawKeyboard(ImDrawList* list);
    void drainOutput();

    bool inKeyboard(Point p) const { return viewport_.screen.contains(p); }
    Modifiers modifiers() const;

    SDL_Window* window_;
    ImFont* monoFont_;
    Settings settings_;
    Settings applied_;

    std::unique_ptr<Layout> layout_;
    UmpOutput output_;
    Performer performer_;
    Viewport viewport_;

    // Pointer ids for the Performer: 0 is the mouse, fingers count from 1.
    std::map<SDL_FingerID, int> pointers_;
    int nextPointer_ = 1;
    bool mouseDown_ = false;

    std::deque<std::string> monitor_;
    bool monitorFollow_ = true;
    float barHeight_ = 0;
};

} // namespace kedo::app
