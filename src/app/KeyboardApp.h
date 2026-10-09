#pragma once

#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "MidiTargets.h"
#include "kedo/Document.h"
#include "kedo/Preferences.h"
#include "kedo/KeyStyle.h"
#include "kedo/Layout.h"
#include "kedo/Performer.h"
#include "kedo/Viewport.h"

struct ImDrawList;
struct ImFont;

// The app's one window: a settings bar, the keyboard, and a MIDI monitor.
// This file and its .cpp are the only code that knows SDL and ImGui; the
// keyboard itself is kedo::core, and where it plays is MidiTargets.
namespace kedo::app {

class KeyboardApp {
public:
    // How to start: from the command line (main.cpp). `path` is the document
    // `document` was loaded from, if any.
    struct Start {
        Document    document;
        std::string path;
        bool        publish = false;   // --publish: on for this run, whatever the preference
    };

    // `monoFont`: the MIDI monitor's font.
    KeyboardApp(SDL_Window* window, ImFont* monoFont, const Start& start);
    ~KeyboardApp();

    KeyboardApp(const KeyboardApp&) = delete;
    KeyboardApp& operator=(const KeyboardApp&) = delete;

    // Called for every SDL event, after ImGui has seen it.
    void handleEvent(const SDL_Event& e);

    // Sends what the keyboard played. Called after each batch of events, before
    // drawing, so a note doesn't wait for the frame.
    void flush();

    // Builds the frame's ImGui windows and keyboard drawing.
    void frame();

    // True while something is moving (a pointer is down), so the loop keeps
    // drawing at the display's rate instead of waiting for events.
    bool animating() const { return !pointers_.empty() || mouseDown_; }

private:
    enum class Dialog { Open, SaveAs };

    void rebuild(bool resetView);
    void applySettings();
    void open(const std::string& path);
    bool save(const std::string& path);
    void showDialog(Dialog dialog);
    void takeDialogResults();
    void updateTitle();
    void setPublish(bool on);
    int  tiledRowsFor(double height) const;
    void drawSettings();
    void drawMonitor();
    void drawKeyboard(ImDrawList* list);

    bool inKeyboard(Point p) const { return viewport_.screen.contains(p); }
    Modifiers modifiers() const;

    SDL_Window* window_;
    ImFont* monoFont_;
    Document settings_;       // what the controls show
    Document applied_;        // what the keyboard and outputs were last set from
    Document saved_;          // as last opened or saved, for the title's "edited"
    std::string path_;        // the document's file; empty until saved
    Preferences prefs_;       // app-level, remembered (publishing)
    std::string prefsPath_;   // in SDL's per-user preferences folder; empty if none

    std::unique_ptr<Layout> layout_;
    MidiTargets targets_;     // before performer_, which plays into it
    Performer performer_;
    Viewport viewport_;

    // Pointer ids for the Performer: 0 is the mouse, fingers count from 1.
    std::map<SDL_FingerID, int> pointers_;
    int nextPointer_ = 1;
    bool mouseDown_ = false;

    // File dialogs answer on SDL's terms (maybe another thread): queued here
    // and taken in frame().
    std::mutex dialogLock_;
    std::vector<std::pair<Dialog, std::string>> dialogResults_;
    std::string status_;      // the last file error, shown in the bar

    std::deque<std::string> monitor_;
    bool monitorFollow_ = true;
    float barHeight_ = 0;
};

} // namespace kedo::app
