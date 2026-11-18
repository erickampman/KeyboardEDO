// Fingers on a Mac's touchscreen, pushed as SDL finger events (MacTouch.h).

#include "MacTouch.h"

#import <AppKit/AppKit.h>

#include <cstdio>
#include <cstdlib>
#include <set>

namespace {

std::set<SDL_FingerID> gDown;   // fingers pushed down and not yet up
bool gLog = false;              // KEDO_TOUCH_LOG=1: what arrives, on stderr

SDL_WindowID gWindow = 0;

void pushFinger(SDL_EventType type, SDL_FingerID finger, float x, float y) {
    SDL_Event e{};
    e.type              = type;
    e.tfinger.timestamp = SDL_GetTicksNS();
    e.tfinger.touchID   = kedo::app::mac::kTouchID;
    e.tfinger.fingerID  = finger;
    e.tfinger.x         = x;
    e.tfinger.y         = y;
    e.tfinger.pressure  = 1.0f;
    e.tfinger.windowID  = gWindow;
    if (gLog)
        std::fprintf(stderr, "touch %s finger %llx at %.3f %.3f\n",
                     type == SDL_EVENT_FINGER_DOWN     ? "down"
                     : type == SDL_EVENT_FINGER_UP     ? "up"
                     : type == SDL_EVENT_FINGER_MOTION ? "move"
                                                       : "cancel",
                     (unsigned long long)finger, x, y);
    SDL_PushEvent(&e);
}

// Every finger still down, cancelled: macOS has taken the touches (a gesture
// of its own) or the app has lost the screen, and no ends will come.
void cancelAll() {
    for (SDL_FingerID f : gDown) pushFinger(SDL_EVENT_FINGER_CANCELED, f, 0, 0);
    gDown.clear();
}

}   // namespace

@interface KedoTouchRecognizer : NSGestureRecognizer
@end

@implementation KedoTouchRecognizer

- (void)push:(NSEvent*)event phase:(NSTouchPhase)phase type:(SDL_EventType)type {
    NSView* view = self.view;
    const NSSize size = view.bounds.size;
    if (size.width <= 0 || size.height <= 0) return;
    for (NSTouch* touch in [event touchesMatchingPhase:phase inView:view]) {
        if (touch.type != NSTouchTypeDirect) continue;
        const SDL_FingerID finger = SDL_FingerID(uintptr_t(touch.identity));
        if (type == SDL_EVENT_FINGER_DOWN) gDown.insert(finger);
        else if (!gDown.count(finger)) continue;   // cancelled already, or never seen down
        else if (type != SDL_EVENT_FINGER_MOTION) gDown.erase(finger);
        const NSPoint p = [touch locationInView:view];
        pushFinger(type, finger, float(p.x / size.width),
                   float(view.isFlipped ? p.y / size.height : 1.0 - p.y / size.height));
    }
}

// Never stopped by another recognizer, the system's or the window's.
- (BOOL)canBePreventedByGestureRecognizer:(NSGestureRecognizer*)other {
    return NO;
}

// Called when the recognizer is made to start over: its touches are gone.
- (void)reset {
    [super reset];
    cancelAll();
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
    [self push:event phase:NSTouchPhaseBegan type:SDL_EVENT_FINGER_DOWN];
}
- (void)touchesMovedWithEvent:(NSEvent*)event {
    [self push:event phase:NSTouchPhaseMoved type:SDL_EVENT_FINGER_MOTION];
}
- (void)touchesEndedWithEvent:(NSEvent*)event {
    [self push:event phase:NSTouchPhaseEnded type:SDL_EVENT_FINGER_UP];
}
- (void)touchesCancelledWithEvent:(NSEvent*)event {
    [self push:event phase:NSTouchPhaseCancelled type:SDL_EVENT_FINGER_CANCELED];
}

@end

namespace kedo::app::mac {

void attach(SDL_Window* window) {
    gLog = std::getenv("KEDO_TOUCH_LOG") != nullptr;
    NSWindow* win = (__bridge NSWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                                               SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
    NSView* view = win.contentView;
    if (!view) return;

    gWindow = SDL_GetWindowID(window);
    KedoTouchRecognizer* recognizer = [[KedoTouchRecognizer alloc] initWithTarget:nil action:nil];
    recognizer.allowedTouchTypes = NSTouchTypeMaskDirect;
    [view addGestureRecognizer:recognizer];

    // Switching away mid-touch: those touches end elsewhere.
    for (NSNotificationName n : {NSApplicationDidResignActiveNotification, NSWindowDidResignKeyNotification})
        [[NSNotificationCenter defaultCenter] addObserverForName:n
                                                          object:nil
                                                           queue:nil
                                                      usingBlock:^(NSNotification*) { cancelAll(); }];
}

}   // namespace kedo::app::mac
