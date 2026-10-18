#include "KeyboardApp.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <tuple>

#include "Icons.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui.h"
#include "kedo/ComputerKeys.h"
#include "kedo/CustomLayout.h"
#include "kedo/Synopsis.h"

extern const unsigned char kBrandLogo[];   // the toolbar's Unlikelyware logo (PNG)
extern const unsigned int kBrandLogoSize;

namespace kedo::app {

namespace {

constexpr float kMonitorHeight = 170;
constexpr size_t kMonitorLines = 1000;
constexpr double kWheelPoints = 30;   // scroll distance per wheel unit
constexpr double kZoomStep = 1.25;    // the zoom buttons' and keys' factor

const double kHalfDiag = TiledLayout::kKeySize * std::sqrt(2.0) / 2;

ImU32 color(const Rgb& c, float alpha = 1) { return ImGui::ColorConvertFloat4ToU32({c.r, c.g, c.b, alpha}); }
ImVec2 vec(Point p) { return {float(p.x), float(p.y)}; }

// A combo over a fixed list of names.
template <typename E, size_t N>
bool enumCombo(const char* label, E& value, const char* const (&names)[N], float width) {
    int i = int(value);
    ImGui::SetNextItemWidth(width);
    const bool changed = ImGui::Combo(label, &i, names, int(N));
    if (changed) value = E(i);
    return changed;
}

} // namespace

KeyboardApp::KeyboardApp(SDL_Window* window, ImFont* monoFont, ImFont* brandFont, const Start& start)
    : window_(window),
      monoFont_(monoFont),
      brandFont_(brandFont),
      doc_(start.document),
      saved_(start.document),
      settings_(start.document.layout()),
      applied_(settings_),
      appliedDestination_(start.document.destination),
      path_(start.path),
      targets_(settings_.output),
      arp_(targets_.sink(), [] { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }),
      performer_(arp_),
      controls_(targets_.sink()) {
    if (start.ownPreferences) {
        // A check run (--snapshot): defaults, read from and saved to nowhere,
        // so it neither depends on nor changes the preferences of the
        // KeyboardEDO the author may be playing meanwhile.
    } else if (char* folder = SDL_GetPrefPath("unlikelyware", "KeyboardEDO")) {
        prefsPath_ = std::string(folder) + "preferences.json";
        SDL_free(folder);
        prefs_ = loadPreferences(prefsPath_);
    }
    if (prefs_.productInstanceId.empty()) {
        // Made once per installation (a check run's is its own, unsaved).
        prefs_.productInstanceId = makeProductInstanceId();
        if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
    }
    rebuild(true);
    targets_.setConfig(settings_.output);
    targets_.setDestination(doc_.destination);
    targets_.setProductInstanceId(prefs_.productInstanceId);
    appliedSpan_ = groupSpan(doc_);
    targets_.setGroups(appliedSpan_.first, appliedSpan_.count);
    targets_.setPublish(prefs_.publish || start.publish);
    arp_.setConfig(settings_.arp);
    controls_.setKnobs(doc_.knobs);
    updateTitle();
    arpThread_ = std::thread([this] { arpLoop(); });
}

void KeyboardApp::arpLoop() {
    std::unique_lock lock(playLock_);
    while (!quitting_) {
        arp_.advance();
        flush();
        if (const auto next = arp_.nextEvent())
            arpWake_.wait_until(lock, std::chrono::steady_clock::time_point(
                                          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                              std::chrono::duration<double>(*next))));
        else
            arpWake_.wait(lock);
    }
}

bool KeyboardApp::animating() const {
    std::lock_guard lock(playLock_);
    return !pointers_.empty() || mouseDown_ || dragging_ || auditionId_.has_value() || arp_.running();
}

void KeyboardApp::releaseAll() {
    performer_.releaseAll();
    arp_.stop();
}

// The Performer's settings: the document's, except that with expression sent
// as MIDI 1.0, pitch bend and controllers can only be channel-wide (MIDI 1.0
// has no per-note forms), so the first finger drives them rather than every
// finger bending the one channel. Polyphonic pressure is MIDI 1.0 too.
PlayConfig KeyboardApp::playConfig() const {
    PlayConfig p = settings_.play;
    if (settings_.output.midi1Expression)
        p.bendScope = p.controllerScope = ExpressionScope::Channel;
    return p;
}

void KeyboardApp::setPublish(bool on) {
    releaseAll();
    targets_.setPublish(on);
    prefs_.publish = on;
    if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
}

KeyboardApp::~KeyboardApp() {
    {
        std::lock_guard lock(playLock_);
        quitting_ = true;
    }
    arpWake_.notify_one();
    arpThread_.join();
    // Release what's held while the outputs still exist; their destructor
    // then sends All Notes Off.
    releaseAll();
    flush();
    if (logo_) SDL_DestroyTexture(logo_);
}

// --- Building the keyboard --------------------------------------------------------

int KeyboardApp::tiledRowsFor(double height) const {
    // As many rows as fit, as KeyEDO: rows are half a diamond apart.
    const int rows = int((height / viewport_.zoom - 2 * kHalfDiag) / kHalfDiag) + 1;
    return std::clamp(rows, 1, 40);
}

void KeyboardApp::rebuild(bool resetView) {
    performer_.releaseAll();
    if (settings_.layout.kind == LayoutKind::Tiled)
        settings_.layout.tiledRows = tiledRowsFor(viewport_.screen.h > 0 ? viewport_.screen.h : 400);
    auto layout = makeLayout(settings_.layout, settings_.tuning);
    performer_.setLayout(layout.get());
    layout_ = std::move(layout);
    computerKeys_ = computerKeyMap(*layout_);

    // The arpeggiator's octave copies: the layout's own note at that pitch on
    // the same group and channel, so EDO numbering holds; else 12 up.
    using Place = std::tuple<int, int, long long>;
    auto place = [](const Note& n, double pitch) {
        return Place{n.group.value_or(255), n.channel.value_or(255), std::llround(pitch * 1e4)};
    };
    std::map<Place, Note> notes;
    const Rect b = layout_->bounds();
    for (const Key& k : layout_->keysIn({b.x - 1, b.y - 1, b.w + 2, b.h + 2}))
        if (k.playable) notes.emplace(place(k.note, k.note.pitch), k.note);
    arp_.setOctaveFinder([notes = std::move(notes), place](const Note& n, int octaves) {
        const auto it = notes.find(place(n, n.pitch + 12.0 * octaves));
        return it != notes.end() ? it->second : Arpeggiator::octaveAbove(n, octaves);
    });
    heldKeys_.clear();   // setLayout released them
    performer_.setConfig(playConfig());

    if (resetView) {
        // Start at zoom 1, the lowest keys at the left (Tiled: the origin).
        viewport_.zoom = 1;
        viewport_.scroll = {layout_->bounds().x, layout_->bounds().y};
        // Tiled and Octave columns: the lowest key (the origin note in octave
        // -1) at the left, and (frame(), originFrames_) its row, the bottom
        // one, at the bottom.
        if (settings_.layout.kind == LayoutKind::Tiled)
            if (const auto k = layout_->keyForStep(settings_.tuning.lowestStep())) {
                viewport_.scroll.x = k->center.x - 2 * kHalfDiag;
                originFrames_ = 2;
            }
        // Custom: the bottom left, where the lowest keys usually are.
        if (settings_.layout.kind == LayoutKind::Custom) originFrames_ = 2;
    }
}

// Zoom about the keyboard's centre (buttons and keys; the wheel and pinch
// zoom about the pointer). Not while locked.
void KeyboardApp::zoomBy(double factor) {
    if (settings_.locked) return;
    const Rect& s = viewport_.screen;
    viewport_.zoomAbout({s.x + s.w / 2, s.y + s.h / 2}, factor);
}

Modifiers KeyboardApp::modifiers() const {
    return {.latch = shiftDown_ || (SDL_GetModState() & SDL_KMOD_SHIFT) != 0};
}

// A computer key (decisions.md, "Computer keyboard"), by its physical place:
// assigning it, or playing the note key it's on. True if it was used.
bool KeyboardApp::computerKey(const SDL_KeyboardEvent& key) {
    const ImGuiIO& io = ImGui::GetIO();
    const int code = int(key.scancode);
    if (key.down && assigning_) {
        if (key.key == SDLK_ESCAPE) {
            assigning_ = false;
            return true;
        }
        if (!isAssignableComputerKey(code) || !editingKeys() || selected_.size() != 1) return true;
        // One note key each: taken from any other.
        pushUndo();
        for (CustomKey& k : keys())
            if (k.computerKey == code) k.computerKey = 0;
        keys()[*selected_.begin()].computerKey = code;
        assigning_ = false;
        return true;
    }
    if (!key.down) {
        if (!heldKeys_.erase(code)) return false;
        performer_.keyUp(code, modifiers());
        return true;
    }
    // Not while typing, and Cmd/Ctrl with a key is a shortcut.
    if (io.WantTextInput || (key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL))) return false;
    const auto it = computerKeys_.find(code);
    if (it == computerKeys_.end()) return false;
    if (!key.repeat && heldKeys_.insert(code).second) performer_.keyDown(code, it->second.note, modifiers());
    return true;
}

// What a computer key is called on this keyboard ("Q", or "A" on AZERTY).
std::string KeyboardApp::computerKeyLabel(int code) const {
    const SDL_Keycode key = SDL_GetKeyFromScancode(SDL_Scancode(code), SDL_KMOD_NONE, false);
    const char* name = key != SDLK_UNKNOWN ? SDL_GetKeyName(key) : "";
    return name && *name ? name : computerKeyName(code);
}

// --- Input ------------------------------------------------------------------------

Point KeyboardApp::dragPoint(Point screen) const {
    if (settings_.play.drag == DragBehavior::Gliss) {
        const Rect& s = viewport_.screen;
        screen.x = std::clamp(screen.x, s.x, std::max(s.x, s.right() - 1));
        screen.y = std::clamp(screen.y, s.y, std::max(s.y, s.bottom() - 1));
    }
    return viewport_.toLayout(screen);
}

// Takes the play lock, and wakes the arpeggiator's thread after, as what
// happened may have given it notes.
#define KEDO_PLAYING                                                        \
    std::lock_guard lock(playLock_);                                        \
    struct Wake {                                                           \
        std::condition_variable_any& cv;                                    \
        ~Wake() { cv.notify_one(); }                                        \
    } wake{arpWake_}

