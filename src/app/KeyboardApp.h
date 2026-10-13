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
    // How to start: from the command line (main.cpp). `path` is the .kedo
    // bundle `document` was loaded from, if any (a KeyEDO file is imported:
    // no path).
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
    enum class Dialog { Open, Import, SaveAs };

    void rebuild(bool resetView);
    void applySettings();
    // The document's layouts: the one showing is settings_ while it shows.
    void storeLayout() { doc_.layout() = settings_; }
    Document documentNow() const;
    void selectLayout(int index);
    void addLayout(LayoutSetup layout);   // after the current one, and shows it
    void removeLayout();
    void moveLayout(int by);
    void setEditing(bool on);
    void drawLayoutMenu(float width);
    void drawLayoutsSection();
    void open(const std::string& path);
    bool save(const std::string& path);
    void showDialog(Dialog dialog);
    void takeDialogResults();
    void updateTitle();
    void setPublish(bool on);
    PlayConfig playConfig() const;
    int  tiledRowsFor(double height) const;
    void drawToolbar();
    void drawRoutingMenu();
    float drawPanel();                 // returns its width
    float drawMonitor(float width);    // returns its height
    void setPanes(bool panel, bool monitor);
    void drawKeyboard(ImDrawList* list);

    bool inKeyboard(Point p) const { return viewport_.screen.contains(p); }
    Modifiers modifiers() const;
    void zoomBy(double factor);

    SDL_Window* window_;
    ImFont* monoFont_;
    Document doc_;            // the document; its current layout is settings_ while it shows
    Document saved_;          // as last opened or saved, for the title's "edited"
    LayoutSetup settings_;    // the layout showing, as the controls set it
    LayoutSetup applied_;     // what the keyboard and outputs were last set from
    std::string appliedDestination_;
    std::string path_;        // the document's .kedo bundle; empty until saved
    Preferences prefs_;       // app-level, remembered (publishing)
    std::string prefsPath_;   // in SDL's per-user preferences folder; empty if none

    std::unique_ptr<Layout> layout_;
    MidiTargets targets_;     // before performer_, which plays into it
    Performer performer_;
    Viewport viewport_;
    int originFrames_ = 0;    // frames left to keep Tiled's bottom row in view (frame())

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
