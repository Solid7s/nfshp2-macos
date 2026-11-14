// Manual/automated GUI smoke checks, separate from the shipped executable.
#import <AppKit/AppKit.h>
#include "macos_setup.h"
#include <SDL3/SDL.h>
#include <cstdio>

static NSButton *find_button(NSView *view, NSString *title)
{
    if ([view isKindOfClass:[NSButton class]] && [((NSButton *)view).title isEqualToString:title]) return (NSButton *)view;
    for (NSView *child in view.subviews) if (NSButton *button = find_button(child, title)) return button;
    return nil;
}
int main(int argc, char **argv)
{
    @autoreleasepool {
        if (argc < 3) { fprintf(stderr, "Usage: setup-probe {choose-quit|choose-cancel|choose-q|remembered} <isolated support folder> | {import|cancel|invalid} <iso> <new destination>\n"); return 2; }
        if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
        const std::string mode = argv[1];
        __block int stage = 0;
        __block bool captured = false;
        NSTimer *timer = [NSTimer timerWithTimeInterval:0.3 repeats:YES block:^(NSTimer *) {
            if (!captured && getenv("HP2_SETUP_SHOT")) for (NSWindow *window in NSApp.windows) {
                if (!window.visible || !window.contentView) continue;
                NSView *view = window.contentView;
                NSBitmapImageRep *bitmap = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
                [view cacheDisplayInRect:view.bounds toBitmapImageRep:bitmap];
                [[bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}]
                    writeToFile:[NSString stringWithUTF8String:getenv("HP2_SETUP_SHOT")] atomically:YES];
                captured = true;
                break;
            }
            if (mode == "choose-q" && NSApp.modalWindow && stage == 0) {
                SDL_Event quit{}; quit.type = SDL_EVENT_QUIT; SDL_PushEvent(&quit); ++stage;
            } else if (mode == "choose-cancel" && stage == 1 && [NSApp.modalWindow isKindOfClass:[NSOpenPanel class]]) {
                [(NSOpenPanel *)NSApp.modalWindow cancel:nil]; ++stage;
            } else {
                NSString *title = nil;
                if (mode == "choose-quit" || (mode == "choose-cancel" && stage == 2)) title = @"Quit";
                if (mode == "choose-cancel" && stage == 0) title = @"Choose ISO…";
                if (mode == "cancel" && stage == 0) title = @"Cancel";
                if (mode == "invalid" && stage == 0) title = @"OK";
                if (title) for (NSWindow *window in NSApp.windows) {
                    if (window.visible) if (NSButton *button = find_button(window.contentView, title)) {
                        ++stage; [button performClick:nil]; break;
                    }
                }
            }
        }];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
        bool ok = false;
        if (mode == "import" || mode == "cancel" || mode == "invalid") {
            if (argc != 4) return 2;
            bool imported = mac_setup::import_with_progress(argv[2], argv[3]);
            ok = mode == "import" ? imported && mac_setup::game_folder(argv[3]) : !imported && stage == 1 && !std::filesystem::exists(argv[3]);
            for (const auto &entry : std::filesystem::directory_iterator(std::filesystem::path(argv[3]).parent_path()))
                if (entry.path().filename().string().find(".import-") == 0) ok = false;
        } else {
            setenv("HP2_SETUP_HOME", argv[2], 1);
            std::string game;
            const bool ready = mac_setup::choose_game(game);
            ok = mode == "remembered" ? ready && stage == 0 : !ready && stage > 0;
            if (ready) fprintf(stderr, "Reused: %s\n", game.c_str());
        }
        [timer invalidate];
        SDL_Quit();
        fprintf(stderr, "%s: %s\n", ok ? "PASS" : "FAIL", mode.c_str());
        return ok ? 0 : 1;
    }
}