void KeyboardApp::handleEvent(const SDL_Event& e) {
    KEDO_PLAYING;
    const ImGuiIO& io = ImGui::GetIO();
    switch (e.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        if (e.button.button != SDL_BUTTON_LEFT) break;
        const Point p{e.button.x, e.button.y};
        if (!inKeyboard(p) || io.WantCaptureMouse) break;
        // Editing a custom layout: the editor's, touches included (SDL's
        // mouse copies of them).
        if (editingKeys()) {
            editPress(p, shiftDown_ || (SDL_GetModState() & SDL_KMOD_SHIFT) != 0, e.button.clicks >= 2);
            break;
        }
        // Touches arrive as fingers below; SDL's mouse copies of them are ImGui's.
        if (e.button.which == SDL_TOUCH_MOUSEID) break;
        mouseDown_ = true;
        performer_.pointerDown(0, viewport_.toLayout(p), modifiers());
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (e.motion.which != SDL_TOUCH_MOUSEID) {
            mouseAt_ = {e.motion.x, e.motion.y};
            if (performer_.hovering()) performer_.hoverMove(viewport_.toLayout(mouseAt_));
        }
        if (dragging_) editDrag({e.motion.x, e.motion.y});
        else if (mouseDown_ && e.motion.which != SDL_TOUCH_MOUSEID)
            performer_.pointerMove(0, dragPoint({e.motion.x, e.motion.y}));
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (dragging_ && e.button.button == SDL_BUTTON_LEFT) dragging_ = false;
        else if (mouseDown_ && e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) {
            performer_.pointerUp(0, modifiers());
            mouseDown_ = false;
        }
        break;

    case SDL_EVENT_FINGER_DOWN: {
        // Only touchscreens: a trackpad's fingers have no place on the screen.
        // Editing keys, the editor has them as the mouse.
        if (editingKeys() || SDL_GetTouchDeviceType(e.tfinger.touchID) != SDL_TOUCH_DEVICE_DIRECT) break;
        int w = 0, h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        const Point p{e.tfinger.x * w, e.tfinger.y * h};
        if (!inKeyboard(p)) break;
        const int id = nextPointer_++;
        pointers_[e.tfinger.fingerID] = id;
        performer_.pointerDown(id, viewport_.toLayout(p), modifiers());
        break;
    }
    case SDL_EVENT_FINGER_MOTION: {
        const auto it = pointers_.find(e.tfinger.fingerID);
        if (it == pointers_.end()) break;
        int w = 0, h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        performer_.pointerMove(it->second, dragPoint({e.tfinger.x * w, e.tfinger.y * h}));
        break;
    }
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        const auto it = pointers_.find(e.tfinger.fingerID);
        if (it == pointers_.end()) break;
        if (e.type == SDL_EVENT_FINGER_UP) performer_.pointerUp(it->second, modifiers());
        else performer_.pointerCancel(it->second);
        pointers_.erase(it);
        break;
    }

    case SDL_EVENT_MOUSE_WHEEL: {
        if (settings_.locked || e.wheel.which == SDL_TOUCH_MOUSEID) break;
        const Point at{e.wheel.mouse_x, e.wheel.mouse_y};
        if (!inKeyboard(at) || io.WantCaptureMouse) break;
        if (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) {
            viewport_.zoomAbout(at, std::pow(1.1, e.wheel.y));
            break;
        }
        // SDL's wheel values already follow the system's scroll direction
        // ("natural" on the Mac); moving the view by them makes the content
        // follow the fingers. (The author found the opposite sign backwards.)
        double dx = e.wheel.x * kWheelPoints, dy = e.wheel.y * kWheelPoints;
        // A keyboard that fits vertically scrolls sideways with a plain wheel;
        // one that doesn't (Octave columns), with Shift held, as is usual.
        const bool fits = layout_->bounds().h * viewport_.zoom <= viewport_.screen.h;
        if ((fits || (SDL_GetModState() & SDL_KMOD_SHIFT)) && dx == 0) std::swap(dx, dy);
        viewport_.scrollBy(dx, dy);
        break;
    }
    case SDL_EVENT_PINCH_UPDATE: {
        if (settings_.locked) break;
        float x = 0, y = 0;
        SDL_GetMouseState(&x, &y);
        const Point at = inKeyboard({x, y}) ? Point{x, y}
                                            : Point{viewport_.screen.x + viewport_.screen.w / 2,
                                                    viewport_.screen.y + viewport_.screen.h / 2};
        viewport_.zoomAbout(at, e.pinch.scale);
        break;
    }

    case SDL_EVENT_KEY_UP:
        if (e.key.key == SDLK_LSHIFT || e.key.key == SDLK_RSHIFT) shiftDown_ = false;
        if (e.key.key == SDLK_LCTRL || e.key.key == SDLK_RCTRL) performer_.hoverEnd();
        computerKey(e.key);
        break;
    case SDL_EVENT_KEY_DOWN: {
        if (e.key.key == SDLK_LSHIFT || e.key.key == SDLK_RSHIFT) shiftDown_ = true;
        // Ctrl down: hover expression, from where the mouse is.
        if ((e.key.key == SDLK_LCTRL || e.key.key == SDLK_RCTRL) && !e.key.repeat)
            performer_.hoverBegin(viewport_.toLayout(mouseAt_));
        if (computerKey(e.key)) break;
        if (editingKeys() && !io.WantCaptureKeyboard && editKey(e.key)) break;
        if (e.key.key == SDLK_ESCAPE && !io.WantCaptureKeyboard) releaseAll();
        if (e.key.key == SDLK_F1 && !e.key.repeat) openHelp();
        // Cmd (Mac) or Ctrl: O open, S save, Shift-S save as; = (or +) and -
        // zoom, and 0 back to zoom 1, about the keyboard's centre.
        const bool command = (e.key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL)) != 0;
        if (command) {
            if (e.key.key == SDLK_EQUALS || e.key.key == SDLK_PLUS || e.key.key == SDLK_KP_PLUS) zoomBy(kZoomStep);
            if (e.key.key == SDLK_MINUS || e.key.key == SDLK_KP_MINUS) zoomBy(1 / kZoomStep);
            if (e.key.key == SDLK_0 || e.key.key == SDLK_KP_0) zoomBy(1 / viewport_.zoom);
        }
        if (command && !e.key.repeat) {
            if (e.key.key == SDLK_O) showDialog(Dialog::Open);
            if (e.key.key == SDLK_SLASH && (e.key.mod & SDL_KMOD_SHIFT)) openHelp();   // Cmd-?
            if (e.key.key == SDLK_S) {
                if ((e.key.mod & SDL_KMOD_SHIFT) || path_.empty()) showDialog(Dialog::SaveAs);
                else save(path_);
            }
        }
        break;
    }

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        // A release we'll never see would leave a note stuck.
        if (mouseDown_) performer_.pointerCancel(0);
        mouseDown_ = false;
        performer_.hoverEnd();
        for (const int code : heldKeys_) performer_.keyUp(code);
        heldKeys_.clear();
        assigning_ = false;
        dragging_ = false;
        shiftDown_ = false;   // its release goes elsewhere
        break;
    default:
        break;
    }
}

// --- Frame --------------------------------------------------------------------------

void KeyboardApp::frame() {
    KEDO_PLAYING;
    int w = 0, h = 0;
    SDL_GetWindowSize(window_, &w, &h);

    // The toolbar across the top, the performance bar down the left, the
    // settings panel down the right, the monitor under the keyboard; the
    // keyboard gets what's left.
    drawToolbar();
    const float barWidth = prefs_.barOpen ? drawBar() : 0.0f;
    const float panelWidth = prefs_.panelOpen ? drawPanel() : 0.0f;
    const float monitorHeight =
        prefs_.monitorOpen ? drawMonitor(barWidth, std::max(0.0f, float(w) - barWidth - panelWidth)) : 0.0f;
    viewport_.screen = {barWidth, barHeight_, std::max(0.0, double(w) - barWidth - panelWidth),
                        std::max(0.0, double(h) - barHeight_ - monitorHeight)};

    takeDialogResults();
    if (auditionId_ && SDL_GetTicks() >= auditionOff_) {
        targets_.sink().noteOff(*auditionId_);
        auditionId_.reset();
    }
    if (!editingKeys()) {
        selected_.clear();
        dragging_ = false;
    }
    applySettings();
    {
        std::lock_guard lock(playLock_);
        targets_.poll();
    }

    // After a reset, Tiled's bottom row at the bottom: clampTo turns the
    // bottom of the layout into the lowest scroll that shows it, or centres
    // rows that fit. Kept for two frames, as ImGui sizes the toolbar a frame
    // late, so the first frame's keyboard area is too tall. Octave columns'
    // rows don't fit, and scroll.
    const bool resetting = originFrames_ > 0;
    if (originFrames_ > 0) {
        --originFrames_;
        viewport_.scroll.y = layout_->bounds().bottom();
    }
    // Editing keys, the view stays where it's put once it's set: centring or
    // clamping to the keys would move them out from under the pointer as they
    // move, and there's room needed to place new ones.
    if (!editingKeys() || resetting) viewport_.clampTo(layout_->bounds());
    drawKeyboard(ImGui::GetBackgroundDrawList());
    flush();
    updateTitle();
}

void KeyboardApp::flush() {
    std::lock_guard lock(playLock_);
    targets_.flush(monitor_, kMonitorLines);
}

// Apply what the settings bar (or a document) changed.
void KeyboardApp::applySettings() {
    // A new destination or publishing starts from silence: notes held now
    // would never get their note-off there.
    if (doc_.destination != appliedDestination_) {
        releaseAll();
        targets_.setDestination(doc_.destination);
        appliedDestination_ = doc_.destination;
    }
    if (!(settings_.output == applied_.output)) {
        releaseAll();
        targets_.setConfig(settings_.output);
    }
    // The published endpoint's function block spans the whole document's
    // groups, so switching layouts leaves it be (decisions.md, "Identity and
    // discovery"); opening a document or changing a group may move it.
    if (const GroupSpan span = groupSpan(doc_, &settings_); !(span == appliedSpan_)) {
        std::lock_guard lock(playLock_);
        targets_.setGroups(span.first, span.count);
        appliedSpan_ = span;
    }
    // The arpeggiator on or off: what's held is released the way it was
    // played (through it, or past it), then it starts empty.
    if (!(settings_.arp == applied_.arp)) {
        if (settings_.arp.enabled != applied_.arp.enabled) releaseAll();
        arp_.setConfig(settings_.arp);
    }
    if (!(doc_.knobs == controls_.knobs())) controls_.setKnobs(doc_.knobs);
    const bool keysChanged = !(settings_.tuning == applied_.tuning) || !(settings_.layout == applied_.layout);
    if (!(settings_.play == applied_.play) || settings_.output.midi1Expression != applied_.output.midi1Expression)
        performer_.setConfig(playConfig());
    if (keysChanged) {
        // Becoming custom isn't new keys: converting keeps them where they are.
        const bool newKeys = settings_.tuning.edo != applied_.tuning.edo ||
                             (settings_.layout.kind != applied_.layout.kind &&
                              settings_.layout.kind != LayoutKind::Custom);
        arp_.stop();   // what Hold kept was the old keys' notes
        rebuild(newKeys);
    } else if (settings_.layout.kind == LayoutKind::Tiled && !animating() &&
               tiledRowsFor(viewport_.screen.h) != settings_.layout.tiledRows) {
        rebuild(false);   // the window or zoom changed how many rows fit
    }
    applied_ = settings_;
}

// --- Documents ----------------------------------------------------------------------

// A .kedo bundle, or a KeyEDO file, imported: a document of one layout, not
// saved until Save As makes it a bundle.
void KeyboardApp::open(const std::string& path) {
    std::string error;
    const auto document = loadDocument(path, &error);
    if (!document) {
        status_ = error;
        return;
    }
    const bool imported = !isBundlePath(path);
    status_ = imported ? "Imported: Save makes it a .kedo document" : "";
    releaseAll();
    doc_ = *document;
    saved_ = *document;
    settings_ = doc_.layout();
    resetEditor();
    path_ = imported ? "" : path;
    applySettings();
    rebuild(true);
}

bool KeyboardApp::save(const std::string& path) {
    storeLayout();
    std::string error;
    if (!saveDocument(path, doc_, &error)) {
        status_ = error;
        return false;
    }
    status_.clear();
    saved_ = doc_;
    path_ = path;
    return true;
}

// The document as it would be saved: the showing layout's settings stored.
Document KeyboardApp::documentNow() const {
    Document d = doc_;
    d.layout() = settings_;
    return d;
}

