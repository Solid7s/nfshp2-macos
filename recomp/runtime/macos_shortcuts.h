#pragma once
#include <SDL3/SDL.h>
#include <array>

// Keep a shortcut's key consumed until release, even if Command/Option is
// released first. F is also a driving action and must not reach the game here.
class MacFullscreenShortcut {
    std::array<bool, SDL_SCANCODE_COUNT> held{};
public:
    void reset() { held.fill(false); }
    bool consume(const SDL_KeyboardEvent &key, bool &toggle)
    {
        toggle = false;
        if (key.scancode <= SDL_SCANCODE_UNKNOWN || key.scancode >= SDL_SCANCODE_COUNT) return false;
        const bool chord = (key.scancode == SDL_SCANCODE_F && (key.mod & SDL_KMOD_GUI)) ||
            ((key.scancode == SDL_SCANCODE_RETURN || key.scancode == SDL_SCANCODE_KP_ENTER) && (key.mod & SDL_KMOD_ALT));
        bool &captured = held[key.scancode];
        if (key.down && chord) {
            toggle = !key.repeat && !captured;
            captured = true;
        }
        const bool consumed = captured;
        if (!key.down) captured = false;
        return consumed;
    }
};
