// Test the actual gamepad refresh against SDL's shared queue. No window or disc.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include "../../recomp/runtime/pad.cpp"
#include "macos_shortcuts.h"

// pcconfig's display-size dependency; these checks do not use display settings.
void window_view_size(int &w, int &h) { w = 960; h = 720; }

static void require(bool ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL: %s (%s)\n", what, SDL_GetError()); std::exit(1); }
}

static SDL_KeyboardEvent keyboard(SDL_Scancode sc, bool down, SDL_Keymod mod = SDL_KMOD_NONE, bool repeat = false)
{
    SDL_KeyboardEvent e{};
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.scancode = sc; e.down = down; e.mod = mod; e.repeat = repeat;
    return e;
}

int main()
{
    SDL_SetMainReady();
    require(SDL_Init(SDL_INIT_GAMEPAD), "initialize SDL gamepads");
    // Exercise presses, releases, focus loss and close events pending while the
    // polling worker refreshes devices. A lost Right release leaves steering held.
    const SDL_KeyboardEvent keys[] = {keyboard(SDL_SCANCODE_RIGHT, true), keyboard(SDL_SCANCODE_RIGHT, false),
                                    keyboard(SDL_SCANCODE_LEFT, true), keyboard(SDL_SCANCODE_LEFT, false)};
    for (int pass = 0; pass < 100; ++pass) {
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST); // test owns this queue
        for (auto k : keys) { SDL_Event e{}; e.key = k; require(SDL_PushEvent(&e), "queue steering key"); }
        for (auto type : {SDL_EVENT_WINDOW_FOCUS_LOST, SDL_EVENT_WINDOW_CLOSE_REQUESTED}) {
            SDL_Event e{}; e.type = type; require(SDL_PushEvent(&e), "queue window event");
        }
        // The worker is deliberately scheduled between enqueue and dispatch.
        std::thread([] { refresh(); }).join();
        SDL_Event e[16]{};
        int n = SDL_PeepEvents(e, 16, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
        require(n >= 6, "gamepad polling must preserve keyboard and window events");
        for (int i = 0; i < 4; ++i)
            require(e[i].type == keys[i].type && e[i].key.scancode == keys[i].scancode,
                    "steering press/release ordering");
        require(e[4].type == SDL_EVENT_WINDOW_FOCUS_LOST && e[5].type == SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                "focus and close events survive polling");
    }
    MacFullscreenShortcut shortcut;
    bool toggle;
    require(!shortcut.consume(keyboard(SDL_SCANCODE_F, true), toggle) && !toggle, "plain F remains a game key");
    shortcut.consume(keyboard(SDL_SCANCODE_F, false), toggle);
    for (auto cmd : {SDL_KMOD_LGUI, SDL_KMOD_RGUI}) {
        require(shortcut.consume(keyboard(SDL_SCANCODE_F, true, cmd), toggle) && toggle, "Command+F toggles fullscreen");
        require(shortcut.consume(keyboard(SDL_SCANCODE_F, true, cmd, true), toggle) && !toggle, "key repeat must not retoggle");
        require(shortcut.consume(keyboard(SDL_SCANCODE_F, true, SDL_KMOD_NONE, true), toggle) && !toggle,
                "releasing Command first must not leak F into driving");
        require(shortcut.consume(keyboard(SDL_SCANCODE_F, false), toggle) && !toggle, "shortcut release is consumed");
        require(!shortcut.consume(keyboard(SDL_SCANCODE_F, true), toggle), "F works again after shortcut release");
        shortcut.consume(keyboard(SDL_SCANCODE_F, false), toggle);
    }
    for (auto sc : {SDL_SCANCODE_RETURN, SDL_SCANCODE_KP_ENTER}) {
        require(shortcut.consume(keyboard(sc, true, SDL_KMOD_ALT), toggle) && toggle, "Option+Enter remains available");
        require(shortcut.consume(keyboard(sc, false), toggle), "Option+Enter release is consumed");
    }
    shortcut.consume(keyboard(SDL_SCANCODE_F, true, SDL_KMOD_GUI), toggle);
    shortcut.reset();
    require(!shortcut.consume(keyboard(SDL_SCANCODE_F, true), toggle), "focus loss resets shortcut capture");
    SDL_Quit();
    puts("PASS: 100 controller-refresh/steering sequences, focus/close events, and fullscreen shortcuts");
}