// --- Layouts ----------------------------------------------------------------------------

// Switching releases what's held (the author's choice), then the new layout's
// settings apply as if chosen: a new output setup goes out, the keys rebuild,
// and the view starts afresh.
void KeyboardApp::selectLayout(int index) {
    if (index == doc_.current || index < 0 || index >= int(doc_.layouts.size())) return;
    storeLayout();
    releaseAll();
    doc_.current = index;
    settings_ = doc_.layout();
    resetEditor();
    applySettings();
    rebuild(true);
}

void KeyboardApp::addLayout(LayoutSetup layout) {
    storeLayout();
    doc_.layouts.insert(doc_.layouts.begin() + doc_.current + 1, std::move(layout));
    selectLayout(doc_.current + 1);
}

void KeyboardApp::removeLayout() {
    if (doc_.layouts.size() < 2) return;
    releaseAll();
    doc_.layouts.erase(doc_.layouts.begin() + doc_.current);
    doc_.current = std::min(doc_.current, int(doc_.layouts.size()) - 1);
    settings_ = doc_.layout();
    resetEditor();
    applySettings();
    rebuild(true);
}

void KeyboardApp::moveLayout(int by) {
    const int to = doc_.current + by;
    if (to < 0 || to >= int(doc_.layouts.size())) return;
    storeLayout();
    std::swap(doc_.layouts[doc_.current], doc_.layouts[to]);
    doc_.current = to;
}

void KeyboardApp::setEditing(bool on) {
    prefs_.editing = on;
    if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
}

// --- The custom layout editor -----------------------------------------------------------

// A layout shown afresh: nothing selected, nothing to undo.
void KeyboardApp::resetEditor() {
    selected_.clear();
    dragging_ = false;
    undo_.clear();
    redo_.clear();
}

void KeyboardApp::pushUndo() { pushUndo({keys(), selected_}); }

void KeyboardApp::pushUndo(EditState state) {
    if (!undo_.empty() && undo_.back() == state) return;   // nothing changed since
    undo_.push_back(std::move(state));
    if (undo_.size() > 200) undo_.erase(undo_.begin());
    redo_.clear();
}

void KeyboardApp::undo() {
    if (undo_.empty()) return;
    redo_.push_back({keys(), selected_});
    keys() = undo_.back().keys;
    selected_ = undo_.back().selected;
    undo_.pop_back();
}

void KeyboardApp::redo() {
    if (redo_.empty()) return;
    undo_.push_back({keys(), selected_});
    keys() = redo_.back().keys;
    selected_ = redo_.back().selected;
    redo_.pop_back();
}

// Half a diamond's width: the generated diamond layouts' key centres all lie
// on this grid, and neighbours on it share edges (decisions.md).
double KeyboardApp::gridStep() const { return halfGrid_ ? kHalfDiag / 2 : kHalfDiag; }

Point KeyboardApp::snapped(Point p) const {
    if (!snap_) return p;
    const double step = gridStep();
    return {std::round(p.x / step) * step, std::round(p.y / step) * step};
}

// A click selects (Shift adds or removes) and starts a drag; a double click
// on empty space adds a key there; a click on empty space clears the selection.
void KeyboardApp::editPress(Point screen, bool shift, bool doubleClick) {
    const Point at = viewport_.toLayout(screen);
    // What's under the pointer as drawn: a selected key is on top.
    int index = -1;
    if (const auto* custom = dynamic_cast<const CustomLayout*>(layout_.get()))
        for (auto it = selected_.rbegin(); it != selected_.rend() && index < 0; ++it)
            if (*it < int(custom->keys().size()) && custom->keys()[*it].contains(at)) index = *it;
    if (index < 0)
        if (const auto key = layout_->keyAt(at)) index = key->row;   // CustomLayout: the key's index
    if (index < 0) {
        if (doubleClick) addKeyAt(at);
        else if (!shift) selected_.clear();
        return;
    }
    if (shift) {
        if (!selected_.erase(index)) selected_.insert(index);
        if (!selected_.count(index)) return;   // deselected: nothing to drag
    } else if (!selected_.count(index)) {
        selected_ = {index};
    }
    dragging_ = true;
    dragMoved_ = false;
    dragFrom_ = at;
    dragAnchor_ = index;
    dragOrigins_.clear();
    for (const auto& k : keys()) dragOrigins_.push_back(k.center);
}

// The grabbed key follows the pointer, snapped; the rest of the selection
// keeps its place relative to it.
void KeyboardApp::editDrag(Point screen) {
    if (dragAnchor_ < 0 || dragAnchor_ >= int(keys().size())) return;
    const Point at = viewport_.toLayout(screen);
    const Point from = dragOrigins_[dragAnchor_];
    const Point to = snapped({from.x + at.x - dragFrom_.x, from.y + at.y - dragFrom_.y});
    const double dx = to.x - from.x, dy = to.y - from.y;
    if (!dragMoved_ && dx == 0 && dy == 0) return;
    if (!dragMoved_) pushUndo();
    dragMoved_ = true;
    for (const int i : selected_)
        if (i < int(keys().size())) keys()[i].center = {dragOrigins_[i].x + dx, dragOrigins_[i].y + dy};
}

bool KeyboardApp::editKey(const SDL_KeyboardEvent& key) {
    const bool command = (key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL)) != 0;
    const bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
    const double step = gridStep() * (shift ? 4 : 1);
    switch (key.key) {
    case SDLK_Z:
        if (!command) return false;
        if (shift) redo();
        else undo();
        return true;
    case SDLK_Y:
        if (!command) return false;
        redo();
        return true;
    case SDLK_A:
        if (!command) return false;
        selected_.clear();
        for (int i = 0; i < int(keys().size()); ++i) selected_.insert(i);
        return true;
    case SDLK_D:
        if (!command) return false;
        duplicateSelected();
        return true;
    case SDLK_DELETE:
    case SDLK_BACKSPACE: deleteSelected(); return true;
    case SDLK_ESCAPE: selected_.clear(); return false;   // Escape also releases notes
    case SDLK_LEFT: nudge(-step, 0); return true;
    case SDLK_RIGHT: nudge(step, 0); return true;
    case SDLK_UP: nudge(0, -step); return true;
    case SDLK_DOWN: nudge(0, step); return true;
    default: return false;
    }
}

// A new key at `at` (snapped): the selected key's pitch a step up, or the
// origin note in octave 4, in the layout's EDO; its note number suggested and
// its label its note's name and octave.
void KeyboardApp::addKeyAt(Point at) {
    pushUndo();
    CustomKey k;
    k.center = snapped(at);
    k.edo = settings_.tuning.edo;
    if (!selected_.empty() && *selected_.begin() < int(keys().size())) {
        const CustomKey& from = keys()[*selected_.begin()];
        k.edo = from.edo;
        k.octave = from.octave + (from.degree + 1) / from.edo;
        k.degree = (from.degree + 1) % from.edo;
        k.halfWidth = from.halfWidth;
        k.halfHeight = from.halfHeight;
    }
    keys().push_back(k);
    const int index = int(keys().size()) - 1;
    keys()[index].number = suggestNumber(keys(), index, settings_.tuning);
    keys()[index].labelStyle = LabelStyle::NameOctave;
    selected_ = {index};
    audition(index);
}

void KeyboardApp::deleteSelected() {
    if (selected_.empty()) return;
    pushUndo();
    for (auto it = selected_.rbegin(); it != selected_.rend(); ++it)
        if (*it < int(keys().size())) keys().erase(keys().begin() + *it);
    selected_.clear();
}

// Copies a grid step right and down, the copies selected, each copy's number
// suggested afresh (the original keeps its own).
void KeyboardApp::duplicateSelected() {
    if (selected_.empty()) return;
    pushUndo();
    std::set<int> copies;
    for (const int i : selected_) {
        if (i >= int(keys().size())) continue;
        CustomKey copy = keys()[i];
        copy.center = {copy.center.x + 2 * gridStep(), copy.center.y};
        copy.numberChosen = false;
        copy.computerKey = 0;   // one note key each
        keys().push_back(copy);
        copies.insert(int(keys().size()) - 1);
    }
    for (const int i : copies) keys()[i].number = suggestNumber(keys(), i, settings_.tuning);
    selected_ = copies;
}

void KeyboardApp::nudge(double dx, double dy) {
    if (selected_.empty()) return;
    pushUndo();
    for (const int i : selected_)
        if (i < int(keys().size())) keys()[i].center = {keys()[i].center.x + dx, keys()[i].center.y + dy};
}

// After a key's pitch changed: a suggested number follows it. (Labels from
// the pitch follow it by themselves.)
void KeyboardApp::pitchChanged(int index) {
    CustomKey& k = keys()[index];
    if (!k.numberChosen) k.number = suggestNumber(keys(), index, settings_.tuning);
}

// Sounds the key's note briefly through the outputs, as playing it would
// (decisions.md: auditioning edits).
void KeyboardApp::audition(int index) {
    if (index < 0 || index >= int(keys().size())) return;
    NoteSink& sink = targets_.sink();
    if (auditionId_) sink.noteOff(*auditionId_);
    const CustomLayout one({keys()[index]}, settings_.tuning);
    const Key& k = one.keys().front();
    if (!k.playable) {
        auditionId_.reset();
        return;
    }
    sink.noteOn(k.note, 0xC000);   // its id is the one the whole layout gives it
    auditionId_ = k.note.id;
    auditionOff_ = SDL_GetTicks() + 400;
}

void KeyboardApp::showDialog(Dialog dialog) {
    static const SDL_DialogFileFilter kBundles[] = {{"KeyboardEDO documents", "kedo"}};
    static const SDL_DialogFileFilter kKeyEdo[] = {{"KeyEDO documents (JSON)", "json"}};
    struct Request {
        KeyboardApp* app;
        Dialog dialog;
    };
    // SDL calls back once, perhaps on another thread; the request lives until then.
    auto* request = new Request{this, dialog};
    const auto callback = [](void* user, const char* const* files, int) {
        std::unique_ptr<Request> r(static_cast<Request*>(user));
        if (!files || !files[0]) return;   // cancelled, or failed
        std::lock_guard<std::mutex> g(r->app->dialogLock_);
        r->app->dialogResults_.emplace_back(r->dialog, files[0]);
    };
    // A bundle is a folder (on the Mac, once the app declares .kedo a
    // package, the file dialog will show it as one file).
    if (dialog == Dialog::Open)
        SDL_ShowOpenFolderDialog(callback, request, window_, nullptr, false);
    else if (dialog == Dialog::Import)
        SDL_ShowOpenFileDialog(callback, request, window_, kKeyEdo, 1, nullptr, false);
    else
        SDL_ShowSaveFileDialog(callback, request, window_, kBundles, 1, path_.empty() ? nullptr : path_.c_str());
}

void KeyboardApp::takeDialogResults() {
    std::vector<std::pair<Dialog, std::string>> results;
    {
        std::lock_guard<std::mutex> g(dialogLock_);
        results.swap(dialogResults_);
    }
    for (auto& [dialog, path] : results) {
        if (dialog != Dialog::SaveAs) {
            open(path);
        } else {
            if (path.size() < 5 || path.compare(path.size() - 5, 5, ".kedo") != 0) path += ".kedo";
            save(path);
        }
    }
}

