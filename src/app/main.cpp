// KeyboardEDO: SDL3 window, Dear ImGui, and the keyboard (KeyboardApp).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <SDL3/SDL.h>

#include "KeyboardApp.h"
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
                 "usage: keyboardedo [--edo N] [--origin NOTE] [--octave N] [--layout piano|tiled]\n"
                 "                   [--snapshot FILE.png [--click X,Y]]\n"
                 "  --snapshot  draw a few frames, save the window as PNG, and quit\n"
                 "  --click     with --snapshot: press the mouse at X,Y (window points) first\n");
}

} // namespace

int main(int argc, char** argv) {
    kedo::app::KeyboardApp::Start start;
    std::string snapshot;
    float clickX = -1, clickY = -1;
    for (int i = 1; i < argc; i += 2) {   // every option takes a value
        const char* arg = argv[i];
        const char* value = i + 1 < argc ? argv[i + 1] : nullptr;
        if (!value) { usage(); return 2; }
        if (!std::strcmp(arg, "--edo")) start.tuning.edo = std::atoi(value);
        else if (!std::strcmp(arg, "--origin")) start.tuning.originPitchClass = kedo::pitchClassOf(value);
        else if (!std::strcmp(arg, "--octave")) start.tuning.originOctave = std::atoi(value);
        else if (!std::strcmp(arg, "--layout")) start.layout = !std::strcmp(value, "tiled") ? kedo::LayoutKind::Tiled : kedo::LayoutKind::Piano;
        else if (!std::strcmp(arg, "--snapshot")) snapshot = value;
        else if (!std::strcmp(arg, "--click") && std::sscanf(value, "%f,%f", &clickX, &clickY) == 2) {}
        else { usage(); return 2; }
    }
    if (start.tuning.edo < kedo::Tuning::kMinEdo || start.tuning.edo > kedo::Tuning::kMaxEdo ||
        start.tuning.originPitchClass < 0) {
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

    kedo::app::KeyboardApp app(window, monoFont, start);

    // Draw only when something happens: a few frames after each event (ImGui
    // settles hover and focus over two), and every frame while a key is held.
    int framesToDraw = 3;
    int snapshotFrames = 5;   // ImGui sizes its windows over the first frames
    bool running = true;
    while (running) {
        SDL_Event e;
        bool got = framesToDraw > 0 || app.animating() ? SDL_PollEvent(&e) : SDL_WaitEventTimeout(&e, 500);
        while (got) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT ||
                (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && e.window.windowID == SDL_GetWindowID(window)))
                running = false;
            app.handleEvent(e);
            framesToDraw = 3;
            got = SDL_PollEvent(&e);
        }
        if (framesToDraw == 0 && !app.animating()) continue;
        if (framesToDraw > 0) --framesToDraw;
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        app.frame();
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColorFloat(renderer, 0.11f, 0.11f, 0.11f, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        if (!snapshot.empty() && --snapshotFrames == 0) {
            SDL_Surface* shot = SDL_RenderReadPixels(renderer, nullptr);
            const bool saved = shot && SDL_SavePNG(shot, snapshot.c_str());
            if (!saved) std::fprintf(stderr, "snapshot: %s\n", SDL_GetError());
            SDL_DestroySurface(shot);
            running = false;
        }
        if (!snapshot.empty()) framesToDraw = 1;
        if (!snapshot.empty() && clickX >= 0 && snapshotFrames == 3) {
            // A synthetic press, through the same path as a real one.
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

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
