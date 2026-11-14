// Cocoa owns the main thread; the recompiled game and renderer run on workers.
#include "platform_compat.h"
#include "hw.h"
#include "pcconfig.h"
#include "host.h"
#include "pcmenu.h"
#include "pad.h"
#include "native.h"
#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include "macos_shortcuts.h"
#include "macos_setup.h"
#include <vulkan/vulkan.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <pthread.h>
#include <spawn.h>
#include <unistd.h>

// Keep the existing configuration files' virtual-key numbering on every host.
enum {
    VK_LBUTTON=1, VK_RBUTTON=2, VK_MBUTTON=4, VK_BACK=8, VK_RETURN=13,
    VK_SHIFT=16, VK_CONTROL=17, VK_MENU=18, VK_ESCAPE=27, VK_SPACE=32,
    VK_END=35, VK_HOME=36, VK_LEFT=37, VK_RIGHT=39, VK_DELETE=46,
    VK_NUMPAD0=96, VK_NUMPAD9=105, VK_DECIMAL=110, VK_F9=120,
    VK_F10=121, VK_F11=122, VK_OEM_PERIOD=190
};
static std::atomic<SDL_Window *> g_hwnd;
static std::atomic<bool> g_focused, g_caps_lock, g_cursor_on, g_quit, g_finished;
static std::atomic<int> g_exit_code;
static std::atomic<bool> g_keys[256];
static std::atomic<int> g_pixel_w{960}, g_pixel_h{720}, g_client_w{960}, g_client_h{720};
static std::atomic<int> g_screen_w{1920}, g_screen_h{1080};
static std::atomic<float> g_mouse_x, g_mouse_y;
static std::atomic<bool> g_mouse_left;
static MacFullscreenShortcut g_shortcuts; // main thread only
std::atomic<bool> g_pc_ui_dirty;

static bool window_key_down(int vk) { return vk > 0 && vk < 256 && g_keys[vk]; }
static bool window_focused() { return g_focused; }
static bool window_caps_lock() { return g_caps_lock; }
static void window_cursor_changed()
{
    SDL_RunOnMainThread([](void *) {
        if (g_cursor_on || !g_focused) SDL_ShowCursor(); else SDL_HideCursor();
    }, nullptr, false);
}
static bool window_mouse_client(float &x, float &y, int &w, int &h, bool &left)
{
    x = g_mouse_x; y = g_mouse_y; w = g_client_w; h = g_client_h; left = g_mouse_left;
    return g_focused;
}
#include "window_input.inc"

static void remember_size()
{
    if (!g_hwnd) return;
    int w, h;
    SDL_GetWindowSize(g_hwnd, &w, &h); g_client_w = w; g_client_h = h;
    SDL_GetWindowSizeInPixels(g_hwnd, &w, &h); g_pixel_w = w; g_pixel_h = h;
    const auto *mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(g_hwnd));
    if (mode) { g_screen_w = mode->w; g_screen_h = mode->h; }
}