void KeyboardApp::updateTitle() {
    std::string name = "Untitled";
    if (!path_.empty()) {
        const auto slash = path_.find_last_of("/\\");
        name = slash == std::string::npos ? path_ : path_.substr(slash + 1);
    }
    // Tiled's row count follows the window, and which layout shows follows
    // playing, so neither is an edit.
    auto plain = [](Document d) {
        d.current = 0;
        for (auto& l : d.layouts) l.layout.tiledRows = 0;
        return d;
    };
    const bool edited = !(plain(documentNow()) == plain(saved_));
    std::string title = name + (edited ? " (edited)" : "") + " - KeyboardEDO";
    if (title != SDL_GetWindowTitle(window_)) SDL_SetWindowTitle(window_, title.c_str());
}

// --- Drawing --------------------------------------------------------------------------

void KeyboardApp::drawKeyboard(ImDrawList* list) {
    const Rect& s = viewport_.screen;
    if (s.w <= 0 || s.h <= 0) return;
    const ImVec2 lo{float(s.x), float(s.y)}, hi{float(s.right()), float(s.bottom())};
    list->AddRectFilled(lo, hi, color(keyboardBackground(settings_.layout.kind, settings_.tuning)));
    list->PushClipRect(lo, hi, true);

    ImFont* font = ImGui::GetFont();
    const float zoom = float(viewport_.zoom);
    // Editing keys, the selected ones on top, so what's being moved shows.
    std::vector<Key> keys = layout_->keysIn(viewport_.visible());
    if (editingKeys())
        std::stable_partition(keys.begin(), keys.end(), [&](const Key& k) { return !selected_.count(k.row); });
    for (const Key& k : keys) {
        // The arpeggiator's step sounds; what its Hold keeps shows latched.
        const bool held = arp_.heldByHold(k.note.id);
        const KeyLook look = keyLook(k, settings_.tuning, settings_.layout.kind, settings_.tileColor,
                                     performer_.isSounding(k.note.id) || arp_.sounding(k.note.id) || held,
                                     performer_.isLatched(k.note.id) || held);
        const ImVec2 c = vec(viewport_.toScreen(k.center));
        const float hw = float(k.halfWidth) * zoom, hh = float(k.halfHeight) * zoom;

        float titleY;
        float titleSize = 13 * zoom, detailSize = 10 * zoom;
        if (k.shape == KeyShape::Diamond) {
            const ImVec2 top{c.x, c.y - hh}, right{c.x + hw, c.y}, bottom{c.x, c.y + hh}, left{c.x - hw, c.y};
            list->AddQuadFilled(top, right, bottom, left, color(look.fill, look.opacity));
            list->AddQuad(top, right, bottom, left, color(look.outline, look.opacity), 1.0f);
            if (editingKeys() && selected_.count(k.row))   // the editor's selection
                list->AddQuad(top, right, bottom, left, IM_COL32(40, 150, 255, 255), 3.0f);
            titleY = c.y - (look.detail.empty() ? titleSize / 2 : titleSize * 0.8f);
        } else {
            const ImVec2 a{c.x - hw, c.y - hh}, b{c.x + hw, c.y + hh};
            const float round = 3 * zoom;
            list->AddRectFilled(a, b, color(look.fill, look.opacity), round, ImDrawFlags_RoundCornersBottom);
            list->AddRect(a, b, color(look.outline, look.opacity), round, ImDrawFlags_RoundCornersBottom, 1.0f);
            titleSize = 10 * zoom;
            titleY = b.y - titleSize - 6 * zoom;
        }

        if (titleSize < 6) continue;   // too small to read
        if (!look.title.empty()) {
            const ImVec2 size = font->CalcTextSizeA(titleSize, FLT_MAX, 0, look.title.c_str());
            list->AddText(font, titleSize, {c.x - size.x / 2, titleY}, color(look.ink, look.opacity), look.title.c_str());
        }
        if (!look.detail.empty()) {
            const ImVec2 size = font->CalcTextSizeA(detailSize, FLT_MAX, 0, look.detail.c_str());
            const Rgb detailInk = k.shape == KeyShape::Diamond && settings_.layout.kind == LayoutKind::Piano
                                      ? Rgb{0.0f, 0.48f, 1.0f}   // KeyEDO's blue frequency
                                      : look.ink;
            list->AddText(font, detailSize, {c.x - size.x / 2, titleY + titleSize + 1},
                          color(detailInk, look.opacity), look.detail.c_str());
        }
        // Its computer key, small, near the bottom corner (piano: above the
        // C's name).
        if (k.computerKey && prefs_.showComputerKeys && detailSize >= 6) {
            const std::string name = computerKeyLabel(k.computerKey);
            const float size = detailSize;
            const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0, name.c_str());
            const float y = k.shape == KeyShape::Diamond ? c.y + hh * 0.62f - extent.y / 2
                                                         : c.y + hh - extent.y - (look.title.empty() ? 6 : 22) * zoom;
            const ImVec2 at{c.x - extent.x / 2, y};
            const Rgb& ink = look.ink;   // a little softer than the labels
            list->AddText(font, size, at, color(Rgb{ink.r * 0.6f + 0.2f, ink.g * 0.6f + 0.2f, ink.b * 0.6f + 0.2f}),
                          name.c_str());
        }
    }
    list->PopClipRect();
}

void KeyboardApp::setBarOpen(bool open) {
    prefs_.barOpen = open;
    if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
}

void KeyboardApp::setPanes(bool panel, bool monitor) {
    if (panel == prefs_.panelOpen && monitor == prefs_.monitorOpen) return;
    prefs_.panelOpen = panel;
    prefs_.monitorOpen = monitor;
    if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
}

// The toolbar: files on the left, the protocol badge in the middle, routing
// and the keyboard's modes on the right (KeyEDO's), and the synopsis under it.
// The help page (docs/decisions.md, "Identity and help"): help/help.html, which
// the build puts beside the app (in the bundle's Resources on the Mac, next to
// the executable elsewhere) with the logo it shows, opened in the default
// browser. A missing page is said in the synopsis line.
void KeyboardApp::openHelp() {
    const char* base = SDL_GetBasePath();
    const std::string dir = base ? base : "";
    for (const std::string& path : {dir + "help/help.html", dir + "../share/keyboardedo/help/help.html"}) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) continue;
        // A file URL, with anything but a path's plain characters escaped.
        std::string url = "file://";
        for (unsigned char c : std::filesystem::weakly_canonical(path, error).string()) {
            if (std::isalnum(c) || std::strchr("/-._~", c)) {
                url += char(c);
            } else {
                char escaped[4];
                std::snprintf(escaped, sizeof escaped, "%%%02X", c);
                url += escaped;
            }
        }
        if (!SDL_OpenURL(url.c_str())) status_ = std::string("Couldn't open the help page: ") + SDL_GetError();
        return;
    }
    status_ = "The help page isn't installed (help/help.html beside the app)";
}

// The Unlikelyware logo and the two-colour "KeyboardEDO" wordmark, as the
// plugins' top strips have (IFSynth's "IFS" + "ynth"), but in the app's
// blues rather than the logo's tan and terracotta: the keyboards' slate
// backgrounds and ImGui's accent are blue, so the warm colours stood apart.
// "Keyboard" is a pale steel blue, "EDO" the accent. The font is Karla, a sans with some character: the plugins'
// Almendra looked out of place as the only serif in the app. The logo is
// loaded on first use, as the renderer exists by then.
void KeyboardApp::drawBrand(float height, float rowY) {
    if (!logoTried_) {
        logoTried_ = true;
        if (SDL_Surface* s = SDL_LoadPNG_IO(SDL_IOFromConstMem(kBrandLogo, kBrandLogoSize), true)) {
            logoAspect_ = float(s->w) / float(s->h);
            logo_ = SDL_CreateTextureFromSurface(SDL_GetRenderer(window_), s);
            if (logo_) SDL_SetTextureScaleMode(logo_, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(s);
        }
        if (!logo_) std::fprintf(stderr, "the toolbar's logo didn't load: %s\n", SDL_GetError());
    }
    if (logo_) {
        ImGui::Image(ImTextureRef(ImTextureID(intptr_t(logo_))), {height * logoAspect_, height});
        // A click opens the website in the default browser, as the plugins'
        // logo does.
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            ImGui::SetTooltip("unlikelyware.com");
        }
        if (ImGui::IsItemClicked() && !SDL_OpenURL("https://www.unlikelyware.com"))
            std::fprintf(stderr, "couldn't open unlikelyware.com: %s\n", SDL_GetError());
        ImGui::SameLine();
    }
    // Both words drawn on one baseline, a letter at a time so the spacing can
    // be tightened (Karla's is generous and it has no narrow width), then the
    // space they take reserved.
    ImGui::PushFont(brandFont_, ImGui::GetStyle().FontSizeBase * 1.5f);
    ImGui::SetCursorPosY(rowY);
    const float size = ImGui::GetFontSize();
    const float tracking = -0.06f * size;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float y = at.y + (height - size) / 2;
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float x = at.x;
    auto word = [&](const char* text, ImU32 colour) {
        for (const char* c = text; *c; ++c) {
            draw->AddText(brandFont_, size, {x, y}, colour, c, c + 1);
            x += ImGui::CalcTextSize(c, c + 1).x + tracking;
        }
    };
    word("Keyboard", ImColor(159, 191, 224));
    word("EDO", ImColor(66, 150, 250));   // ImGui's dark-style accent
    ImGui::Dummy({x - tracking - at.x, height});
    ImGui::PopFont();
    ImGui::SameLine(0, 2 * ImGui::GetStyle().ItemSpacing.x);
    ImGui::SetCursorPosY(rowY);
}

