#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "macos_setup.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <mutex>
#include <thread>

@interface HP2ImportWindow : NSObject <NSWindowDelegate> {
@public
    std::atomic<bool> stop;
}
- (void)cancel:(id)sender;
@end
@implementation HP2ImportWindow
- (instancetype)init { if ((self = [super init])) stop = false; return self; }
- (void)cancel:(id)sender { stop = true; }
- (BOOL)windowShouldClose:(NSWindow *)sender { stop = true; return NO; }
@end

namespace mac_setup {
static bool quit_requested = false; // main thread only
static NSString *text(const std::string &s) { return [NSString stringWithUTF8String:s.c_str()] ?: @""; }
static NSString *path_text(const fs::path &p) { return text(p.string()); }
static void error(const std::string &message)
{
    NSAlert *alert = [NSAlert new];
    alert.messageText = @"Couldn’t set up the game";
    alert.informativeText = text(message);
    alert.alertStyle = NSAlertStyleWarning;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

bool import_with_progress(const fs::path &image, const fs::path &destination)
{
    @autoreleasepool {
        HP2ImportWindow *control = [HP2ImportWindow new];
        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 540, 220)
            styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
            backing:NSBackingStoreBuffered defer:NO];
        window.title = @"Need for Speed: Hot Pursuit 2";
        window.releasedWhenClosed = NO;
        window.delegate = control;
        auto label = [&](NSString *value, NSRect frame, CGFloat size) {
            NSTextField *field = [NSTextField labelWithString:value];
            field.frame = frame;
            field.font = [NSFont systemFontOfSize:size];
            field.lineBreakMode = NSLineBreakByTruncatingMiddle;
            [window.contentView addSubview:field];
            return field;
        };
        label(@"Importing your game", NSMakeRect(24, 172, 492, 26), 20);
        label(@"This only needs to be done once. The game will open when it’s ready.", NSMakeRect(24, 141, 492, 22), 12);
        NSTextField *status = label(@"Checking the disc image…", NSMakeRect(24, 105, 492, 20), 12);
        NSProgressIndicator *bar = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(24, 76, 492, 18)];
        bar.indeterminate = NO; bar.minValue = 0; bar.maxValue = 1000;
        [window.contentView addSubview:bar];
        NSButton *cancel = [NSButton buttonWithTitle:@"Cancel" target:control action:@selector(cancel:)];
        cancel.frame = NSMakeRect(418, 21, 100, 32);
        cancel.keyEquivalent = @"\033";
        [window.contentView addSubview:cancel];
        [window center]; [window makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];

        std::atomic<bool> done{false};
        std::mutex mutex;
        int progress = 0;
        std::string detail = "Checking the disc image…";
        InstallSteps steps;
        steps.stop = &control->stop;
        steps.progress = [&](int pm, const std::wstring &what) {
            std::lock_guard<std::mutex> lock(mutex);
            progress = pm;
            detail = fs::path(what).string();
        };
        bool success = false;
        std::thread worker([&] { success = import_game(image, destination, steps); done = true; });
        while (!done) {
            @autoreleasepool {
                SDL_Event event;
                while (SDL_PollEvent(&event)) if (event.type == SDL_EVENT_QUIT) quit_requested = true;
                if (quit_requested) control->stop = true;
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    bar.doubleValue = progress;
                    status.stringValue = control->stop ? @"Cancelling…" : text(std::to_string(progress / 10) + "% — " + detail);
                }
                if (control->stop) cancel.enabled = NO;
                SDL_Delay(16);
            }
        }
        worker.join();
        [window close];
        if (!success && !control->stop) error(steps.why);
        return success;
    }
}

bool choose_game(std::string &game, const fs::path &legacy)
{
    @autoreleasepool {
        quit_requested = false;
        // Cocoa's modal panels run their own loop. SDL queues Command+Q instead
        // of terminating immediately, so drain quit requests in those loops.
        NSTimer *timer = [NSTimer timerWithTimeInterval:0.05 repeats:YES block:^(NSTimer *) {
            SDL_Event event;
            if (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0) {
                quit_requested = true;
                if ([NSApp.modalWindow isKindOfClass:[NSOpenPanel class]]) [(NSOpenPanel *)NSApp.modalWindow cancel:nil];
                else if (NSApp.modalWindow) [NSApp abortModal];
            }
        }];
        [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSModalPanelRunLoopMode];
        struct TimerCleanup { __strong NSTimer *timer; ~TimerCleanup() { [timer invalidate]; } } cleanup{timer};
        try {
            // Override is for isolated developer checks; normal players use
            // their own Library, never a path embedded in the app bundle.
            const char *override = getenv("HP2_SETUP_HOME");
            fs::path support = override ? fs::path(override) :
                fs::path([NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES).firstObject fileSystemRepresentation]) /
                    "Need for Speed Hot Pursuit 2";
            support = fs::absolute(support);
            auto existing = remembered_game(support, legacy);
            std::string why;
            if (!existing.empty()) {
                if (!remember_game(support, existing, why)) { error(why); return false; }
                game = existing.string();
                return true;
            }
            [NSApp activateIgnoringOtherApps:YES];
            for (;;) {
                NSAlert *welcome = [NSAlert new];
                welcome.messageText = @"Set up Need for Speed: Hot Pursuit 2";
                welcome.informativeText = @"Choose your PlayStation 2 disc image (.iso) to import the game. You’ll only need to do this once.\n\nAlready imported it? Choose your existing game folder to keep using its settings and saves.";
                [welcome addButtonWithTitle:@"Choose ISO…"];
                [welcome addButtonWithTitle:@"Use Existing Folder…"];
                [welcome addButtonWithTitle:@"Quit"];
                NSModalResponse response = [welcome runModal];
                if (quit_requested) return false;
                if (response != NSAlertFirstButtonReturn && response != NSAlertSecondButtonReturn) return false;
                const bool folder = response == NSAlertSecondButtonReturn;
                NSOpenPanel *panel = [NSOpenPanel openPanel];
                panel.canChooseFiles = !folder;
                panel.canChooseDirectories = folder;
                panel.allowsMultipleSelection = NO;
                panel.title = folder ? @"Choose your imported game folder" : @"Choose your PS2 disc image";
                panel.prompt = folder ? @"Use Folder" : @"Import";
                panel.message = folder ? @"Select the folder containing data, hp2.ini, and saves." :
                    [NSString stringWithFormat:@"Game data and saves will be stored in %@. Allow about 4 GB of free space.", path_text(support)];
                if (!folder) panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"iso"] ?: UTTypeData];
                const NSModalResponse picked = [panel runModal];
                if (quit_requested) return false;
                if (picked != NSModalResponseOK) continue;
                fs::path selected(panel.URL.fileSystemRepresentation);
                if (folder) {
                    if (!game_folder(selected)) { error("Choose an imported game folder containing data/index.txt, data/SLUS_203.62, and data/IRX253/STREAM.IRX."); continue; }
                } else {
                    const auto destination = new_game_folder(support);
                    const bool imported = import_with_progress(selected, destination);
                    if (quit_requested) return false;
                    if (!imported) continue;
                    selected = destination;
                }
                if (!remember_game(support, selected, why)) { error(why + "\n\nYour game folder is at " + selected.string()); return false; }
                game = selected.string();
                return true;
            }
        } catch (const std::exception &e) { error(e.what()); return false; }
    }
}
}
