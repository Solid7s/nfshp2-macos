// The Win32 window, the keyboard and the gamepads. The renderer draws to the window itself (native_vk.cpp).
#include "hw.h"
#include "pcconfig.h"
#include "host.h"
#include "pcmenu.h"
#include "pad.h"
#include "native.h"
#include <array>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <thread>
#include <vector>

static HWND g_hwnd;
static bool g_fullscreen;               // borderless, on the window's monitor
static RECT g_windowed_rect;
static std::atomic<bool> g_want_fullscreen;
static const UINT WM_SETFULLSCREEN = WM_APP + 1;
static const UINT WM_SETWINDOWSIZE = WM_APP + 2;
static const UINT WM_CURSORSTATE = WM_APP + 3;
// set when a setting changed from the keyboard (Alt+Enter): the menu shows the new value
std::atomic<bool> g_pc_ui_dirty;

// borderless fullscreen on the monitor the window is on, and back; on the window's thread
static void set_fullscreen_now(bool fs)
{
    if (!g_hwnd || fs == g_fullscreen) return;
    if (fs) {
        GetWindowRect(g_hwnd, &g_windowed_rect);
        MONITORINFO mi = {sizeof mi};
        GetMonitorInfoA(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLongA(g_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(g_hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        SetWindowLongA(g_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(g_hwnd, HWND_NOTOPMOST, g_windowed_rect.left, g_windowed_rect.top,
                     g_windowed_rect.right - g_windowed_rect.left, g_windowed_rect.bottom - g_windowed_rect.top,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    g_fullscreen = fs;
}

// from any thread; the setting is saved, and the menu told
void window_set_fullscreen(bool fs)
{
    g_cfg.fullscreen = fs;
    pc_config_save();
    g_pc_ui_dirty = true;
    if (g_hwnd) PostMessageA(g_hwnd, WM_SETFULLSCREEN, fs, 0);
}

void window_set_size(int w, int h)
{
    g_cfg.win_w = w;
    g_cfg.win_h = h;
    pc_config_save();
    if (g_hwnd) PostMessageA(g_hwnd, WM_SETWINDOWSIZE, 0, 0);
}

void *window_handle() { return g_hwnd; }

void window_screen_size(int &w, int &h)
{
    MONITORINFO mi = {sizeof mi};
    HMONITOR mon = g_hwnd ? MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST) : MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    if (GetMonitorInfoA(mon, &mi)) {
        w = mi.rcMonitor.right - mi.rcMonitor.left;
        h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    } else {
        w = GetSystemMetrics(SM_CXSCREEN);
        h = GetSystemMetrics(SM_CYSCREEN);
    }
}

void window_view_size(int &w, int &h)
{
    if (g_cfg.fullscreen) {
        window_screen_size(w, h);
    } else {
        w = g_cfg.win_w;
        h = g_cfg.win_h;
    }
}

void window_relaunch(const std::string &profile)
{
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    SetEnvironmentVariableA("HP2_PROFILE", profile.c_str());
    // The new one starts as from the desktop: no standard handles (this one's are the log's pipe, whose numbers mean nothing there), and
    // speed.log free for it to open.
    host_log_close();
    STARTUPINFOA si = {};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi = {};
    if (CreateProcessA(path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        ExitProcess(0);
    }
    SetEnvironmentVariableA("HP2_PROFILE", nullptr);
}

void window_quit()
{
    if (g_hwnd) PostMessageA(g_hwnd, WM_CLOSE, 0, 0);
    else ExitProcess(0);
}

// the pointer shows in the menus; data\ui\cursor.cur or cursor.ani (beside the program, or under HP2_DATA) replaces the arrow
static std::atomic<bool> g_cursor_on{false};    // not till a screen asks for it and the mouse moves (window_pad)
static HCURSOR menu_cursor()
{
    static HCURSOR c = [] {
        char path[MAX_PATH];
        DWORD n;
        if (const char *data = getenv("HP2_DATA")) {
            n = (DWORD)snprintf(path, MAX_PATH, "%s\\", data);
        } else {
            n = GetModuleFileNameA(nullptr, path, MAX_PATH);
            while (n && path[n - 1] != '\\' && path[n - 1] != '/') n--;
        }
        for (const char *name : {"data\\ui\\cursor.cur", "data\\ui\\cursor.ani"}) {
            snprintf(path + n, MAX_PATH - n, "%s", name);
            if (HCURSOR own = LoadCursorFromFileA(path)) return own;
        }
        return LoadCursor(nullptr, IDC_ARROW);
    }();
    return c;
}

// Hidden over all of the window, the title bar too, while it is in front; SetCursor alone covers the client area and only from the next message.
static void apply_cursor(HWND h)
{
    static bool hidden;
    const bool hide = !g_cursor_on && GetForegroundWindow() == h;
    if (hide != hidden) {
        const int count = ShowCursor(hide ? FALSE : TRUE);
        hidden = hide;
        if (getenv("HP2_NATIVELOG")) fprintf(stderr, "cursor: %s, count %d\n", hide ? "hidden" : "shown", count);
    }
    SetCursor(g_cursor_on ? menu_cursor() : nullptr);
}

// Platform observations consumed by the shared input mappings.
static bool window_key_down(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
static bool window_focused() { return GetForegroundWindow() == g_hwnd; }
static bool window_caps_lock() { return (GetKeyState(VK_CAPITAL) & 1) != 0; }
static void window_cursor_changed() { PostMessageA(g_hwnd, WM_CURSORSTATE, 0, 0); }
static bool window_mouse_client(float &x, float &y, int &w, int &h, bool &left)
{
    POINT p;
    RECT r;
    if (!GetCursorPos(&p) || !ScreenToClient(g_hwnd, &p) || !GetClientRect(g_hwnd, &r)) return false;
    x = (float)p.x; y = (float)p.y; w = r.right; h = r.bottom;
    left = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    return true;
}
#include "window_input.inc"

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    // Any new press on the keyboard or the mouse says the player has moved to them (a repeat says nothing, nor do the system's Alt and Win).
    case WM_KEYDOWN:
        if (!(lp & 1 << 30) && wp != VK_LWIN && wp != VK_RWIN) g_input_kind = 0, g_mouse_at = 0;
        if (g_text_on)
            if (const int ch = text_char((int)wp, (GetKeyState(VK_SHIFT) & 0x8000) != 0))
                if ((ch != 13 || g_text_enter_free) && (ch != 27 || g_text_esc_free)) text_push(ch);
        if (!g_text_on && g_text_armed && !(lp & 1 << 30))
            if (const int ch = text_char((int)wp, (GetKeyState(VK_SHIFT) & 0x8000) != 0))
                if (ch >= 32 && ch < 127) text_push(ch);
        break;
    case WM_KEYUP:
        if (wp == VK_RETURN) g_text_enter_free = true;
        if (wp == VK_ESCAPE) g_text_esc_free = true;
        break;
    case WM_MOUSEMOVE: {
        // Windows sends one when a window comes up under a pointer that stands still: only a new place is a move
        static LPARAM last = -1;
        if (lp != last) {
            if (last != -1) mouse_used();
            last = lp;
        }
        break;
    }
    case WM_MOUSEWHEEL:
        g_input_kind = 0;
        mouse_used();
        g_wheel += GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
        if (g_wheel > 4) g_wheel = 4;
        if (g_wheel < -4) g_wheel = -4;
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        g_input_kind = 0;
        mouse_used();
        break;
    case WM_SYSKEYDOWN:
        if (wp == VK_RETURN && !(lp & 1 << 30)) {                   // Alt+Enter
            window_set_fullscreen(!g_cfg.fullscreen);
            return 0;
        }
        break;
    case WM_SETWINDOWSIZE:
        if (!g_cfg.fullscreen) {
            RECT r = {0, 0, g_cfg.win_w, g_cfg.win_h};
            AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
            SetWindowPos(h, nullptr, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
        }
        return 0;
    case WM_SETFULLSCREEN:
        set_fullscreen_now(wp != 0);
        return 0;
    case WM_CURSORSTATE:
        apply_cursor(h);
        return 0;
    case WM_ACTIVATE:
        apply_cursor(h);
        pad_rumble_quiet(LOWORD(wp) == WA_INACTIVE);    // a pad does not shake for a game behind another window
        break;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            apply_cursor(h);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        window_rumble(0, 0);            // a pad's motors outlive the program
        pad_settle(150);                // the pads' thread tells them
        host_log_flush();
        ExitProcess(0);
    }
    return DefWindowProcA(h, m, wp, lp);
}

// The process takes the screen's real pixels, whatever the screen's scaling: the window's client size is then the swapchain's on every
// driver, and the fullscreen window is the monitor's own size. Said here and not left to what else is in the process (a driver, an overlay,
// the system's compatibility settings): a window Windows scales by itself is drawn at another size than it has, and each driver takes that
// its own way. The functions by name: they are Windows 10's.
int window_dpi_aware()
{
    HMODULE user = GetModuleHandleA("user32.dll");
    typedef void *(WINAPI *GetContext)();
    typedef int (WINAPI *FromContext)(void *);
    typedef BOOL (WINAPI *SetContext)(void *);
    const auto get = (GetContext)(void *)GetProcAddress(user, "GetThreadDpiAwarenessContext");
    const auto from = (FromContext)(void *)GetProcAddress(user, "GetAwarenessFromDpiAwarenessContext");
    const auto set = (SetContext)(void *)GetProcAddress(user, "SetProcessDpiAwarenessContext");
    const int was = get && from ? from(get()) : -1;
    if (!set || !set((void *)(INT_PTR)-4))                  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
        if (was < 1) SetProcessDPIAware();
    return was;
}

// HP2_DPI=0 | 1, for tests: the window's thread and the renderer's as in a process Windows scales (unaware, or aware of the system's scaling
// only), which is what the port was on a scaled screen before it declared itself. Called at the start of each of the two threads.
void window_dpi_thread()
{
    const char *e = getenv("HP2_DPI");
    if (!e) return;
    typedef void *(WINAPI *SetThreadContext)(void *);
    if (const auto set = (SetThreadContext)(void *)GetProcAddress(GetModuleHandleA("user32.dll"), "SetThreadDpiAwarenessContext"))
        set((void *)(INT_PTR)(atoi(e) == 1 ? -2 : -1));
}

void window_start()
{
    std::thread([] {
        window_dpi_thread();
        if (getenv("HP2_CURSORSTART")) g_cursor_on = true, mouse_used();    // for tests: as if the mouse had just been used on a screen before
        WNDCLASSA wc = {};
        wc.lpfnWndProc = wndproc;
        wc.hInstance = GetModuleHandleA(nullptr);
        wc.lpszClassName = "hp2recomp";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(1));    // speed.rc
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        RegisterClassA(&wc);
        RECT r = {0, 0, g_cfg.win_w, g_cfg.win_h};
        AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
        g_hwnd = CreateWindowA("hp2recomp", "Need for Speed: Hot Pursuit 2", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                               CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, nullptr,
                               nullptr, wc.hInstance, nullptr);
        if (g_cfg.fullscreen) set_fullscreen_now(true);
        MSG msg;
        while (GetMessageA(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }).detach();
}