void KeyboardApp::drawToolbar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSizeConstraints({vp->Size.x, 0}, {vp->Size.x, FLT_MAX});
    ImGui::Begin("Toolbar", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
    const float em = ImGui::GetFontSize();
    const float icon = 1.9f * em;
    const float gap = ImGui::GetStyle().ItemSpacing.x;

    const float rowY = ImGui::GetCursorPosY();
    drawBrand(icon, rowY);
    // The performance bar's button on the left, over it, as Settings' is on
    // the right.
    if (iconButton("performance", Icon::Performance, icon, "Performance bar: bend, mod wheel, knobs, arpeggiator",
                   prefs_.barOpen))
        setBarOpen(!prefs_.barOpen);
    ImGui::SameLine();
    if (iconButton("open", Icon::Open, icon, "Open... (Cmd/Ctrl-O)")) showDialog(Dialog::Open);
    ImGui::SameLine();
    if (iconButton("save", Icon::Save, icon, "Save (Cmd/Ctrl-S; Shift for Save As)")) {
        if (path_.empty()) showDialog(Dialog::SaveAs);
        else save(path_);
    }

    // The document's layouts, and the mode: Edit sets them up, Play only
    // chooses among them.
    // Centred on the icons' row (setting only y would start a new line).
    const float centredY = rowY + (icon - ImGui::GetFrameHeight()) / 2;
    ImGui::SameLine();
    ImGui::SetCursorPos({ImGui::GetCursorPosX(), centredY});
    drawLayoutMenu(12 * em);
    ImGui::SameLine();
    ImGui::SetCursorPos({ImGui::GetCursorPosX(), centredY});
    const bool editing = prefs_.editing;
    if (editing) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::Button(editing ? "Editing" : "Edit")) setEditing(!editing);
    if (editing) ImGui::PopStyleColor();
    ImGui::SetItemTooltip(editing ? "Edit mode: set up the document's layouts. Click to play"
                                  : "Play mode: choose among the layouts. Click to set them up");

    // The badge, centred: what the destination gets (KeyEDO's).
    const char* badge = !targets_.available()           ? "no MIDI output"
                        : targets_.destinationOpen()    ? (targets_.destinationProtocol() == uw::MidiProtocol::Midi2
                                                               ? "MIDI 2.0" : "MIDI 1.0")
                        : targets_.publishing()         ? "MIDI 2.0"
                                                        : "not sending";
    const bool sending = targets_.destinationOpen() || targets_.publishing();
    const float badgeWidth = ImGui::CalcTextSize(badge).x + 2 * ImGui::GetStyle().FramePadding.x;
    ImGui::SameLine((vp->Size.x - badgeWidth) / 2);
    ImGui::BeginDisabled(!sending);
    ImGui::SmallButton(badge);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip(sending ? "The protocol notes are sent in" : "Choose where notes go with the antenna button");

    // Routing and modes, from the right.
    const int buttons = 8;
    ImGui::SameLine(vp->Size.x - buttons * (icon + gap) - ImGui::GetStyle().WindowPadding.x + gap);
    if (iconButton("routing", Icon::Routing, icon, "Where notes go", sending)) ImGui::OpenPopup("routing");
    drawRoutingMenu();
    ImGui::SameLine();
    if (iconButton("lock", settings_.locked ? Icon::Locked : Icon::Unlocked, icon,
                   settings_.locked ? "Locked: scrolling and zooming are off (keys still play)"
                                    : "Lock the keyboard: no scrolling or zooming",
                   settings_.locked))
        settings_.locked = !settings_.locked;
    ImGui::SameLine();
    if (iconButton("latch", Icon::Latch, icon,
                   performer_.latchMode() ? "Latch: on. Released notes sustain; turn off to release them"
                                          : "Latch: released notes sustain (as holding Shift)",
                   performer_.latchMode()))
        performer_.setLatchMode(!performer_.latchMode());
    ImGui::SameLine();
    if (iconButton("arp", Icon::Arpeggiator, icon,
                   settings_.arp.enabled ? "Arpeggiator: on. Its settings are in the performance bar"
                                         : "Arpeggiator: step through the notes held",
                   settings_.arp.enabled))
        settings_.arp.enabled = !settings_.arp.enabled;
    ImGui::SameLine();
    if (iconButton("panic", Icon::Panic, icon, "All Notes Off")) {
        releaseAll();
        targets_.allNotesOff();
    }
    ImGui::SameLine();
    if (iconButton("monitor", Icon::Monitor, icon, "MIDI monitor", prefs_.monitorOpen))
        setPanes(prefs_.panelOpen, !prefs_.monitorOpen);
    ImGui::SameLine();
    if (iconButton("help", Icon::Help, icon, "Help, in your browser (F1, or Cmd/Ctrl-?)")) openHelp();
    ImGui::SameLine();
    if (iconButton("configure", Icon::Configure, icon, "Settings", prefs_.panelOpen))
        setPanes(!prefs_.panelOpen, prefs_.monitorOpen);

    // The synopsis, centred under it.
    LiveState live;
    live.latch = performer_.latchMode();
    live.publishing = targets_.publishing();
    if (!doc_.destination.empty()) {
        live.destination = doc_.destination;
        live.destinationFound = false;
        for (const auto& d : targets_.destinations())
            if (d.id == doc_.destination || d.name == doc_.destination) {
                live.destination = d.name;
                live.destinationFound = true;
            }
    }
    const std::string line = synopsis(settings_, live);
    // Its row is set apart from the controls above by a faint band, full
    // width, with a hairline on top (drawn first, so behind the text). The
    // window sizes itself to its content, so the band ends at its bottom.
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        // A little room on both sides of the hairline, so neither the logo
        // above nor the synopsis below sits tight against it.
        const float room = 3.0f * style.FontScaleDpi;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + room);
        const ImVec2 win = ImGui::GetWindowPos();
        const float top = ImGui::GetCursorScreenPos().y - style.ItemSpacing.y / 2;
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + room);
        const float bottom = ImGui::GetCursorScreenPos().y + ImGui::GetTextLineHeight() + style.WindowPadding.y;
        const float right = win.x + ImGui::GetWindowWidth();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled({win.x, top}, {right, bottom}, IM_COL32(255, 255, 255, 10));
        draw->AddLine({win.x, top}, {right, top}, IM_COL32(255, 255, 255, 40), 1.0f);
    }
    ImGui::SetCursorPosX(std::max(0.0f, (vp->Size.x - ImGui::CalcTextSize(line.c_str()).x) / 2));
    ImGui::TextDisabled("%s", line.c_str());
    if (!status_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.45f, 0.4f, 1.0f}, "  %s", status_.c_str());
    }

    barHeight_ = ImGui::GetWindowHeight();
    ImGui::End();
}

// The layout menu on the toolbar: the document's layouts, the showing one
// chosen.
void KeyboardApp::drawLayoutMenu(float width) {
    ImGui::SetNextItemWidth(width);
    const std::string shown = settings_.name.empty() ? "(unnamed)" : settings_.name;
    int chosen = -1;
    if (ImGui::BeginCombo("##layout", shown.c_str())) {
        for (int i = 0; i < int(doc_.layouts.size()); ++i) {
            const std::string& name = i == doc_.current ? settings_.name : doc_.layouts[i].name;
            const std::string label = (name.empty() ? "(unnamed)" : name) + "##" + std::to_string(i);
            if (ImGui::Selectable(label.c_str(), i == doc_.current)) chosen = i;
        }
        ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("The document's layouts (switching releases held notes)");
    if (chosen >= 0) selectLayout(chosen);
}

// The panel's Layouts: the list to choose from, and in Edit mode, managing them.
void KeyboardApp::drawLayoutsSection() {
    if (!ImGui::CollapsingHeader("Layouts", ImGuiTreeNodeFlags_DefaultOpen)) return;
    int chosen = -1;
    for (int i = 0; i < int(doc_.layouts.size()); ++i) {
        const std::string& name = i == doc_.current ? settings_.name : doc_.layouts[i].name;
        const std::string label = (name.empty() ? "(unnamed)" : name) + "##layout" + std::to_string(i);
        if (ImGui::Selectable(label.c_str(), i == doc_.current)) chosen = i;
    }
    if (chosen >= 0) selectLayout(chosen);
    if (!prefs_.editing) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Play mode: choose a layout here or in the toolbar's menu. To add, "
                           "rename or change layouts, click Edit in the toolbar.");
        ImGui::PopStyleColor();
        return;
    }

    char name[128] = {};
    settings_.name.copy(name, sizeof name - 1);
    if (ImGui::InputText("Name", name, sizeof name)) settings_.name = name;
    if (ImGui::Button("Add##layout")) {
        LayoutSetup fresh;
        for (int n = int(doc_.layouts.size()) + 1;; ++n) {
            fresh.name = "Layout " + std::to_string(n);
            if (std::none_of(doc_.layouts.begin(), doc_.layouts.end(),
                             [&](const LayoutSetup& l) { return l.name == fresh.name; }))
                break;
        }
        addLayout(fresh);
    }
    ImGui::SetItemTooltip("A new layout, after this one");
    ImGui::SameLine();
    if (ImGui::Button("Duplicate##layout")) {
        LayoutSetup copy = settings_;
        copy.name += " copy";
        addLayout(copy);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(doc_.layouts.size() < 2);
    if (ImGui::Button("Remove")) removeLayout();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(doc_.current == 0);
    if (ImGui::ArrowButton("##earlier", ImGuiDir_Up)) moveLayout(-1);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Earlier in the list");
    ImGui::SameLine();
    ImGui::BeginDisabled(doc_.current + 1 >= int(doc_.layouts.size()));
    if (ImGui::ArrowButton("##later", ImGuiDir_Down)) moveLayout(1);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Later in the list");
}

