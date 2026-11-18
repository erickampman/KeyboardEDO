#pragma once

// Fingers on a Mac's touchscreen (ReleaseUMP R2-29, MILESTONES M9). SDL 3.4's
// Cocoa backend skips direct touches (its handleTouches has a TODO for them),
// and macOS hands them only to a gesture recognizer, as NSEventTypeDirectTouch,
// never to the view's touchesBegan. So this adds a recognizer to SDL's view
// and pushes each touch as an SDL finger event, with kTouchID as its device,
// for handleEvent's finger code (decisions.md, "Touch").
//
// With the recognizer there, macOS makes no clicks of its own from those
// touches (it does on a plain view), so the keyboard doesn't hear them twice.
// When macOS takes touches for a gesture of its own, or the app loses the
// screen, every finger still down is cancelled.
//
// Elsewhere, attach() does nothing.

#include <SDL3/SDL.h>

namespace kedo::app::mac {

// The device of the fingers attach() pushes: not one SDL knows.
inline constexpr SDL_TouchID kTouchID = 0x6b65646f746368ULL;   // "kedotch"

#ifdef __APPLE__
void attach(SDL_Window* window);
#else
inline void attach(SDL_Window*) {}
#endif

}   // namespace kedo::app::mac
