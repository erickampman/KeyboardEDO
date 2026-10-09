#include "KeyboardApp.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "Icons.h"
#include "imgui.h"
#include "kedo/Synopsis.h"

namespace kedo::app {

namespace {

constexpr float kMonitorHeight = 170;
constexpr size_t kMonitorLines = 1000;
constexpr double kWheelPoints = 30;   // scroll distance per wheel unit

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

KeyboardApp::KeyboardApp(SDL_Window* window, ImFont* monoFont, const Start& start)
    : window_(window),
      monoFont_(monoFont),
      settings_(start.document),
      applied_(start.document),
      saved_(start.document),
      path_(start.path),
      targets_(start.document.output),
      performer_(targets_.sink()) {
    if (char* folder = SDL_GetPrefPath("unlikelyware", "KeyboardEDO")) {
        prefsPath_ = std::string(folder) + "preferences.json";
        SDL_free(folder);
        prefs_ = loadPreferences(prefsPath_);
    }
    rebuild(true);
    targets_.setConfig(settings_.output);
    targets_.setDestination(settings_.destination);
    targets_.setPublish(prefs_.publish || start.publish);
    updateTitle();
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
    performer_.releaseAll();
    targets_.setPublish(on);
    prefs_.publish = on;
    if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
}

KeyboardApp::~KeyboardApp() {
    // Release what's held while the outputs still exist; their destructor
    // then sends All Notes Off.
    performer_.releaseAll();
    flush();
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
    performer_.setConfig(playConfig());

    if (resetView) {
        // Start at zoom 1, the lowest keys at the left (Tiled: the origin).
        viewport_.zoom = 1;
        viewport_.scroll = {layout_->bounds().x, layout_->bounds().y};
        if (settings_.layout.kind == LayoutKind::Tiled)
            if (const auto k = layout_->keyForStep(0)) viewport_.scroll.x = k->center.x - 2 * kHalfDiag;
    }
}

Modifiers KeyboardApp::modifiers() const {
    return {.latch = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0};
}

// --- Input ------------------------------------------------------------------------

void KeyboardApp::handleEvent(const SDL_Event& e) {
    const ImGuiIO& io = ImGui::GetIO();
    switch (e.type) {
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        // Touches arrive as fingers below; SDL's mouse copies of them are ImGui's.
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) break;
        const Point p{e.button.x, e.button.y};
        if (!inKeyboard(p) || io.WantCaptureMouse) break;
        mouseDown_ = true;
        performer_.pointerDown(0, viewport_.toLayout(p), modifiers());
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (mouseDown_ && e.motion.which != SDL_TOUCH_MOUSEID)
            performer_.pointerMove(0, viewport_.toLayout({e.motion.x, e.motion.y}));
        break;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (mouseDown_ && e.button.which != SDL_TOUCH_MOUSEID && e.button.button == SDL_BUTTON_LEFT) {
            performer_.pointerUp(0, modifiers());
            mouseDown_ = false;
        }
        break;

    case SDL_EVENT_FINGER_DOWN: {
        // Only touchscreens: a trackpad's fingers have no place on the screen.
        if (SDL_GetTouchDeviceType(e.tfinger.touchID) != SDL_TOUCH_DEVICE_DIRECT) break;
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
        performer_.pointerMove(it->second, viewport_.toLayout({e.tfinger.x * w, e.tfinger.y * h}));
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
        // A keyboard that fits vertically scrolls sideways with a plain wheel.
        if (layout_->bounds().h * viewport_.zoom <= viewport_.screen.h && dx == 0) std::swap(dx, dy);
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

    case SDL_EVENT_KEY_DOWN: {
        if (e.key.key == SDLK_ESCAPE && !io.WantCaptureKeyboard) performer_.releaseAll();
        // Cmd (Mac) or Ctrl: O open, S save, Shift-S save as.
        const bool command = (e.key.mod & (SDL_KMOD_GUI | SDL_KMOD_CTRL)) != 0;
        if (command && !e.key.repeat) {
            if (e.key.key == SDLK_O) showDialog(Dialog::Open);
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
        break;
    default:
        break;
    }
}

// --- Frame --------------------------------------------------------------------------

void KeyboardApp::frame() {
    int w = 0, h = 0;
    SDL_GetWindowSize(window_, &w, &h);

    // The toolbar across the top, the settings panel down the right, the
    // monitor under the keyboard; the keyboard gets what's left.
    drawToolbar();
    const float panelWidth = prefs_.panelOpen ? drawPanel() : 0.0f;
    const float monitorHeight = prefs_.monitorOpen ? drawMonitor(float(w) - panelWidth) : 0.0f;
    viewport_.screen = {0, barHeight_, std::max(0.0, double(w) - panelWidth),
                        std::max(0.0, double(h) - barHeight_ - monitorHeight)};

    takeDialogResults();
    applySettings();
    targets_.poll();

    viewport_.clampTo(layout_->bounds());
    drawKeyboard(ImGui::GetBackgroundDrawList());
    flush();
    updateTitle();
}

void KeyboardApp::flush() { targets_.flush(monitor_, kMonitorLines); }

// Apply what the settings bar (or a document) changed.
void KeyboardApp::applySettings() {
    // A new destination or publishing starts from silence: notes held now
    // would never get their note-off there.
    if (settings_.destination != applied_.destination) {
        performer_.releaseAll();
        targets_.setDestination(settings_.destination);
    }
    if (!(settings_.output == applied_.output)) {
        performer_.releaseAll();
        targets_.setConfig(settings_.output);
    }
    const bool keysChanged = !(settings_.tuning == applied_.tuning) || !(settings_.layout == applied_.layout);
    if (!(settings_.play == applied_.play) || settings_.output.midi1Expression != applied_.output.midi1Expression)
        performer_.setConfig(playConfig());
    if (keysChanged) {
        const bool newKeys = settings_.tuning.edo != applied_.tuning.edo ||
                             settings_.layout.kind != applied_.layout.kind;
        rebuild(newKeys);
    } else if (settings_.layout.kind == LayoutKind::Tiled && !animating() &&
               tiledRowsFor(viewport_.screen.h) != settings_.layout.tiledRows) {
        rebuild(false);   // the window or zoom changed how many rows fit
    }
    applied_ = settings_;
}

// --- Documents ----------------------------------------------------------------------

void KeyboardApp::open(const std::string& path) {
    std::string error;
    const auto document = loadDocument(path, &error);
    if (!document) {
        status_ = error;
        return;
    }
    status_.clear();
    settings_ = *document;
    saved_ = *document;
    path_ = path;
    applySettings();
    rebuild(true);
}

bool KeyboardApp::save(const std::string& path) {
    std::string error;
    if (!saveDocument(path, settings_, &error)) {
        status_ = error;
        return false;
    }
    status_.clear();
    saved_ = settings_;
    path_ = path;
    return true;
}

void KeyboardApp::showDialog(Dialog dialog) {
    static const SDL_DialogFileFilter kFilters[] = {{"Keyboard documents (JSON)", "json"}};
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
    if (dialog == Dialog::Open)
        SDL_ShowOpenFileDialog(callback, request, window_, kFilters, 1, nullptr, false);
    else
        SDL_ShowSaveFileDialog(callback, request, window_, kFilters, 1, path_.empty() ? nullptr : path_.c_str());
}

void KeyboardApp::takeDialogResults() {
    std::vector<std::pair<Dialog, std::string>> results;
    {
        std::lock_guard<std::mutex> g(dialogLock_);
        results.swap(dialogResults_);
    }
    for (auto& [dialog, path] : results) {
        if (dialog == Dialog::Open) {
            open(path);
        } else {
            if (path.size() < 5 || path.compare(path.size() - 5, 5, ".json") != 0) path += ".json";
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
    // Tiled's row count follows the window, so it isn't an edit.
    Document current = settings_;
    current.layout.tiledRows = saved_.layout.tiledRows;
    std::string title = name + (current == saved_ ? "" : " (edited)") + " - KeyboardEDO";
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
    for (const Key& k : layout_->keysIn(viewport_.visible())) {
        const KeyLook look = keyLook(k, settings_.tuning, settings_.layout.kind, settings_.tileColor,
                                     performer_.isSounding(k.note.id), performer_.isLatched(k.note.id));
        const ImVec2 c = vec(viewport_.toScreen(k.center));
        const float hw = float(k.halfWidth) * zoom, hh = float(k.halfHeight) * zoom;

        float titleY;
        float titleSize = 13 * zoom, detailSize = 10 * zoom;
        if (k.shape == KeyShape::Diamond) {
            const ImVec2 top{c.x, c.y - hh}, right{c.x + hw, c.y}, bottom{c.x, c.y + hh}, left{c.x - hw, c.y};
            list->AddQuadFilled(top, right, bottom, left, color(look.fill));
            list->AddQuad(top, right, bottom, left, color(look.outline), 1.0f);
            titleY = c.y - (look.detail.empty() ? titleSize / 2 : titleSize * 0.8f);
        } else {
            const ImVec2 a{c.x - hw, c.y - hh}, b{c.x + hw, c.y + hh};
            const float round = 3 * zoom;
            list->AddRectFilled(a, b, color(look.fill), round, ImDrawFlags_RoundCornersBottom);
            list->AddRect(a, b, color(look.outline), round, ImDrawFlags_RoundCornersBottom, 1.0f);
            titleSize = 10 * zoom;
            titleY = b.y - titleSize - 6 * zoom;
        }

        if (titleSize < 6) continue;   // too small to read
        if (!look.title.empty()) {
            const ImVec2 size = font->CalcTextSizeA(titleSize, FLT_MAX, 0, look.title.c_str());
            list->AddText(font, titleSize, {c.x - size.x / 2, titleY}, color(look.ink), look.title.c_str());
        }
        if (!look.detail.empty()) {
            const ImVec2 size = font->CalcTextSizeA(detailSize, FLT_MAX, 0, look.detail.c_str());
            const Rgb detailInk = k.shape == KeyShape::Diamond && settings_.layout.kind == LayoutKind::Piano
                                      ? Rgb{0.0f, 0.48f, 1.0f}   // KeyEDO's blue frequency
                                      : look.ink;
            list->AddText(font, detailSize, {c.x - size.x / 2, titleY + titleSize + 1},
                          color(detailInk), look.detail.c_str());
        }
    }
    list->PopClipRect();
}

void KeyboardApp::setPanes(bool panel, bool monitor) {
    if (panel == prefs_.panelOpen && monitor == prefs_.monitorOpen) return;
    prefs_.panelOpen = panel;
    prefs_.monitorOpen = monitor;
    if (!prefsPath_.empty()) savePreferences(prefsPath_, prefs_);
}

// The toolbar: files on the left, the protocol badge in the middle, routing
// and the keyboard's modes on the right (KeyEDO's), and the synopsis under it.
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

    if (iconButton("open", Icon::Open, icon, "Open... (Cmd/Ctrl-O)")) showDialog(Dialog::Open);
    ImGui::SameLine();
    if (iconButton("save", Icon::Save, icon, "Save (Cmd/Ctrl-S; Shift for Save As)")) {
        if (path_.empty()) showDialog(Dialog::SaveAs);
        else save(path_);
    }

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
    const int buttons = 6;
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
    if (iconButton("panic", Icon::Panic, icon, "All Notes Off")) {
        performer_.releaseAll();
        targets_.allNotesOff();
    }
    ImGui::SameLine();
    if (iconButton("monitor", Icon::Monitor, icon, "MIDI monitor", prefs_.monitorOpen))
        setPanes(prefs_.panelOpen, !prefs_.monitorOpen);
    ImGui::SameLine();
    if (iconButton("configure", Icon::Configure, icon, "Settings", prefs_.panelOpen))
        setPanes(!prefs_.panelOpen, prefs_.monitorOpen);

    // The synopsis, centred under it.
    LiveState live;
    live.latch = performer_.latchMode();
    live.publishing = targets_.publishing();
    if (!settings_.destination.empty()) {
        live.destination = settings_.destination;
        live.destinationFound = false;
        for (const auto& d : targets_.destinations())
            if (d.id == settings_.destination || d.name == settings_.destination) {
                live.destination = d.name;
                live.destinationFound = true;
            }
    }
    const std::string line = synopsis(settings_, live);
    ImGui::SetCursorPosX(std::max(0.0f, (vp->Size.x - ImGui::CalcTextSize(line.c_str()).x) / 2));
    ImGui::TextDisabled("%s", line.c_str());
    if (!status_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.45f, 0.4f, 1.0f}, "  %s", status_.c_str());
    }

    barHeight_ = ImGui::GetWindowHeight();
    ImGui::End();
}

// KeyEDO's routing menu: Send to a destination, and Publish our own source.
void KeyboardApp::drawRoutingMenu() {
    if (!ImGui::BeginPopup("routing")) return;
    if (!targets_.available()) {
        ImGui::TextDisabled("No MIDI output on this platform yet:");
        ImGui::TextDisabled("the monitor shows what would be sent.");
        ImGui::EndPopup();
        return;
    }
    ImGui::SeparatorText("Send to");
    if (ImGui::MenuItem("None", nullptr, settings_.destination.empty())) settings_.destination.clear();
    bool found = settings_.destination.empty();
    for (const auto& d : targets_.destinations()) {
        const bool chosen = d.id == settings_.destination || d.name == settings_.destination;
        found = found || chosen;
        const std::string label = d.name + "##" + d.id;
        if (ImGui::MenuItem(label.c_str(), d.protocol == uw::MidiProtocol::Midi2 ? "2.0" : "1.0", chosen))
            settings_.destination = d.id;
    }
    if (!found) {
        const std::string missing = settings_.destination + " (not found)";
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
    static const char* const kLayouts[] = {"Piano", "Tiled"};
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

    if (ImGui::CollapsingHeader("Tuning", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::InputInt("EDO steps", &t.edo)) t.edo = std::clamp(t.edo, Tuning::kMinEdo, Tuning::kMaxEdo);
        ImGui::Combo("Origin note", &t.originPitchClass, kNotes, 12);
        if (ImGui::InputInt("Origin octave", &t.originOctave)) t.originOctave = std::clamp(t.originOctave, -1, 7);
        ImGui::TextDisabled("Origin: %s%d, %.2f Hz", kNotes[t.originPitchClass], t.originOctave, t.originFrequency());
    }

    // The combo inside has the same label: "##kind" gives it its own ID.
    if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
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
                    ImGui::SetItemTooltip("right %+d, up-right %+d, up-left %+d steps", g.right, g.upRight, g.upLeft());
                }
                ImGui::EndCombo();
            }
            const auto& g = presets[l.generatorIndex];
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Steps: right %+d, up-right %+d, up-left %+d", g.right, g.upRight, g.upLeft());
            ImGui::PopStyleColor();
            enumCombo("Tile colour", settings_.tileColor, kTileColors, ImGui::CalcItemWidth());
        }
        if (ImGui::Button("Reset View")) rebuild(true);
    }

    if (ImGui::CollapsingHeader("Playing", ImGuiTreeNodeFlags_DefaultOpen)) {
        enumCombo("Drag", p.drag, kDrags, ImGui::CalcItemWidth());
        if (p.drag == DragBehavior::Gliss) {
            ImGui::Checkbox("Glide (Pitch 7.25)", &p.glide);
            ImGui::SetItemTooltip("Slide one note's pitch from key to key instead of striking each (MIDI 2.0)");
        } else {
            enumCombo("Horizontal", p.horizontal, kHorizontal, ImGui::CalcItemWidth());
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

    if (ImGui::CollapsingHeader("MIDI", ImGuiTreeNodeFlags_DefaultOpen)) {
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
        ImGui::TextWrapped("%s", path_.empty() ? "Not saved yet." : path_.c_str());
    }

    ImGui::PopItemWidth();
    ImGui::End();
    return width;
}

// The MIDI monitor, under the keyboard. Returns its height.
float KeyboardApp::drawMonitor(float width) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y + vp->Size.y - kMonitorHeight});
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
