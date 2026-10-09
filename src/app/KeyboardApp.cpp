#include "KeyboardApp.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "imgui.h"
#include "kedo/UmpText.h"

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
    : window_(window), monoFont_(monoFont), output_(start.tuning, settings_.output), performer_(output_) {
    settings_.tuning = start.tuning;
    settings_.layout.kind = start.layout;
    settings_.layout.pianoLowest = 36;   // C2, as KeyEDO's screenshot
    rebuild(true);
    applied_ = settings_;
    output_.setup();
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
    output_.setTuning(settings_.tuning);
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

    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_ESCAPE && !io.WantCaptureKeyboard) performer_.releaseAll();
        break;

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

    // Apply what the settings bar changed.
    const bool keysChanged = !(settings_.tuning == applied_.tuning) || !(settings_.layout == applied_.layout);
    if (!(settings_.output == applied_.output)) {
        performer_.releaseAll();
        output_.allNotesOff();
        output_.setConfig(settings_.output);
        output_.setup();
    }
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

    viewport_.clampTo(layout_->bounds());
    drawKeyboard(ImGui::GetBackgroundDrawList());
    drainOutput();
}

void KeyboardApp::drainOutput() {
    for (const auto& m : output_.take()) {
        monitor_.push_back(describeUmp(m));
        if (monitor_.size() > kMonitorLines) monitor_.pop_front();
    }
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
                                     performer_.isSounding(k.step), performer_.isLatched(k.step));
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
    static const char* const kProtocols[] = {"MIDI 1.0", "MIDI 2.0"};

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

    // Output.
    int protocol = o.protocol == uw::MidiProtocol::Midi2 ? 1 : 0;
    ImGui::SetNextItemWidth(6 * em);
    if (ImGui::Combo("Protocol", &protocol, kProtocols, 2))
        o.protocol = protocol ? uw::MidiProtocol::Midi2 : uw::MidiProtocol::Midi1;
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
        output_.allNotesOff();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(no MIDI output yet: the monitor shows what would be sent)");

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