// The custom layout editor's panel: snapping, the edits, and the inspector for
// the selected keys (decisions.md, "Custom layout").
void KeyboardApp::drawKeyInspector() {
    if (!ImGui::CollapsingHeader("Keys", ImGuiTreeNodeFlags_DefaultOpen)) return;
    for (auto it = selected_.begin(); it != selected_.end();)   // after undo, deletion
        it = *it < int(keys().size()) ? std::next(it) : selected_.erase(it);
    // Each edit's widget saves the keys for undo as it's first touched: as
    // they were before this frame, as the widget may already have changed
    // them in the frame it was touched.
    const EditState before{keys(), selected_};
    auto undoable = [&] {
        if (ImGui::IsItemActivated()) pushUndo(before);
    };

    ImGui::Checkbox("Snap to grid", &snap_);
    ImGui::SameLine();
    ImGui::BeginDisabled(!snap_);
    ImGui::Checkbox("Half steps", &halfGrid_);
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Snap to half the grid, between keys");

    // Labels of their own: the Layouts section above has Add and Duplicate,
    // and ImGui takes a widget's label as its identity.
    ImGui::BeginDisabled(undo_.empty());
    if (ImGui::Button("Undo")) undo();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Cmd/Ctrl-Z");
    ImGui::SameLine();
    ImGui::BeginDisabled(redo_.empty());
    if (ImGui::Button("Redo")) redo();
    ImGui::EndDisabled();
    ImGui::SetItemTooltip("Cmd/Ctrl-Shift-Z");
    if (ImGui::Button("Add key")) {
        const Rect v = viewport_.visible();
        addKeyAt({v.x + v.w / 2, v.y + v.h / 2});
    }
    ImGui::SetItemTooltip("A key in the middle of the view (or double-click empty space)");
    ImGui::SameLine();
    ImGui::BeginDisabled(selected_.empty());
    if (ImGui::Button("Duplicate##keys")) duplicateSelected();
    ImGui::SetItemTooltip("The selected keys (Cmd/Ctrl-D)");
    ImGui::SameLine();
    if (ImGui::Button("Delete##keys")) deleteSelected();
    ImGui::SetItemTooltip("The selected keys (Del)");
    ImGui::EndDisabled();

    if (selected_.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Click a key to select it, Shift-click for more, and drag to move them; "
                           "arrows nudge (Shift: further). Del deletes, Cmd/Ctrl-A selects all.");
        ImGui::PopStyleColor();
        return;
    }
    const Tuning& tuning = settings_.tuning;
    const int primary = *selected_.begin();
    const bool several = selected_.size() > 1;
    auto each = [&](auto&& change) {
        for (const int i : selected_) change(keys()[i]);
    };

    // Pitch: one key's EDO, octave and degree; several keys, transposed.
    ImGui::SeparatorText(several ? "Pitch (each key)" : "Pitch");
    if (!several) {
        CustomKey& k = keys()[primary];
        int edo = k.edo, octave = k.octave, degree = k.degree;
        bool changed = false;
        changed |= ImGui::InputInt("EDO##key", &edo);
        undoable();
        changed |= ImGui::InputInt("Octave##key", &octave);
        undoable();
        changed |= ImGui::InputInt("Degree##key", &degree);
        undoable();
        ImGui::SetItemTooltip("Steps of the EDO above the origin note (%s) in the octave",
                              pitchClassName(tuning.originPitchClass));
        if (changed) {
            edo = std::clamp(edo, 1, Tuning::kMaxEdo);
            while (degree >= edo) degree -= edo, ++octave;
            while (degree < 0) degree += edo, --octave;
            k.edo = edo;
            k.octave = std::clamp(octave, -1, 9);
            k.degree = degree;
            pitchChanged(primary);
            audition(primary);
        }
    } else {
        auto transpose = [&](int steps, int octaves) {
            pushUndo();
            for (const int i : selected_) {
                CustomKey& k = keys()[i];
                k.octave += octaves;
                k.degree += steps;
                while (k.degree >= k.edo) k.degree -= k.edo, ++k.octave;
                while (k.degree < 0) k.degree += k.edo, --k.octave;
                pitchChanged(i);
            }
            audition(primary);
        };
        if (ImGui::Button("- step")) transpose(-1, 0);
        ImGui::SameLine();
        if (ImGui::Button("+ step")) transpose(1, 0);
        ImGui::SameLine();
        if (ImGui::Button("- octave")) transpose(0, -1);
        ImGui::SameLine();
        if (ImGui::Button("+ octave")) transpose(0, 1);
    }
    {
        const CustomKey& k = keys()[primary];
        const double pitch = customPitch(k, tuning);
        const StepLabel l = labelOfPitch(pitch);
        ImGui::TextDisabled("%s%s %+.0f cents, %.2f Hz", several ? "First: " : "", noteNameOf(pitch).c_str(),
                            l.cents, 440.0 * std::pow(2.0, (pitch - 69) / 12.0));
    }

    // The note number: suggested (nearest free) unless chosen; one key's.
    ImGui::SeparatorText("MIDI");
    if (!several) {
        CustomKey& k = keys()[primary];
        int number = k.number;
        if (ImGui::InputInt("Note number", &number)) {
            k.number = std::clamp(number, 0, 127);
            k.numberChosen = true;
            audition(primary);
        }
        undoable();
        if (k.numberChosen) {
            if (ImGui::Button("Suggest")) {
                pushUndo();
                k.numberChosen = false;
                k.number = suggestNumber(keys(), primary, tuning);
                audition(primary);
            }
            ImGui::SetItemTooltip("The nearest free number to the key's pitch");
        } else {
            ImGui::TextDisabled("Suggested: the nearest free number");
        }
        // Another key's note on this number, at another pitch: the receiver
        // hears one note (decisions.md: duplicates warn).
        const double pitch = customPitch(k, tuning);
        for (int i = 0; i < int(keys().size()); ++i) {
            const CustomKey& o = keys()[i];
            if (i != primary && o.number == k.number && o.group == k.group && o.channel == k.channel &&
                std::abs(customPitch(o, tuning) - pitch) > 1e-9) {
                ImGui::TextColored({1.0f, 0.6f, 0.3f, 1.0f}, "Shared with a key of another pitch:");
                ImGui::TextColored({1.0f, 0.6f, 0.3f, 1.0f}, "they'll sound as one note.");
                break;
            }
        }
    }
    // Its computer key: pressed to assign (taken from any other key).
    if (!several) {
        CustomKey& k = keys()[primary];
        const std::string shown = k.computerKey ? computerKeyLabel(k.computerKey) : "none";
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Computer key: %s", assigning_ ? "press one..." : shown.c_str());
        ImGui::SameLine();
        if (ImGui::Button(assigning_ ? "Cancel##ck" : "Assign...")) assigning_ = !assigning_;
        ImGui::SetItemTooltip("Then press a letter, digit or punctuation key (Escape cancels).\n"
                              "A key already on another note key moves here");
        if (k.computerKey) {
            ImGui::SameLine();
            if (ImGui::Button("Clear##ck")) {
                pushUndo();
                k.computerKey = 0;
            }
        }
    } else {
        assigning_ = false;
    }
    // Group and channel: the layout's (the MIDI section), or the key's own.
    auto route = [&](const char* label, std::optional<std::uint8_t> CustomKey::*field) {
        const auto& first = keys()[primary].*field;
        const std::string shown = first ? std::to_string(*first + 1) : "Layout's";
        if (ImGui::BeginCombo(label, shown.c_str())) {
            std::optional<std::optional<std::uint8_t>> chosen;
            if (ImGui::Selectable("Layout's", !first)) chosen = std::optional<std::uint8_t>{};
            for (int n = 0; n < 16; ++n)
                if (ImGui::Selectable(std::to_string(n + 1).c_str(), first && *first == n))
                    chosen = std::optional<std::uint8_t>{std::uint8_t(n)};
            ImGui::EndCombo();
            if (chosen) {
                pushUndo();
                each([&](CustomKey& k) { k.*field = *chosen; });
                for (const int i : selected_)   // a new route: free numbers there
                    if (!keys()[i].numberChosen) keys()[i].number = suggestNumber(keys(), i, tuning);
                audition(primary);
            }
        }
    };
    route("Group##key", &CustomKey::group);
    route("Channel##key", &CustomKey::channel);

    // The look.
    ImGui::SeparatorText("Look");
    {
        CustomKey& k = keys()[primary];
        static const char* const kShades[] = {"White", "Black"};
        int shade = int(k.shade);
        if (ImGui::Combo("Shade", &shade, kShades, 2)) each([&](CustomKey& o) { o.shade = KeyShade(shade); });
        undoable();
        bool own = k.color.has_value();
        if (ImGui::Checkbox("Own colour", &own))
            each([&](CustomKey& o) { o.color = own ? std::optional<std::uint32_t>(k.color.value_or(0xb0d0ff)) : std::nullopt; });
        undoable();
        if (k.color) {
            ImGui::SameLine();
            float rgb[3] = {((*k.color >> 16) & 0xFF) / 255.0f, ((*k.color >> 8) & 0xFF) / 255.0f,
                            (*k.color & 0xFF) / 255.0f};
            if (ImGui::ColorEdit3("##colour", rgb, ImGuiColorEditFlags_NoInputs)) {
                auto byte = [](float v) { return std::uint32_t(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };
                const std::uint32_t packed = byte(rgb[0]) << 16 | byte(rgb[1]) << 8 | byte(rgb[2]);
                each([&](CustomKey& o) { o.color = packed; });
            }
            undoable();
        }
        // Width and height: 50, 100 (the standard diamond's, the default), 150
        // or 200%. A size from elsewhere shows as the nearest.
        static const char* const kSizes[] = {"50%", "100%", "150%", "200%"};
        auto sizeIndex = [](double half) {
            const double percent = 100 * (half > 0 ? half : kHalfDiag) / kHalfDiag;
            return std::clamp(int(std::lround(percent / 50)) - 1, 0, 3);
        };
        int width = sizeIndex(k.halfWidth), height = sizeIndex(k.halfHeight);
        if (ImGui::Combo("Width", &width, kSizes, 4)) {
            pushUndo(before);
            each([&](CustomKey& o) { o.halfWidth = (width + 1) * 0.5 * kHalfDiag; });
        }
        if (ImGui::Combo("Height", &height, kSizes, 4)) {
            pushUndo(before);
            each([&](CustomKey& o) { o.halfHeight = (height + 1) * 0.5 * kHalfDiag; });
        }
        // The label and the line under it: typed, or from the pitch (and
        // following it). Typing over one from the pitch makes it typed.
        static const char* const kStyles[] = {"Text", "Name and octave", "Name", "Degree", "Frequency"};
        auto labelLine = [&](const char* name, std::string CustomKey::*text, LabelStyle CustomKey::*style) {
            ImGui::PushID(name);
            int chosen = int(k.*style);
            if (ImGui::Combo(name, &chosen, kStyles, 5)) {
                pushUndo(before);
                each([&](CustomKey& o) {
                    if (LabelStyle(chosen) == LabelStyle::Text && o.*style != LabelStyle::Text)
                        o.*text = labelText(o, o.*style, o.*text, tuning);   // keep what it showed
                    o.*style = LabelStyle(chosen);
                });
            }
            char buf[64] = {};
            labelText(k, k.*style, k.*text, tuning).copy(buf, sizeof buf - 1);
            if (ImGui::InputText("##text", buf, sizeof buf))
                each([&](CustomKey& o) {
                    o.*text = buf;
                    o.*style = LabelStyle::Text;
                });
            undoable();
            ImGui::PopID();
        };
        labelLine("Label", &CustomKey::label, &CustomKey::labelStyle);
        labelLine("Under it", &CustomKey::sublabel, &CustomKey::sublabelStyle);
    }
}

// KeyEDO's routing menu: Send to a destination, and Publish our own source.
void KeyboardApp::drawRoutingMenu() {
    if (!ImGui::BeginPopup("routing")) return;
    if (!targets_.available()) {
        ImGui::TextDisabled("No MIDI output (the system's MIDI couldn't be reached):");
        ImGui::TextDisabled("the monitor shows what would be sent.");
        ImGui::EndPopup();
        return;
    }
    ImGui::SeparatorText("Send to");
    if (ImGui::MenuItem("None", nullptr, doc_.destination.empty())) doc_.destination.clear();
    bool found = doc_.destination.empty();
    for (const auto& d : targets_.destinations()) {
        const bool chosen = d.id == doc_.destination || d.name == doc_.destination;
        found = found || chosen;
        const std::string label = d.name + "##" + d.id;
        if (ImGui::MenuItem(label.c_str(), d.protocol == uw::MidiProtocol::Midi2 ? "2.0" : "1.0", chosen))
            doc_.destination = d.id;
    }
    if (!found) {
        const std::string missing = doc_.destination + " (not found)";
        ImGui::MenuItem(missing.c_str(), nullptr, true, false);
    }

    ImGui::SeparatorText("Publish");
    bool publish = targets_.publishing();
    if (ImGui::MenuItem("Publish as \"KeyboardEDO\"", nullptr, &publish)) setPublish(publish);
    ImGui::SetItemTooltip(
        "Offers a MIDI source named \"%s\" that other apps can receive from: in\n"
        "REAPER, it's a MIDI input device. It keeps the same identity from one\n"
        "launch to the next, and this setting is remembered. An app that was\n"
        "already running when it appeared may need to rescan its MIDI devices\n"
        "(REAPER: Preferences > MIDI Inputs > Reset all MIDI devices).",
        MidiTargets::kPublishedName);
    ImGui::EndPopup();
}

