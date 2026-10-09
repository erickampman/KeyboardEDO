// KeyboardEDO: SDL3 window, Dear ImGui, and the keyboard (KeyboardApp).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

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
                 "usage: keyboardedo [DOCUMENT.json] [--edo N] [--origin NOTE] [--octave N]\n"
                 "                   [--layout piano|tiled] [--destination NAME] [--publish]\n"
                 "                   [--snapshot FILE.png [--click X,Y] [--wait SECONDS]]\n"
                 "  DOCUMENT     a KeyboardEDO or KeyEDO document; the options override it\n"
                 "  --snapshot   draw a few frames, save the window as PNG, and quit\n"
                 "  --click      with --snapshot: press the mouse at X,Y (window points) first\n"
                 "  --wait       with --snapshot: wait this long first (e.g. for other apps to\n"
                 "               see a published source)\n");
}

} // namespace

int main(int argc, char** argv) {
    kedo::app::KeyboardApp::Start start;
    kedo::Document& doc = start.document;
    std::string snapshot;
    float clickX = -1, clickY = -1;
    double wait = 0;   // seconds before the click and snapshot
    // The document first, so the options can override it.
    int first = 1;
    if (argc > 1 && argv[1][0] != '-') {
        std::string error;
        const auto loaded = kedo::loadDocument(argv[1], &error);
        if (!loaded) {
            std::fprintf(stderr, "keyboardedo: %s\n", error.c_str());
            return 1;
        }
        doc = *loaded;
        start.path = argv[1];
        first = 2;
    }
    for (int i = first; i < argc; ++i) {
        const char* arg = argv[i];
        if (!std::strcmp(arg, "--publish")) {
            doc.publish = true;
            continue;
        }
        const char* value = i + 1 < argc ? argv[++i] : nullptr;
        if (!value) { usage(); return 2; }
        if (!std::strcmp(arg, "--edo")) doc.tuning.edo = std::atoi(value);
        else if (!std::strcmp(arg, "--origin")) doc.tuning.originPitchClass = kedo::pitchClassOf(value);
        else if (!std::strcmp(arg, "--octave")) doc.tuning.originOctave = std::atoi(value);
        else if (!std::strcmp(arg, "--layout")) doc.layout.kind = !std::strcmp(value, "tiled") ? kedo::LayoutKind::Tiled : kedo::LayoutKind::Piano;
        else if (!std::strcmp(arg, "--destination")) doc.destination = value;
        else if (!std::strcmp(arg, "--snapshot")) snapshot = value;
        else if (!std::strcmp(arg, "--click") && std::sscanf(value, "%f,%f", &clickX, &clickY) == 2) {}
        else if (!std::strcmp(arg, "--wait")) wait = std::atof(value);
        else { usage(); return 2; }
    }
    if (doc.tuning.edo < kedo::Tuning::kMinEdo || doc.tuning.edo > kedo::Tuning::kMaxEdo ||
        doc.tuning.originPitchClass < 0) {
        usage();
        return 2;
    }

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

    auto app = std::make_unique<kedo::app::KeyboardApp>(window, monoFont, start);

    // Draw only when something happens: a few frames after each event (ImGui
    // settles hover and focus over two), and every frame while a key is held.
    int framesToDraw = 3;
    // Snapshot timing: ImGui sizes its windows over the first frames, so the
    // click comes after a few, and the picture a few after that.
    int snapshotFrames = 0;
    const Uint64 snapshotFrom = SDL_GetTicks() + Uint64(wait * 1000);
    bool clicked = false;
    bool running = true;
    while (running) {
        SDL_Event e;
        bool got = framesToDraw > 0 || app->animating() ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 500);
        while (got) {
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
        if (!snapshot.empty() && snapshotFrames == 6) {
            SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
            const bool saved = shot && SDL_SavePNG(shot, snapshot.c_str());
            if (!saved) std::fprintf(stderr, "snapshot: %s\n", SDL_GetError());
            SDL_DestroySurface(shot);
            running = false;
        }
        if (!snapshot.empty()) framesToDraw = 1;
        if (!snapshot.empty() && clickX >= 0 && snapshotFrames == 3 && !clicked) {
            clicked = true;
            // A synthetic press, through the same path as a real one. It moves
            // the pointer there first, as a real click would, so ImGui's idea
            // of the mouse (from the real cursor, maybe over the settings bar)
            // doesn't decide who gets it.
            SDL_WarpMouseInWindow(window, clickX, clickY);
            SDL_Event move{};
            move.type = SDL_EVENT_MOUSE_MOTION;
            move.motion.windowID = SDL_GetWindowID(window);
            move.motion.which = 1;
            move.motion.x = clickX;
            move.motion.y = clickY;
            SDL_PushEvent(&move);
            SDL_Event press{};
            press.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            press.button.windowID = SDL_GetWindowID(window);
            press.button.which = 1;
            press.button.button = SDL_BUTTON_LEFT;
            press.button.down = true;
            press.button.x = clickX;
            press.button.y = clickY;
            SDL_PushEvent(&press);
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