void *window_handle() { return g_hwnd; }
void window_drawable_size(int &w, int &h) { w = g_pixel_w; h = g_pixel_h; }
void window_screen_size(int &w, int &h) { w = g_screen_w; h = g_screen_h; }
void window_view_size(int &w, int &h)
{
    if (g_cfg.fullscreen) window_screen_size(w, h);
    else { w = g_cfg.win_w; h = g_cfg.win_h; }
}
int window_dpi_aware() { return 2; }
void window_dpi_thread() {}
void window_set_fullscreen(bool fs)
{
    g_cfg.fullscreen = fs;
    pc_config_save(); g_pc_ui_dirty = true;
    SDL_RunOnMainThread([](void *p) {
        if (g_hwnd) SDL_SetWindowFullscreen(g_hwnd, (intptr_t)p != 0);
        remember_size();
    }, (void *)(intptr_t)fs, true);
}
void window_set_size(int w, int h)
{
    g_cfg.win_w = w; g_cfg.win_h = h; pc_config_save();
    SDL_RunOnMainThread([](void *) {
        if (g_hwnd && !g_cfg.fullscreen) SDL_SetWindowSize(g_hwnd, g_cfg.win_w, g_cfg.win_h);
        remember_size();
    }, nullptr, true);
}
void window_quit() { g_quit = true; }
void window_relaunch(const std::string &profile)
{
    extern char **environ;
    const std::string exe = host_exe_dir() + "speed";
    setenv("HP2_PROFILE", profile.c_str(), 1);
    pid_t pid;
    char *args[] = {const_cast<char *>(exe.c_str()), nullptr};
    host_log_close();
    if (posix_spawn(&pid, exe.c_str(), nullptr, nullptr, args, environ) == 0) window_quit();
    else host_fatal("Could not restart the game for the selected profile.");
}
void window_start()
{
    if (!SDL_RunOnMainThread([](void *) {
        g_hwnd = SDL_CreateWindow("Need for Speed: Hot Pursuit 2", g_cfg.win_w, g_cfg.win_h,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        if (!g_hwnd) { host_fatal(SDL_GetError()); g_exit_code = 1; g_quit = true; return; }
        if (g_cfg.fullscreen) SDL_SetWindowFullscreen(g_hwnd, true);
        g_focused = (SDL_GetWindowFlags(g_hwnd) & SDL_WINDOW_INPUT_FOCUS) != 0;
        remember_size();
        if (getenv("HP2_CURSORSTART")) { g_cursor_on = true; mouse_used(); }
        window_cursor_changed();
    }, nullptr, true)) { host_fatal(SDL_GetError()); std::_Exit(1); }
    if (!g_hwnd) std::_Exit(1);
}

bool window_vulkan_surface(VkInstance instance, VkSurfaceKHR *surface)
{
    struct Request { VkInstance instance; VkSurfaceKHR *surface; bool ok; } r{instance, surface, false};
    if (!SDL_RunOnMainThread([](void *p) {
        auto &r = *static_cast<Request *>(p);
        r.ok = SDL_Vulkan_CreateSurface(g_hwnd, r.instance, nullptr, r.surface);
        if (!r.ok) fprintf(stderr, "Vulkan surface: %s\n", SDL_GetError());
    }, &r, true)) return false;
    return r.ok;
}

static int virtual_key(SDL_Scancode sc)
{
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return 'A' + sc - SDL_SCANCODE_A;
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return '1' + sc - SDL_SCANCODE_1;
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return 0x70 + sc - SDL_SCANCODE_F1;
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9) return 0x61 + sc - SDL_SCANCODE_KP_1;
    switch (sc) {
    case SDL_SCANCODE_0: return '0';
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 13;
    case SDL_SCANCODE_ESCAPE: return 27;
    case SDL_SCANCODE_BACKSPACE: return 8;
    case SDL_SCANCODE_TAB: return 9;
    case SDL_SCANCODE_SPACE: return 32;
    case SDL_SCANCODE_CAPSLOCK: return 20;
    case SDL_SCANCODE_PAGEUP: return 33; case SDL_SCANCODE_PAGEDOWN: return 34;
    case SDL_SCANCODE_END: return 35; case SDL_SCANCODE_HOME: return 36;
    case SDL_SCANCODE_LEFT: return 37; case SDL_SCANCODE_UP: return 38;
    case SDL_SCANCODE_RIGHT: return 39; case SDL_SCANCODE_DOWN: return 40;
    case SDL_SCANCODE_INSERT: return 45; case SDL_SCANCODE_DELETE: return 46;
    case SDL_SCANCODE_KP_0: return 96; case SDL_SCANCODE_KP_PERIOD: return 110;
    case SDL_SCANCODE_KP_MULTIPLY: return 106; case SDL_SCANCODE_KP_PLUS: return 107;
    case SDL_SCANCODE_KP_MINUS: return 109; case SDL_SCANCODE_KP_DIVIDE: return 111;
    case SDL_SCANCODE_LSHIFT: return 160; case SDL_SCANCODE_RSHIFT: return 161;
    case SDL_SCANCODE_LCTRL: return 162; case SDL_SCANCODE_RCTRL: return 163;
    case SDL_SCANCODE_LALT: return 164; case SDL_SCANCODE_RALT: return 165;
    case SDL_SCANCODE_LGUI: return 91; case SDL_SCANCODE_RGUI: return 92;
    case SDL_SCANCODE_SEMICOLON: return 186; case SDL_SCANCODE_EQUALS: return 187;
    case SDL_SCANCODE_COMMA: return 188; case SDL_SCANCODE_MINUS: return 189;
    case SDL_SCANCODE_PERIOD: return 190; case SDL_SCANCODE_SLASH: return 191;
    case SDL_SCANCODE_GRAVE: return 192; case SDL_SCANCODE_LEFTBRACKET: return 219;
    case SDL_SCANCODE_BACKSLASH: return 220; case SDL_SCANCODE_RIGHTBRACKET: return 221;
    case SDL_SCANCODE_APOSTROPHE: return 222;
    default: return 0;
    }
}