// The settings panel down the right: KeyEDO's Configure sheet, showing only
// what the current layout uses. Returns its width.
float KeyboardApp::drawPanel() {
    static const char* const kNotes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char* const kLayouts[] = {"Piano", "Tiled", "Custom"};
    static const char* const kTileColors[] = {"Off", "Note", "Octave"};
    static const char* const kDrags[] = {"Gliss", "Fixed"};
    static const char* const kHorizontal[] = {"Off", "Pitch Bend"};
    static const char* const kVertical[] = {"Off", "Controller", "Pressure", "Pitch Bend"};
    static const char* const kScopes[] = {"Per note", "Channel"};
    static const char* const kPressureScopes[] = {"Poly", "Channel"};

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float em = ImGui::GetFontSize();
    const float width = 22 * em;
    ImGui::SetNextWindowPos({vp->Pos.x + vp->Size.x - width, vp->Pos.y + barHeight_});
    ImGui::SetNextWindowSize({width, vp->Size.y - barHeight_});
    bool open = true;
    ImGui::Begin("Settings", &open,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (!open) setPanes(false, prefs_.monitorOpen);
    ImGui::PushItemWidth(-9 * em);   // room for the labels

    auto& t = settings_.tuning;
    auto& l = settings_.layout;
    auto& p = settings_.play;
    auto& o = settings_.output;

    drawLayoutsSection();

    // The showing layout's settings: Edit mode only, so nothing changes by
    // accident while playing.
    if (prefs_.editing && ImGui::CollapsingHeader("Tuning", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::InputInt("EDO steps", &t.edo)) t.edo = std::clamp(t.edo, Tuning::kMinEdo, Tuning::kMaxEdo);
        ImGui::Combo("Origin note", &t.originPitchClass, kNotes, 12);
        if (ImGui::InputInt("Origin octave", &t.originOctave)) t.originOctave = std::clamp(t.originOctave, -1, 7);
        ImGui::TextDisabled("Origin: %s%d, %.2f Hz", kNotes[t.originPitchClass], t.originOctave, t.originFrequency());
    }

    // The combo inside has the same label: "##kind" gives it its own ID.
    if (prefs_.editing && ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        enumCombo("Layout##kind", l.kind, kLayouts, ImGui::CalcItemWidth());
        if (l.kind == LayoutKind::Piano) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", t.edo == 12 ? "A standard piano." : "Diamonds: each piano key's steps, stacked.");
            ImGui::PopStyleColor();
        }
        if (l.kind == LayoutKind::Tiled) {
            const auto presets = generatorPresets(t.edo);
            if (l.generatorIndex >= int(presets.size())) l.generatorIndex = 0;
            if (ImGui::BeginCombo("Generator", presets[l.generatorIndex].name.c_str())) {
                for (int i = 0; i < int(presets.size()); ++i) {
                    const auto& g = presets[i];
                    if (ImGui::Selectable(g.name.c_str(), i == l.generatorIndex)) l.generatorIndex = i;
                    if (g.octaveColumns)
                        ImGui::SetItemTooltip("each column one note in every octave, two octaves across");
                    else
                        ImGui::SetItemTooltip("right %+d, up-right %+d, up-left %+d steps", g.right, g.upRight, g.upLeft());
                }
                ImGui::EndCombo();
            }
            const auto& g = presets[l.generatorIndex];
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (g.octaveColumns)
                ImGui::TextWrapped("Each column one note in every octave; right +2, up-right +1.");
            else
                ImGui::TextWrapped("Steps: right %+d, up-right %+d, up-left %+d", g.right, g.upRight, g.upLeft());
            ImGui::PopStyleColor();
            enumCombo("Tile colour", settings_.tileColor, kTileColors, ImGui::CalcItemWidth());
        }
        if (l.kind == LayoutKind::Custom) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Keys placed one by one, each with its own pitch (EDO, octave, degree "
                               "from the origin note) and note number. The EDO above is for keys added.");
            ImGui::PopStyleColor();
        } else {
            if (ImGui::Button("Convert to custom")) {
                l.customKeys = convertToCustom(*layout_, t, l.kind, settings_.tileColor);
                l.kind = LayoutKind::Custom;
            }
            ImGui::SetItemTooltip("Make this layout's keys editable: each keeps its place, look,\n"
                                  "pitch and note number, so it plays just as it does now");
        }
    }

    if (editingKeys()) drawKeyInspector();

    if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Reset View")) rebuild(true);
        ImGui::SameLine();
        ImGui::BeginDisabled(settings_.locked);
        if (ImGui::Button("-##zoom")) zoomBy(1 / kZoomStep);
        ImGui::SetItemTooltip("Zoom out (Cmd/Ctrl -)");
        ImGui::SameLine();
        if (ImGui::Button("+##zoom")) zoomBy(kZoomStep);
        ImGui::SetItemTooltip("Zoom in (Cmd/Ctrl =)");
        ImGui::SameLine();
        ImGui::Text("%d%%", int(std::lround(viewport_.zoom * 100)));
        ImGui::EndDisabled();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Wheel scrolls, Shift-wheel sideways; Ctrl-wheel or pinch zooms.");
        ImGui::PopStyleColor();
        if (ImGui::Checkbox("Computer keys", &prefs_.showComputerKeys) && !prefsPath_.empty())
            savePreferences(prefsPath_, prefs_);
        ImGui::SetItemTooltip("Show each key's computer key. Shift latches; Ctrl with the mouse\n"
                              "sends expression (Playing) for the notes they hold");
    }

    if (prefs_.editing && ImGui::CollapsingHeader("Playing", ImGuiTreeNodeFlags_DefaultOpen)) {
        enumCombo("Drag", p.drag, kDrags, ImGui::CalcItemWidth());
        if (p.drag == DragBehavior::Gliss) {
            ImGui::Checkbox("Glide (Pitch 7.25)", &p.glide);
            ImGui::SetItemTooltip("Slide one note's pitch from key to key instead of striking each (MIDI 2.0)");
        }
        // What a Fixed drag sends, and in either mode, moving the mouse with
        // Ctrl held while computer keys hold notes (hover expression).
        {
            enumCombo("Horizontal", p.horizontal, kHorizontal, ImGui::CalcItemWidth());
            ImGui::SetItemTooltip("What dragging sends (Fixed), and moving the mouse with Ctrl held\n"
                                  "while computer keys hold notes");
            enumCombo("Vertical", p.vertical, kVertical, ImGui::CalcItemWidth());
            if (p.vertical == VerticalControl::Controller &&
                ImGui::InputInt("Controller", &p.verticalController))
                p.verticalController = std::clamp(p.verticalController, 0, 127);
            // Each message in use: per note or for the channel.
            const bool bends = p.horizontal == HorizontalControl::PitchBend || p.vertical == VerticalControl::PitchBend;
            const char* const midi1Only = "Channel only while expression is sent as MIDI 1.0 (MIDI section):\n"
                                          "MIDI 1.0 has no per-note form, so the first finger down drives it";
            if (bends) {
                ExpressionScope shown = o.midi1Expression ? ExpressionScope::Channel : p.bendScope;
                ImGui::BeginDisabled(o.midi1Expression);
                if (enumCombo("Bend scope", shown, kScopes, ImGui::CalcItemWidth())) p.bendScope = shown;
                ImGui::EndDisabled();
                ImGui::SetItemTooltip("%s", o.midi1Expression ? midi1Only
                                                              : "Per note: each finger bends only its own note (MIDI 2.0\n"
                                                                "per-note pitch bend). Channel: the first finger bends all");
            }
            if (p.vertical == VerticalControl::Controller) {
                ExpressionScope shown = o.midi1Expression ? ExpressionScope::Channel : p.controllerScope;
                ImGui::BeginDisabled(o.midi1Expression);
                if (enumCombo("Controller scope", shown, kScopes, ImGui::CalcItemWidth())) p.controllerScope = shown;
                ImGui::EndDisabled();
                ImGui::SetItemTooltip("%s", o.midi1Expression ? midi1Only
                                                              : "Per note: a registered per-note controller (MIDI 2.0).\n"
                                                                "Channel: a CC, from the first finger");
            }
            if (p.vertical == VerticalControl::Pressure) {
                enumCombo("Pressure scope", p.pressureScope, kPressureScopes, ImGui::CalcItemWidth());
                ImGui::SetItemTooltip("Poly: each finger's own note (polyphonic key pressure).\n"
                                      "Channel: channel pressure, from the first finger");
            }
        }
        ImGui::Checkbox("Strike velocity", &p.strikeVelocity);
        ImGui::SetItemTooltip("Louder toward the top of a key");
        if (p.strikeVelocity) ImGui::SliderInt("Minimum", &p.strikeVelocityMin, 1, 126);
    }

    if (prefs_.editing && ImGui::CollapsingHeader("MIDI", ImGuiTreeNodeFlags_DefaultOpen)) {
        int group = o.group + 1, channel = o.channel + 1;
        if (ImGui::InputInt("Group", &group)) o.group = std::uint8_t(std::clamp(group, 1, 16) - 1);
        if (ImGui::InputInt("Channel", &channel)) o.channel = std::uint8_t(std::clamp(channel, 1, 16) - 1);
        float bend = float(o.bendRange), perNoteBend = float(o.perNoteBendRange);
        if (ImGui::InputFloat("Bend range", &bend, 1, 12, "%.0f st")) o.bendRange = std::clamp(double(bend), 0.0, 96.0);
        if (ImGui::InputFloat("Per-note bend", &perNoteBend, 1, 12, "%.0f st"))
            o.perNoteBendRange = std::clamp(double(perNoteBend), 0.0, 96.0);
        ImGui::Checkbox("Expression as MIDI 1.0", &o.midi1Expression);
        ImGui::SetItemTooltip(
            "Send pitch bend, controllers and pressure as MIDI 1.0 messages, even to\n"
            "a MIDI 2.0 receiver, as KeyEDO did: Logic Pro and other DAWs mishandle\n"
            "MIDI 2.0 bend and controllers. Notes stay MIDI 2.0, with exact pitch.\n"
            "Turn off for receivers that handle MIDI 2.0 fully (StageHand, Umpire),\n"
            "to get 32-bit and per-note expression.");
        ImGui::Checkbox("Per-Note Management", &o.perNoteManagement);
        ImGui::SetItemTooltip("Reset a note's per-note bend and controllers when it's struck again (MIDI 2.0)");
        ImGui::TextDisabled("Where notes go: the antenna button.");
    }

    if (ImGui::CollapsingHeader("File", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Open...")) showDialog(Dialog::Open);
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (path_.empty()) showDialog(Dialog::SaveAs);
            else save(path_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save As...")) showDialog(Dialog::SaveAs);
        if (ImGui::Button("Import KeyEDO...")) showDialog(Dialog::Import);
        ImGui::SetItemTooltip("A KeyEDO document, as a new document of one layout");
        ImGui::TextWrapped("%s", path_.empty() ? "Not saved yet." : path_.c_str());
    }

    ImGui::PopItemWidth();
    ImGui::End();
    return width;
}

// --- The performance bar ----------------------------------------------------------------

namespace {

// How far a drag moves a fader or knob from end to end, in points.
constexpr float kKnobTravel = 160;

// A vertical fader, `size` across and high, from `lo` (bottom) to `hi`. A
// drag moves it by as much as the pointer moves, wherever it's grabbed, so a
// click doesn't make it jump. `spring`: back to `rest` when let go (a bend
// wheel). `rest` is marked on the track. Returns true when it changed.
bool fader(const char* id, float& value, float lo, float hi, float rest, ImVec2 size, bool spring) {
    const float before = value;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const float handle = size.x * 0.55f;
    if (ImGui::IsItemActive())
        value = std::clamp(value - ImGui::GetIO().MouseDelta.y / (size.y - handle) * (hi - lo), lo, hi);
    else if (spring)
        value = rest;

    ImDrawList* d = ImGui::GetWindowDrawList();
    const float cx = at.x + size.x / 2;
    const float travel = size.y - handle;
    auto yFor = [&](float v) { return at.y + handle / 2 + (1 - (v - lo) / (hi - lo)) * travel; };
    d->AddRectFilled(at, {at.x + size.x, at.y + size.y}, ImGui::GetColorU32(ImGuiCol_FrameBg), size.x * 0.2f);
    d->AddLine({cx, at.y + handle / 2}, {cx, at.y + size.y - handle / 2}, ImGui::GetColorU32(ImGuiCol_Border), 3);
    d->AddLine({at.x + 3, yFor(rest)}, {at.x + size.x - 3, yFor(rest)}, ImGui::GetColorU32(ImGuiCol_TextDisabled), 1);
    const float y = yFor(value);
    const ImU32 fill = ImGui::GetColorU32(ImGui::IsItemActive()    ? ImGuiCol_SliderGrabActive
                                          : ImGui::IsItemHovered() ? ImGuiCol_ButtonHovered
                                                                   : ImGuiCol_SliderGrab);
    d->AddRectFilled({at.x + 2, y - handle / 2}, {at.x + size.x - 2, y + handle / 2}, fill, 3);
    d->AddLine({at.x + 5, y}, {at.x + size.x - 5, y}, ImGui::GetColorU32(ImGuiCol_Text), 1.5f);
    return value != before;
}

// A knob, `diameter` across, 0..1: dragging up turns it up, wherever the
// pointer goes; a double click sets it to `start`. Returns true when it
// changed.
bool knob(const char* id, float& value, float start, float diameter) {
    const float before = value;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, {diameter, diameter});
    if (ImGui::IsItemActive()) value = std::clamp(value - ImGui::GetIO().MouseDelta.y / kKnobTravel, 0.0f, 1.0f);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) value = start;

    // A 270-degree sweep, from bottom left round to bottom right.
    constexpr float kFrom = 0.75f * 3.14159265f, kSweep = 1.5f * 3.14159265f;
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 c{at.x + diameter / 2, at.y + diameter / 2};
    const float r = diameter / 2 - 3;
    d->PathArcTo(c, r, kFrom, kFrom + kSweep, 32);
    d->PathStroke(ImGui::GetColorU32(ImGuiCol_FrameBg), ImDrawFlags_None, 4.0f);
    if (value > 0) {
        d->PathArcTo(c, r, kFrom, kFrom + kSweep * value, 32);
        d->PathStroke(ImGui::GetColorU32(ImGuiCol_SliderGrabActive), ImDrawFlags_None, 4.0f);
    }
    const ImU32 face = ImGui::GetColorU32(ImGui::IsItemActive()    ? ImGuiCol_FrameBgActive
                                          : ImGui::IsItemHovered() ? ImGuiCol_FrameBgHovered
                                                                   : ImGuiCol_FrameBg);
    d->AddCircleFilled(c, r - 5, face, 32);
    const float angle = kFrom + kSweep * value;
    d->AddLine({c.x + (r - 14) * std::cos(angle) * 0.3f, c.y + (r - 14) * std::sin(angle) * 0.3f},
               {c.x + (r - 6) * std::cos(angle), c.y + (r - 6) * std::sin(angle)}, ImGui::GetColorU32(ImGuiCol_Text),
               2.5f);
    return value != before;
}

