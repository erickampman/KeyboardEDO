#include "KeyboardApp.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "imgui.h"

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
    performer_.setConfig(settings_.play);

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
        double dx = -e.wheel.x * kWheelPoints, dy = -e.wheel.y * kWheelPoints;
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

    drawSettings();
    drawMonitor();
    viewport_.screen = {0, barHeight_, double(w), std::max(0.0, double(h) - barHeight_ - kMonitorHeight)};

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
    if (!(settings_.play == applied_.play)) performer_.setConfig(settings_.play);
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

void KeyboardApp::drawSettings() {
    static const char* const kNotes[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char* const kLayouts[] = {"Piano", "Tiled"};
    static const char* const kTileColors[] = {"Off", "Note", "Octave"};
    static const char* const kDrags[] = {"Gliss", "Fixed"};
    static const char* const kHorizontal[] = {"Off", "Pitch Bend"};
    static const char* const kVertical[] = {"Off", "Controller", "Pressure", "Pitch Bend"};

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSizeConstraints({vp->Size.x, 0}, {vp->Size.x, FLT_MAX});
    ImGui::Begin("Settings", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    const float em = ImGui::GetFontSize();
    auto& t = settings_.tuning;
    auto& l = settings_.layout;
    auto& p = settings_.play;
    auto& o = settings_.output;

    // Tuning and layout.
    ImGui::SetNextItemWidth(6 * em);
    if (ImGui::InputInt("EDO", &t.edo)) t.edo = std::clamp(t.edo, Tuning::kMinEdo, Tuning::kMaxEdo);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(4 * em);
    ImGui::Combo("Origin", &t.originPitchClass, kNotes, 12);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(5 * em);
    if (ImGui::InputInt("Octave", &t.originOctave)) t.originOctave = std::clamp(t.originOctave, -1, 7);
    ImGui::SameLine();
    enumCombo("Layout", l.kind, kLayouts, 5 * em);
    if (l.kind == LayoutKind::Tiled) {
        const auto presets = generatorPresets(t.edo);
        if (l.generatorIndex >= int(presets.size())) l.generatorIndex = 0;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(9 * em);
        if (ImGui::BeginCombo("Generator", presets[l.generatorIndex].name.c_str())) {
            for (int i = 0; i < int(presets.size()); ++i) {
                const auto& g = presets[i];
                const std::string name = g.name + "  (" + std::to_string(g.right) + ", " +
                                         std::to_string(g.upRight) + ")";
                if (ImGui::Selectable(name.c_str(), i == l.generatorIndex)) l.generatorIndex = i;
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        enumCombo("Tile Color", settings_.tileColor, kTileColors, 5 * em);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Lock", &settings_.locked);
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) rebuild(true);

    // Playing.
    enumCombo("Drag", p.drag, kDrags, 5 * em);
    if (p.drag == DragBehavior::Gliss) {
        ImGui::SameLine();
        ImGui::Checkbox("Glide (Pitch 7.25)", &p.glide);
    } else {
        ImGui::SameLine();
        enumCombo("Horizontal", p.horizontal, kHorizontal, 7 * em);
        ImGui::SameLine();
        enumCombo("Vertical", p.vertical, kVertical, 7 * em);
        if (p.vertical == VerticalControl::Controller) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(5 * em);
            if (ImGui::InputInt("##cc", &p.verticalController))
                p.verticalController = std::clamp(p.verticalController, 0, 127);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Per-note", &p.perNoteExpression);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Strike Velocity", &p.strikeVelocity);
    if (p.strikeVelocity) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(6 * em);
        ImGui::SliderInt("Min", &p.strikeVelocityMin, 1, 126);
    }
    ImGui::SameLine();
    bool latch = performer_.latchMode();
    if (ImGui::Checkbox("Latch", &latch)) performer_.setLatchMode(latch);

    // Documents and output.
    if (ImGui::Button("Open...")) showDialog(Dialog::Open);
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        if (path_.empty()) showDialog(Dialog::SaveAs);
        else save(path_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save As...")) showDialog(Dialog::SaveAs);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    if (targets_.available()) {
        // Send to: KeyEDO's routing menu. A destination that's wanted but absent
        // (unplugged, or named by a document) stays chosen, marked missing.
        const auto& list = targets_.destinations();
        std::string current = "None";
        if (!settings_.destination.empty()) {
            current = settings_.destination + " (not found)";
            for (const auto& d : list)
                if (d.id == settings_.destination || d.name == settings_.destination) current = d.name;
        }
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Send to");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(14 * em);
        if (ImGui::BeginCombo("##destination", current.c_str())) {
            if (ImGui::Selectable("None", settings_.destination.empty())) settings_.destination.clear();
            for (const auto& d : list) {
                const std::string label =
                    d.name + (d.protocol == uw::MidiProtocol::Midi2 ? "  (2.0)" : "  (1.0)") + "##" + d.id;
                if (ImGui::Selectable(label.c_str(), d.id == settings_.destination)) settings_.destination = d.id;
            }
            ImGui::EndCombo();
        }
        // KeyEDO's protocol badge: what the destination gets.
        if (targets_.destinationOpen()) {
            ImGui::SameLine();
            ImGui::TextColored({0.5f, 0.8f, 1.0f, 1.0f}, "%s",
                               targets_.destinationProtocol() == uw::MidiProtocol::Midi2 ? "MIDI 2.0" : "MIDI 1.0");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        // Publish: our own source, for apps that open sources (DAWs such as
        // REAPER, which list it as a MIDI input). Remembered across launches.
        bool publish = targets_.publishing();
        if (ImGui::Checkbox("Publish as \"KeyboardEDO\"", &publish)) setPublish(publish);
        ImGui::SetItemTooltip(
            "Offers a MIDI source named \"%s\" that other apps can receive from: in\n"
            "REAPER, it's a MIDI input device. It keeps the same identity from one\n"
            "launch to the next, and this setting is remembered. An app that was\n"
            "already running when it appeared may need to rescan its MIDI devices\n"
            "(REAPER: Preferences > MIDI Inputs > Reset all MIDI devices).",
            MidiTargets::kPublishedName);
        ImGui::SameLine();
        if (targets_.publishing()) ImGui::TextColored({0.45f, 0.85f, 0.45f, 1.0f}, "on");
        else ImGui::TextDisabled("off");
    } else {
        ImGui::TextDisabled("(no MIDI output on this platform yet: the monitor shows what would be sent)");
    }
    ImGui::SameLine();
    int group = o.group + 1, channel = o.channel + 1;
    ImGui::SetNextItemWidth(5 * em);
    if (ImGui::InputInt("Group", &group)) o.group = std::uint8_t(std::clamp(group, 1, 16) - 1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(5 * em);
    if (ImGui::InputInt("Channel", &channel)) o.channel = std::uint8_t(std::clamp(channel, 1, 16) - 1);
    ImGui::SameLine();
    ImGui::Checkbox("Per-Note Mgmt", &o.perNoteManagement);
    ImGui::SameLine();
    if (ImGui::Button("All Notes Off")) {
        performer_.releaseAll();
        targets_.allNotesOff();
    }
    if (!status_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored({1.0f, 0.45f, 0.4f, 1.0f}, "%s", status_.c_str());
    }

    barHeight_ = ImGui::GetWindowHeight();
    ImGui::End();
}

void KeyboardApp::drawMonitor() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({vp->Pos.x, vp->Pos.y + vp->Size.y - kMonitorHeight});
    ImGui::SetNextWindowSize({vp->Size.x, kMonitorHeight});
    ImGui::Begin("MIDI Monitor", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
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
}

} // namespace kedo::app
