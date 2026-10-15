#pragma once

#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "MidiTargets.h"
#include "kedo/Arpeggiator.h"
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
        bool        ownPreferences = false;   // --snapshot: the defaults, the person's left alone
    };

    // `monoFont`: the MIDI monitor's font.
    KeyboardApp(SDL_Window* window, ImFont* monoFont, const Start& start);
    ~KeyboardApp();

    KeyboardApp(const KeyboardApp&) = delete;
    KeyboardApp& operator=(const KeyboardApp&) = delete;

    // These three take the play lock, which the arpeggiator's thread also
    // takes (decisions.md, "Arpeggiator").
    // Called for every SDL event, after ImGui has seen it.
    void handleEvent(const SDL_Event& e);

    // Sends what the keyboard played. Called after each batch of events, before
    // drawing, so a note doesn't wait for the frame.
    void flush();

    // Builds the frame's ImGui windows and keyboard drawing.
    void frame();

    // True while something is moving (a pointer is down, a key is dragged, an
    // audition is to end), so the loop keeps drawing at the display's rate
    // instead of waiting for events.
    // The arpeggiator playing counts, so the key it's on is drawn.
    bool animating() const;

private:
    enum class Dialog { Open, Import, SaveAs };

    void rebuild(bool resetView);
    // Releases what's held and stops the arpeggiator, Hold or not.
    void releaseAll();
    // The arpeggiator's thread: steps it on time, and sends what it plays.
    void arpLoop();
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

    // The custom layout editor: Edit mode on a custom layout (decisions.md,
    // "Custom layout"). A click selects keys instead of playing them.
    bool editingKeys() const { return prefs_.editing && settings_.layout.kind == LayoutKind::Custom; }
    std::vector<CustomKey>& keys() { return settings_.layout.customKeys; }
    void resetEditor();
    void editPress(Point screen, bool shift, bool doubleClick);
    void editDrag(Point screen);
    bool editKey(const SDL_KeyboardEvent& key);   // true if it was the editor's
    void pushUndo();
    void undo();
    void redo();
    double gridStep() const;
    Point snapped(Point p) const;
    void addKeyAt(Point at);
    void deleteSelected();
    void duplicateSelected();
    void nudge(double dx, double dy);
    void pitchChanged(int index);   // re-suggest the number
    void audition(int index);
    void drawKeyInspector();
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
    bool computerKey(const SDL_KeyboardEvent& key);   // true if it was used
    std::string computerKeyLabel(int code) const;
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
    std::map<int, Key> computerKeys_;   // the layout's, by computer key
    std::set<int> heldKeys_;            // computer keys down, playing
    bool assigning_ = false;            // the inspector's Assign: the next key pressed
    MidiTargets targets_;     // before arp_ and performer_, which play into it
    Arpeggiator arp_;         // between the Performer and the outputs
    Performer performer_;
    // The arpeggiator steps on a thread of its own, as the frame loop waits
    // for vsync. The lock guards everything both threads touch: the
    // Performer, arpeggiator, outputs and monitor (recursive: frame() flushes).
    mutable std::recursive_mutex playLock_;
    std::condition_variable_any arpWake_;
    bool quitting_ = false;
    std::thread arpThread_;
    Viewport viewport_;
    int originFrames_ = 0;    // frames left to keep Tiled's bottom row in view (frame())

    // Pointer ids for the Performer: 0 is the mouse, fingers count from 1.
    std::map<SDL_FingerID, int> pointers_;
    int nextPointer_ = 1;
    bool mouseDown_ = false;
    Point mouseAt_;           // where the mouse last moved, in the window (hover expression)

    // File dialogs answer on SDL's terms (maybe another thread): queued here
    // and taken in frame().
    std::mutex dialogLock_;
    std::vector<std::pair<Dialog, std::string>> dialogResults_;
    std::string status_;      // the last file error, shown in the bar

    // The editor's state. Undo keeps the keys and the selection.
    struct EditState {
        std::vector<CustomKey> keys;
        std::set<int> selected;
        bool operator==(const EditState&) const = default;
    };
    void pushUndo(EditState state);
    std::set<int> selected_;          // indices into keys()
    bool dragging_ = false;
    bool dragMoved_ = false;
    Point dragFrom_;                  // where the drag began, in layout units
    int dragAnchor_ = -1;             // the key grabbed: it snaps, the others follow
    std::vector<Point> dragOrigins_;  // the selected keys' centres when it began
    bool shiftDown_ = false;          // from Shift's own key events, beside SDL's modifier state
    bool snap_ = true;
    bool halfGrid_ = false;           // snap to half the grid step
    std::vector<EditState> undo_, redo_;
    std::optional<int> auditionId_;   // the note an inspector change is sounding
    Uint64 auditionOff_ = 0;          // when it stops (SDL ticks)

    std::deque<std::string> monitor_;
    bool monitorFollow_ = true;
    float barHeight_ = 0;
};

} // namespace kedo::app