// Text centred in the next `width` points of the line.
void centredText(const char* text, float width, bool disabled = false) {
    const float x = ImGui::GetCursorPosX();
    ImGui::SetCursorPosX(x + std::max(0.0f, (width - ImGui::CalcTextSize(text).x) / 2));
    if (disabled) ImGui::TextDisabled("%s", text);
    else ImGui::TextUnformatted(text);
}

} // namespace

// The performance bar down the left (decisions.md, "Performance bar"): the
// bend fader and mod wheel, four knobs in two columns, and the arpeggiator.
// The same in every layout and in both modes; knobs are assigned in Edit
// mode. Returns its width.
float KeyboardApp::drawBar() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float em = ImGui::GetFontSize();
    const float width = 15 * em;
    ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y + barHeight_});
    ImGui::SetNextWindowSize({width, vp->Size.y - barHeight_});
    bool open = true;
    ImGui::Begin("Performance", &open,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (!open) setBarOpen(false);
    const float inner = ImGui::GetContentRegionAvail().x;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float column = (inner - gap) / 2;

    // Bend and mod wheel side by side, each centred in its half.
    const ImVec2 faderSize{2.6f * em, 10 * em};
    const float faderIndent = (column - faderSize.x) / 2;
    const float top = ImGui::GetCursorPosY();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + faderIndent);
    float bend = float(controls_.bend());
    if (fader("##bend", bend, -1, 1, 0, faderSize, true)) controls_.setBend(bend);
    ImGui::SetItemTooltip("Pitch bend, over the bend range (%g semitones); springs back", settings_.output.bendRange);
    ImGui::SameLine(0, 0);
    ImGui::SetCursorPos({ImGui::GetStyle().WindowPadding.x + column + gap + faderIndent, top});
    float wheel = float(controls_.wheel());
    if (fader("##wheel", wheel, 0, 1, 0, faderSize, false)) controls_.setWheel(wheel);
    ImGui::SetItemTooltip("Mod wheel (CC 1): %d", int(std::lround(wheel * 127)));
    centredText("Bend", column);
    ImGui::SameLine(ImGui::GetStyle().WindowPadding.x + column + gap);
    centredText("Mod", column);

    // The knobs, two by two: a click on a name assigns it, in Edit mode.
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    const float diameter = std::min(column, 4.2f * em);
    for (int row = 0; row < kKnobCount / 2; ++row) {
        const float rowTop = ImGui::GetCursorPosY();
        for (int col = 0; col < 2; ++col) {
            const int i = row * 2 + col;
            const KnobAssignment& k = controls_.knobs()[i];
            const float x = ImGui::GetStyle().WindowPadding.x + col * (column + gap);
            ImGui::PushID(i);
            ImGui::SetCursorPos({x + (column - diameter) / 2, rowTop});
            float value = float(controls_.knob(i));
            if (knob("##knob", value, float(knobStart(k)), diameter)) controls_.setKnob(i, value);
            // A right-click assigns it, in either mode: it can't happen by
            // accident while playing.
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("assign");
            const std::string what = k.pressure ? "channel pressure" : "CC " + std::to_string(k.controller);
            ImGui::SetItemTooltip("%s: %s, %d\nDrag up or down; double-click: back to %d\n"
                                  "Right-click: choose what it sends",
                                  knobName(k).c_str(), what.c_str(), int(std::lround(value * 127)),
                                  int(std::lround(knobStart(k) * 127)));
            const std::string name = knobName(k);
            const float nameY = rowTop + diameter + ImGui::GetStyle().ItemSpacing.y;
            ImGui::SetCursorPos({x, nameY});
            if (prefs_.editing) {
                // The name is a button that opens the assignment.
                ImGui::PushStyleColor(ImGuiCol_Button, 0);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
                if (ImGui::Button(name.c_str(), {column, 0})) ImGui::OpenPopup("assign");
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                ImGui::SetItemTooltip("Click to choose what this knob sends");
            } else {
                centredText(name.c_str(), column);
            }
            drawKnobAssignment(i);
            ImGui::SetCursorPos({x, nameY + ImGui::GetTextLineHeightWithSpacing()});
            centredText(std::to_string(int(std::lround(value * 127))).c_str(), column, true);
            ImGui::PopID();
        }
        // Under both columns.
        ImGui::SetCursorPos({ImGui::GetStyle().WindowPadding.x,
                             rowTop + diameter + 2 * ImGui::GetTextLineHeightWithSpacing() +
                                 2 * ImGui::GetStyle().ItemSpacing.y});
        ImGui::Dummy({0, 0});
    }

    ImGui::Spacing();
    drawArpeggiator();
    ImGui::End();
    return width;
}

// What knob `index` sends: a CC or channel pressure, and its label. Saved
// with the document. Opened by a right-click on the knob, or in Edit mode a
// click on its name.
void KeyboardApp::drawKnobAssignment(int index) {
    if (!ImGui::BeginPopup("assign")) return;
    const float em = ImGui::GetFontSize();
    KnobAssignment& k = doc_.knobs[index];
    ImGui::PushItemWidth(12 * em);
    static const char* const kKinds[] = {"Controller (CC)", "Channel pressure"};
    int kind = k.pressure ? 1 : 0;
    if (ImGui::Combo("Sends", &kind, kKinds, 2)) k.pressure = kind == 1;
    if (!k.pressure && ImGui::InputInt("CC", &k.controller)) k.controller = std::clamp(k.controller, 0, 127);
    char label[64];
    std::snprintf(label, sizeof label, "%s", k.label.c_str());
    if (ImGui::InputTextWithHint("Label", knobName({.pressure = k.pressure, .controller = k.controller}).c_str(), label,
                                 sizeof label))
        k.label = label;
    ImGui::PopItemWidth();
    if (ImGui::Button("Default")) k = defaultKnobs()[index];
    ImGui::SetItemTooltip("Back to %s", knobName(defaultKnobs()[index]).c_str());
    ImGui::EndPopup();
}

// The arpeggiator's settings, in Play mode too: pattern, division and tempo
// are changed while playing (decisions.md, "Arpeggiator").
void KeyboardApp::drawArpeggiator() {
    static const char* const kPatterns[] = {"Up", "Down", "Up-Down", "Down-Up", "As Played", "Random"};
    static const char* const kDivisions[] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
                                             "1/2T", "1/4T", "1/8T", "1/16T"};
    static const char* const kVelocities[] = {"As played", "Fixed", "Random"};
    if (!ImGui::CollapsingHeader("Arpeggiator", ImGuiTreeNodeFlags_DefaultOpen)) return;
    const float em = ImGui::GetFontSize();
    ImGui::PushItemWidth(-5.5f * em);   // room for the labels
    ArpConfig& a = settings_.arp;
    ImGui::Checkbox("On##arp", &a.enabled);
    ImGui::SetItemTooltip("Step through the notes held, one at a time (also the toolbar button)");
    ImGui::SameLine();
    ImGui::Checkbox("Hold", &a.hold);
    ImGui::SetItemTooltip("Released notes keep playing, and new ones add to them.\n"
                          "All Notes Off or Escape clears them; so does turning Hold off");
    enumCombo("Pattern", a.pattern, kPatterns, ImGui::CalcItemWidth());
    enumCombo("Division", a.division, kDivisions, ImGui::CalcItemWidth());
    ImGui::SetItemTooltip("A step's length; T: triplets");
    float bpm = float(a.bpm);
    if (ImGui::DragFloat("Tempo", &bpm, 0.5f, 1, 999, "%.1f bpm", ImGuiSliderFlags_AlwaysClamp)) a.bpm = bpm;
    int gate = int(std::lround(a.gate * 100));
    if (ImGui::SliderInt("Gate", &gate, 1, 100, "%d%%")) a.gate = gate / 100.0;
    ImGui::SetItemTooltip("How much of its step each note sounds");
    ImGui::SliderInt("Octaves", &a.octaves, 1, 4);
    ImGui::SetItemTooltip("The pattern, then again an octave higher for each octave more");
    enumCombo("Velocity##arp", a.velocity, kVelocities, ImGui::CalcItemWidth());
    if (a.velocity == ArpVelocity::Fixed) {
        ImGui::SliderInt("Fixed##velocity", &a.fixedVelocity, 1, 127);
        ImGui::SetItemTooltip("The velocity every step plays at");
    }
    ImGui::PopItemWidth();
}

// The MIDI monitor, under the keyboard, from `x`. Returns its height.
float KeyboardApp::drawMonitor(float x, float width) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->Pos.x + x, vp->Pos.y + vp->Size.y - kMonitorHeight});
    ImGui::SetNextWindowSize({width, kMonitorHeight});
    bool open = true;
    ImGui::Begin("MIDI Monitor", &open,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    if (!open) setPanes(prefs_.panelOpen, false);
    if (ImGui::SmallButton("Clear")) monitor_.clear();
    ImGui::SameLine();
    ImGui::Checkbox("Follow", &monitorFollow_);
    ImGui::BeginChild("lines", {0, 0}, ImGuiChildFlags_Borders);
    ImGui::PushFont(monoFont_, 0.0f);
    ImGuiListClipper clipper;
    clipper.Begin(int(monitor_.size()));
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            ImGui::TextUnformatted(monitor_[i].c_str());
    if (monitorFollow_) ImGui::SetScrollHereY(1.0f);
    ImGui::PopFont();
    ImGui::EndChild();
    ImGui::End();
    return kMonitorHeight;
}

} // namespace kedo::app