static void event(const SDL_Event &e)
{
    switch (e.type) {
    case SDL_EVENT_QUIT: case SDL_EVENT_WINDOW_CLOSE_REQUESTED: g_quit = true; break;
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_DISPLAY_CHANGED: remember_size(); break;
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        g_focused = true; pad_rumble_quiet(false); window_cursor_changed(); break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        g_focused = false; for (auto &k : g_keys) k = false;
        g_shortcuts.reset();
        g_mouse_left = false; g_text_enter_free = g_text_esc_free = true; pad_rumble_quiet(true); window_cursor_changed(); break;
    case SDL_EVENT_KEY_DOWN: case SDL_EVENT_KEY_UP: {
        const int vk = virtual_key(e.key.scancode);
        const bool down = e.key.down;
        g_keys[16] = (e.key.mod & SDL_KMOD_SHIFT) != 0;
        g_keys[17] = (e.key.mod & SDL_KMOD_CTRL) != 0;
        g_keys[18] = (e.key.mod & SDL_KMOD_ALT) != 0;
        g_caps_lock = (e.key.mod & SDL_KMOD_CAPS) != 0;
        bool toggle;
        if (g_shortcuts.consume(e.key, toggle)) {
            if (vk) g_keys[vk] = false;
            if (toggle) window_set_fullscreen(!g_cfg.fullscreen);
            break;
        }
        if (vk) g_keys[vk] = down;
        if (!down) {
            if (vk == VK_RETURN) g_text_enter_free = true;
            if (vk == VK_ESCAPE) g_text_esc_free = true;
            break;
        }
        if (!e.key.repeat) { g_input_kind = 0; g_mouse_at = 0; }
        const int ch = text_char(vk, (e.key.mod & SDL_KMOD_SHIFT) != 0);
        if (g_text_on && ch && (ch != 13 || g_text_enter_free) && (ch != 27 || g_text_esc_free)) text_push(ch);
        if (!g_text_on && g_text_armed && !e.key.repeat && ch >= 32 && ch < 127) text_push(ch);
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        g_mouse_x = e.motion.x; g_mouse_y = e.motion.y;
        if (e.motion.xrel || e.motion.yrel) mouse_used(); break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) g_mouse_left = e.button.down;
        g_input_kind = 0; mouse_used(); break;
    case SDL_EVENT_MOUSE_WHEEL:
        g_input_kind = 0; mouse_used();
        if (e.wheel.y) g_wheel = std::clamp(g_wheel.load() + (e.wheel.y > 0 ? 1 : -1), -4, 4); break;
    }
}

int hp2_game_main();
int main(int argc, char **argv)
{
    SDL_SetMainReady();
    const auto contents = std::filesystem::path(host_exe_dir()).parent_path().parent_path();
    const bool bundled = std::filesystem::is_regular_file(contents / "Info.plist");
    if (argc == 3 && !strcmp(argv[1], "--data")) setenv("HP2_DATA", argv[2], 1);
    else if (argc > 1) {
        fprintf(stderr, "Usage: speed [--data <game folder>]\n"); return 2;
    }
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    if (!getenv("HP2_DATA") && bundled) {
        std::string data;
        if (!mac_setup::choose_game(data, contents / "Resources/game-location.txt")) { SDL_Quit(); return 0; }
        setenv("HP2_DATA", data.c_str(), 1);
    }
    if (!getenv("HP2_DATA")) {
        // Loose executable: data beside it.
        const auto base = host_exe_dir();
        if (mac_setup::game_folder(base)) setenv("HP2_DATA", base.c_str(), 1);
    }
    if (!getenv("HP2_DATA") || !mac_setup::game_folder(getenv("HP2_DATA"))) {
        const char *message = "The imported game folder is missing or incomplete. Choose an installed game folder with --data, or open the app to import your PS2 ISO.";
        fprintf(stderr, "%s\n", message);
        if (bundled) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Need for Speed: Hot Pursuit 2", message, nullptr);
        SDL_Quit(); return 1;
    }
    if (const char *data = getenv("HP2_DATA")) {
        const auto absolute = std::filesystem::absolute(data).string();
        setenv("HP2_DATA", absolute.c_str(), 1);
        if (chdir(absolute.c_str())) {
            perror("game folder");
            if (bundled) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Need for Speed: Hot Pursuit 2",
                "The imported game folder could not be opened. Check that the folder is accessible to your account.", nullptr);
            return 1;
        }
    }
    if (bundled && !getenv("HP2_STDERR") && pc_config_log()) {
        std::error_code ec;
        std::filesystem::rename("speed.log", "speed_prev.log", ec);
        if (freopen("speed.log", "w", stderr)) setvbuf(stderr, nullptr, _IONBF, 0);
    }
    const auto local_vulkan = contents / "Frameworks/libMoltenVK.dylib";
    const std::string vulkan = std::filesystem::exists(local_vulkan) ? local_vulkan.string() : HP2_MOLTENVK_LIBRARY;
    if (!SDL_Vulkan_LoadLibrary(vulkan.c_str())) {
        fprintf(stderr, "Vulkan: %s\n", SDL_GetError());
        if (bundled) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Need for Speed: Hot Pursuit 2", SDL_GetError(), nullptr);
        return 1;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 32 * 1024 * 1024);
    pthread_t worker;
    const int err = pthread_create(&worker, &attr, [](void *) -> void * {
        g_exit_code = hp2_game_main(); g_finished = true; return nullptr;
    }, nullptr);
    pthread_attr_destroy(&attr);
    if (err) { fprintf(stderr, "game thread: %s\n", strerror(err)); return 1; }
    const uint64_t started = SDL_GetTicks();
    const char *limit = getenv("HP2_RUN_SECONDS");
    while (!g_quit && !g_finished) {
        SDL_Event e;
        if (SDL_WaitEventTimeout(&e, 8)) { event(e); while (SDL_PollEvent(&e)) event(e); }
        if (limit && SDL_GetTicks() - started >= atof(limit) * 1000) g_quit = true;
    }
    pad_rumble_quiet(true); window_rumble(0, 0); pad_settle(150);
    host_log_flush(); fflush(nullptr);
    // The upstream runtime has detached, lifelong CPU/audio/render threads.
    // Avoid destroying C++ globals while those threads are still using them.
    std::_Exit(g_exit_code);
}
