// KeyboardEDO: SDL3 window, Dear ImGui, and the keyboard (KeyboardApp).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "KeyboardApp.h"
#include "kedo/Document.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

// The MIDI monitor's monospaced font (Cousine, from ImGui's misc/fonts),
// embedded by CMake.
extern const unsigned char kMonoFont[];
extern const unsigned int kMonoFontSize;

namespace {

void usage() {
    std::fprintf(stderr,
                 "usage: keyboardedo [DOCUMENT] [--edo N] [--origin NOTE] [--octave N]\n"
                 "                   [--layout piano|tiled] [--destination NAME] [--publish]\n"
                 "                   [--snapshot FILE.png [--wait SECONDS] [STEP...]]\n"
                 "  DOCUMENT     a .kedo document, or a KeyEDO .json (imported); the options\n"
                 "               change its showing layout (--destination, the document's)\n"
                 "  --snapshot   draw a few frames, save the window as PNG, and quit\n"
                 "  --snapshot   (also: the app's default preferences, neither read nor saved)\n"
                 "  --wait       with --snapshot: wait this long first (e.g. for other apps to\n"
                 "               see a published source)\n"
                 "  STEP         with --snapshot, done in order before the picture (window points):\n"
                 "    --click X,Y              press the mouse there, and hold it\n"
                 "    --tap [shift+]X,Y        press and release (Shift held, with shift+)\n"
                 "    --double X,Y             a double click\n"
                 "    --drag X1,Y1,X2,Y2       press, move, release\n"
                 "    --key [ctrl+][shift+]NAME  a key, by SDL's name (z, delete, right...)\n"
                 "    --press NAME             a key down, and held (e.g. z, \"left ctrl\")\n"
                 "    --release NAME           that key up\n"
                 "    --move X,Y               the mouse moved there, no button\n");
}

// --snapshot's own mouse: while it has steps, the real mouse's events are
// left out, so the pointer's real place (maybe over the window) can't join in.
constexpr SDL_MouseID kStepMouse = 0x5EED;

// One input --snapshot gives the app, through the same path as a real one.
struct Step {
    enum Kind { Click, Tap, Double, Drag, Key, Press, Release, Move } kind = Tap;
    float x = 0, y = 0, x2 = 0, y2 = 0;
    SDL_Keymod mod = SDL_KMOD_NONE;   // held during it
    SDL_Keycode key = SDLK_UNKNOWN;
};

// Modifier prefixes ("ctrl+", "shift+") off the front of `text`.
SDL_Keymod takeMods(std::string& text) {
    SDL_Keymod mod = SDL_KMOD_NONE;
    for (;;) {
        if (text.rfind("ctrl+", 0) == 0) mod |= SDL_KMOD_LCTRL, text.erase(0, 5);
        else if (text.rfind("shift+", 0) == 0) mod |= SDL_KMOD_LSHIFT, text.erase(0, 6);
        else return mod;
    }
}

void pushMouse(SDL_Window* window, Uint32 type, float x, float y, Uint8 clicks = 1) {
    SDL_Event e{};
    e.type = type;
    if (type == SDL_EVENT_MOUSE_MOTION) {
        e.motion.windowID = SDL_GetWindowID(window);
        e.motion.which = kStepMouse;
        e.motion.x = x;
        e.motion.y = y;
    } else {
        e.button.windowID = SDL_GetWindowID(window);
        e.button.which = kStepMouse;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        e.button.clicks = clicks;
        e.button.x = x;
        e.button.y = y;
    }
    SDL_PushEvent(&e);
}

// Frame `frame` of `step`; true when it's done. A press moves the pointer
// there first, as a real one would, so ImGui's idea of the mouse (from the
// real cursor, maybe over the settings bar) doesn't decide who gets it.
void pushKey(SDL_Window* window, SDL_Keycode key, SDL_Keymod mod, bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = SDL_GetWindowID(window);
    e.key.key = key;
    e.key.scancode = SDL_GetScancodeFromKey(key, nullptr);
    e.key.mod = mod;
    e.key.down = down;
    SDL_PushEvent(&e);
}

// A step's modifiers, as key presses (SDL's own modifier state follows the
// real keyboard, under Wayland at least).
void pushMods(SDL_Window* window, SDL_Keymod mod, bool down) {
    if (mod & SDL_KMOD_LSHIFT) pushKey(window, SDLK_LSHIFT, down ? mod : SDL_KMOD_NONE, down);
    if (mod & SDL_KMOD_LCTRL) pushKey(window, SDLK_LCTRL, down ? mod : SDL_KMOD_NONE, down);
}

bool runStep(SDL_Window* window, const Step& s, int frame) {
    const bool mouse = s.kind <= Step::Drag;
    if (frame == 0 && mouse) pushMods(window, s.mod, true);
    // The mouse goes there a frame before it presses, so ImGui knows it's
    // left a panel (else a press just after a panel's is the panel's).
    if (mouse) {
        if (frame == 0) {
            SDL_WarpMouseInWindow(window, s.x, s.y);
            pushMouse(window, SDL_EVENT_MOUSE_MOTION, s.x, s.y);
            return false;
        }
        --frame;
    }
    auto done = [&](bool finished) {
        if (finished && mouse) pushMods(window, s.mod, false);
        return finished;
    };
    auto press = [&](float x, float y, Uint8 clicks) {
        SDL_WarpMouseInWindow(window, x, y);
        pushMouse(window, SDL_EVENT_MOUSE_MOTION, x, y);
        pushMouse(window, SDL_EVENT_MOUSE_BUTTON_DOWN, x, y, clicks);
    };
    switch (s.kind) {
    case Step::Click:
        press(s.x, s.y, 1);
        return done(true);
    case Step::Tap:
        if (frame == 0) press(s.x, s.y, 1);
        else pushMouse(window, SDL_EVENT_MOUSE_BUTTON_UP, s.x, s.y);
        return done(frame == 1);
    case Step::Double:
        if (frame == 0 || frame == 2) press(s.x, s.y, Uint8(frame / 2 + 1));
        else pushMouse(window, SDL_EVENT_MOUSE_BUTTON_UP, s.x, s.y, Uint8(frame / 2 + 1));
        return done(frame == 3);
    case Step::Drag: {
        constexpr int kMoves = 6;
        if (frame == 0) {
            press(s.x, s.y, 1);
        } else if (frame <= kMoves) {
            const float t = float(frame) / kMoves;
            SDL_WarpMouseInWindow(window, s.x + (s.x2 - s.x) * t, s.y + (s.y2 - s.y) * t);
            pushMouse(window, SDL_EVENT_MOUSE_MOTION, s.x + (s.x2 - s.x) * t, s.y + (s.y2 - s.y) * t);
        } else {
            pushMouse(window, SDL_EVENT_MOUSE_BUTTON_UP, s.x2, s.y2);
        }
        return done(frame > kMoves);
    }
    case Step::Key:
        pushKey(window, s.key, s.mod, frame == 0);
        return frame == 1;
    case Step::Press:
    case Step::Release:
        pushKey(window, s.key, SDL_KMOD_NONE, s.kind == Step::Press);
        return true;
    case Step::Move:
        SDL_WarpMouseInWindow(window, s.x, s.y);
        pushMouse(window, SDL_EVENT_MOUSE_MOTION, s.x, s.y);
        return true;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    kedo::app::KeyboardApp::Start start;
    kedo::Document& document = start.document;
    std::string snapshot;
    std::vector<Step> steps;   // --click, --tap, --double, --drag, --key: in order
    double wait = 0;   // seconds before the steps and snapshot
    // The document first, so the options can override it.
    int first = 1;
    if (argc > 1 && argv[1][0] != '-') {
        std::string error;
        const auto loaded = kedo::loadDocument(argv[1], &error);
        if (!loaded) {
            std::fprintf(stderr, "keyboardedo: %s\n", error.c_str());
            return 1;
        }
        document = *loaded;
        if (kedo::isBundlePath(argv[1])) start.path = argv[1];   // a KeyEDO file is imported: unsaved
        first = 2;
    }
    kedo::LayoutSetup& doc = document.layout();
    for (int i = first; i < argc; ++i) {
        const char* arg = argv[i];
        if (!std::strcmp(arg, "--publish")) {
            start.publish = true;
            continue;
        }
        const char* value = i + 1 < argc ? argv[++i] : nullptr;
        if (!value) { usage(); return 2; }
        if (!std::strcmp(arg, "--edo")) doc.tuning.edo = std::atoi(value);
        else if (!std::strcmp(arg, "--origin")) doc.tuning.originPitchClass = kedo::pitchClassOf(value);
        else if (!std::strcmp(arg, "--octave")) doc.tuning.originOctave = std::atoi(value);
        else if (!std::strcmp(arg, "--layout")) doc.layout.kind = !std::strcmp(value, "tiled") ? kedo::LayoutKind::Tiled : kedo::LayoutKind::Piano;
        else if (!std::strcmp(arg, "--destination")) document.destination = value;
        else if (!std::strcmp(arg, "--snapshot")) snapshot = value;
        else if (!std::strcmp(arg, "--wait")) wait = std::atof(value);
        else if (!std::strcmp(arg, "--click") || !std::strcmp(arg, "--tap") || !std::strcmp(arg, "--double") ||
                 !std::strcmp(arg, "--drag") || !std::strcmp(arg, "--key") || !std::strcmp(arg, "--press") ||
                 !std::strcmp(arg, "--release") || !std::strcmp(arg, "--move")) {
            Step s;
            std::string text = value;
            s.mod = takeMods(text);
            const bool press = !std::strcmp(arg, "--press"), release = !std::strcmp(arg, "--release");
            if (!std::strcmp(arg, "--key") || press || release) {
                s.kind = press ? Step::Press : release ? Step::Release : Step::Key;
                s.key = SDL_GetKeyFromName(text.c_str());
                if (s.key == SDLK_UNKNOWN) { usage(); return 2; }
            } else if (!std::strcmp(arg, "--drag")) {
                s.kind = Step::Drag;
                if (std::sscanf(text.c_str(), "%f,%f,%f,%f", &s.x, &s.y, &s.x2, &s.y2) != 4) { usage(); return 2; }
            } else {
                s.kind = !std::strcmp(arg, "--click") ? Step::Click : !std::strcmp(arg, "--tap") ? Step::Tap
                       : !std::strcmp(arg, "--move") ? Step::Move : Step::Double;
                if (std::sscanf(text.c_str(), "%f,%f", &s.x, &s.y) != 2) { usage(); return 2; }
            }
            steps.push_back(s);
        }
        else { usage(); return 2; }
    }
    if (doc.tuning.edo < kedo::Tuning::kMinEdo || doc.tuning.edo > kedo::Tuning::kMaxEdo ||
        doc.tuning.originPitchClass < 0) {
        usage();
        return 2;
    }

#ifdef __linux__
    // On Wayland the title bar and borders come from libdecor's GTK plugin (the only GTK
    // here). VS Code's snaps leak their GTK environment into their terminals, saving each
    // original as NAME_VSCODE_SNAP_ORIG: GDK_BACKEND=x11 stops the plugin, leaving a window
    // that can't be moved or resized, and the snap's GSettings schemas abort GLib. Restore them.
    {
        const std::string suffix = "_VSCODE_SNAP_ORIG";
        std::vector<std::pair<std::string, std::string>> originals;
        for (char** e = environ; *e; ++e) {
            const std::string entry = *e;
            const auto eq = entry.find('=');
            if (eq == std::string::npos || eq <= suffix.size() ||
                entry.compare(eq - suffix.size(), suffix.size(), suffix) != 0) continue;
            originals.emplace_back(entry.substr(0, eq - suffix.size()), entry.substr(eq + 1));
        }
        for (const auto& [name, value] : originals) {
            if (value.empty()) unsetenv(name.c_str());
            else setenv(name.c_str(), value.c_str(), 1);
            unsetenv((name + suffix).c_str());
        }
        // XDG_DATA_HOME, with the snap's own schemas, has no saved original.
        if (const char* data = std::getenv("XDG_DATA_HOME");
            !originals.empty() && data && std::strstr(data, "/snap/"))
            unsetenv("XDG_DATA_HOME");
    }
    if (const char* gdk = std::getenv("GDK_BACKEND"); gdk && !std::strstr(gdk, "wayland"))
        unsetenv("GDK_BACKEND");
#endif
    SDL_SetAppMetadata("KeyboardEDO", "0.1.0", "com.unlikelyware.keyboardedo");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_Window* window = SDL_CreateWindow("KeyboardEDO", int(1280 * scale), int(760 * scale),
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefaultVector();
    ImFontConfig mono;
    mono.FontDataOwnedByAtlas = false;
    ImFont* monoFont = io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(kMonoFont),
                                                      int(kMonoFontSize), 0, &mono);
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleDpi = scale;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    start.ownPreferences = !snapshot.empty();
    auto app = std::make_unique<kedo::app::KeyboardApp>(window, monoFont, start);

    // Draw only when something happens: a few frames after each event (ImGui
    // settles hover and focus over two), and every frame while a key is held.
    int framesToDraw = 3;
    // Snapshot timing: ImGui sizes its windows over the first frames, so the
    // click comes after a few, and the picture a few after that.
    int snapshotFrames = 0;
    const Uint64 snapshotFrom = SDL_GetTicks() + Uint64(wait * 1000);
    std::size_t step = 0;   // the step being done, and its frame
    int stepFrame = 0;
    int stepsDoneAt = steps.empty() ? 3 : -1;   // the frame the last one finished
    bool running = true;
    while (running) {
        SDL_Event e;
        bool got = framesToDraw > 0 || app->animating() ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 500);
        while (got) {
            const bool realMouse =
                (e.type == SDL_EVENT_MOUSE_MOTION && e.motion.which != kStepMouse) ||
                ((e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                 e.button.which != kStepMouse) ||
                e.type == SDL_EVENT_MOUSE_WHEEL;
            if (!steps.empty() && realMouse) {
                got = SDL_PollEvent(&e);
                continue;
            }
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(window)))
                running = false;
            app->handleEvent(e);
            framesToDraw = 3;
            got = SDL_PollEvent(&e);
        }
        app->flush();   // notes out now, not after drawing
        if (framesToDraw == 0 && !app->animating()) continue;
        if (framesToDraw > 0) --framesToDraw;
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        app->frame();
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, 0.11f, 0.11f, 0.11f, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        if (!snapshot.empty() && SDL_GetTicks() >= snapshotFrom) ++snapshotFrames;
        if (!snapshot.empty() && stepsDoneAt >= 0 && snapshotFrames == stepsDoneAt + 3) {
            SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
            const bool saved = shot && SDL_SavePNG(shot, snapshot.c_str());
            if (!saved) std::fprintf(stderr, "snapshot: %s\n", SDL_GetError());
            SDL_DestroySurface(shot);
            running = false;
        }
        if (!snapshot.empty()) framesToDraw = 1;
        // The steps, from the third frame (ImGui has sized its windows), one
        // after another.
        // Two frames between steps, as a person couldn't be faster: a popup
        // (a combo's menu) is sized hidden for a frame before it can be clicked.
        if (!snapshot.empty() && snapshotFrames >= 3 && step < steps.size()) {
            if (stepFrame < 0) {
                ++stepFrame;
            } else if (runStep(window, steps[step], stepFrame++)) {
                ++step;
                stepFrame = -2;
                if (step == steps.size()) stepsDoneAt = snapshotFrames;
            }
        }
        SDL_RenderPresent(renderer);
    }

    app.reset();   // notes off before the outputs close
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
