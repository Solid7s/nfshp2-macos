# macOS port

This branch runs the upstream static recompilation natively on Intel macOS.
The USA disc has been imported and the complete game and sound driver built.
Local testing reached a Hot Pursuit Quick Race at the configured 30 FPS on a
Radeon Pro 5500M, with movies, menus, stereo audio, and profile save/reload working.
v1.0 is the first macOS release, with limited hardware and gameplay testing.

The platform uses SDL3 for the Cocoa window, keyboard, mouse, gamepads, and audio;
the existing Vulkan renderer runs through MoltenVK. The shared input mappings,
menu behavior, PS2 arithmetic, and renderer remain in use. Cocoa events and
surface creation stay on the main thread; the game runs on a worker with a
32 MB stack. Windows-only profiling/debug-register helpers are excluded on Mac.

## Build tools

Install Xcode Command Line Tools, Python 3, CMake, Ninja, shaderc (`glslc`),
Vulkan headers, and MoltenVK. SDL3 is built from the upstream bundled archive.
With Homebrew:

```sh
brew install cmake ninja shaderc vulkan-headers molten-vk
```

Homebrew may build these tools from source on Intel Macs. The Windows LLVM 23
PGO profile is disabled by the script, as is the Windows fixed-address memory
mode. The initial target is the local Intel Mac; Apple Silicon has not been tested.

## Import, build, and run

From this repository:

```sh
./macos.sh import "/path/to/Need for Speed Hot Pursuit 2.iso"
./macos.sh build
./macos.sh run
./macos.sh app
```

The importer reads the ISO without mounting it and writes `game/`. It accepts only
a new or empty destination. A different destination can be supplied as the second
argument to `import`, and then as the argument to `build` and `run`. A game folder
already installed by the upstream port can also be supplied to `build` and `run`.
The folder must contain `data/SLUS_203.62` and `data/IRX253/STREAM.IRX`.

The upstream supported discs and checksums still apply. USA SLUS-20362 has been
tested. The importer retains the upstream Asia/Korea conversion patches and
regional handling; other regions have not been tested on this Mac.
No disc data or generated game code is committed.

`build` generates the C++ with the upstream scripts, then produces
`recompmac/speed`. `run` sets the working directory to the game folder so settings
and saves stay with that installation. Terminal diagnostics go to stderr.

`app` creates `recompmac/Need for Speed Hot Pursuit 2.app`, which can be opened in
Finder. It includes the executable, SDL3, an icon, and MoltenVK, with a local ad hoc
signature. The app can be moved or shared without carrying a machine-specific
game path. It does not contain disc data; users of a prebuilt app do not need
the build tools or Terminal to import their own disc.

On a new account, opening the app shows **Choose ISO…** and **Use Existing Folder…**.
Choose a supported **PlayStation 2** ISO (the PC version is not supported). The
import displays progress, supports cancellation, and starts the game when ready.
It reads the ISO without mounting or changing it. The ISO is not needed again
after a successful import. Allow about 4 GB for the imported data.

New installations live in
`~/Library/Application Support/Need for Speed Hot Pursuit 2/Game/`.
The app remembers the chosen installation in `game-location.txt` in that same
Application Support directory and skips setup on subsequent launches. Existing
folders keep their settings and saves in place. If the remembered folder moves
or disappears, setup opens again so the player can select its new location.
An import uses a temporary folder and only publishes it on success; cancelled or
failed imports are cleaned up. Existing destination folders are never overwritten.

For this development checkout, packaging registers `game/` for the current account
if no location has been saved yet. `./macos.sh app /new/game/folder` explicitly
registers a different existing installation. The path is saved per user, outside
the app. Rebuild the app after recompiling the game.
Within the chosen game folder, settings are in `hp2.ini`, profiles in `saves/`, and app diagnostics in
`speed.log` with the previous run in `speed_prev.log`. The Mac log is not
size capped. `HP2_STDERR=1` keeps app diagnostics on stderr for developer tests.
`HP2_DATA` or `--data <folder>` explicitly selects an installation and bypasses
setup. `HP2_SETUP_HOME=<isolated folder>` redirects setup storage for developer
checks without touching the user's remembered installation.

Existing keyboard and controller mappings are retained. Use arrows and Enter in
menus, Esc to go back or pause, Up to accelerate, Down to brake/reverse, Left/Right
to steer, Space for the handbrake, and C to change the camera. Command+F or Option+Enter toggles
fullscreen. The mouse uses window coordinates while Vulkan uses Retina drawable
pixels. Controls can be changed in the game's Options menu.

## Packaging a release

After building, run `./macos.sh app`, then `python3 tools/macos/package_release.py`.
The app version is defined in `tools/macos/package_app.py`. The release script
verifies the app signature and Intel architecture, checks an explicit list of
allowed bundle files, and creates a ZIP plus SHA-256 checksum under `dist/`.
Only the app and public documentation are archived; imported data, ISO images,
saves, generated source, logs, and the local installation preference are excluded.
The app and ZIP include credits and third-party license notices.

## Checks without a disc

```sh
./macos.sh check
./macos.sh probe
```

Without generated game code, `check` builds the `hp2-platform` object target,
importer, importer checks, and graphics probe. It does **not** produce a playable
executable or substitute a game stub. Only `iop_cpu.cpp`, which needs the generated
sound-driver table, is deferred. Once code is generated, the normal game target
is built instead.

The importer checks exercise embedded resource contents and directory layout,
Unicode paths, known hash vectors, and ISO directory/sector reads using a small
synthetic disc with no game content. Separate checks cover invalid-disc rejection
and preservation of nonempty destination folders.

The input regression check verifies that controller polling preserves queued
steering presses/releases, focus changes, and window-close events. The old polling
worker cleared SDL's entire queue every 2 ms, intermittently losing keyboard
releases and leaving steering held. On Mac only the window's main thread now
consumes that queue. Shortcut checks cover both Command keys, held/repeated F,
modifier release order, Option+Enter, and focus loss.

The setup check covers first launch, remembered locations, Unicode paths, old
launcher migration, moved folders, recovery after import, invalid discs,
cancellation, and preservation of existing saves and settings. The separate
`build/macos/hp2-setup-probe` exercises the actual Cocoa dialogs and import window;
run it without arguments for its developer test modes. GUI checks require a
logged-in desktop session.

`probe` opens a three-second test window and presents cleared images through the
actual Vulkan/Metal swapchain. It reports the selected GPU, supported depth
formats, and relevant features. It does not run the game or its shaders.

The platform target and all 17 shader variants compile successfully on macOS
15.7.4 with Apple Clang 17, CMake 4.5, and the bundled SDL3. The importer CTest
passes. See `build/platform-build.log` for the local build results.

An early probe on Intel x86_64, Radeon Pro 5500M, using MoltenVK
1.4.1 presented 211 frames at 1600 x 1200. A repeat with the built SDL3 and installed
MoltenVK 1.4.2 is recorded in `build/vulkan-probe.log`. Both tested depth/stencil formats,
depth clamp, shader clip distance, and anisotropy were available. These results
establish the basic graphics path, not game correctness or performance.

## Local game validation

Tested on macOS 15.7.4, Intel x86_64, Radeon Pro 5500M, MoltenVK 1.4.2:

- Imported the user's USA ISO and compiled all generated EE and IOP sources.
- Displayed intro movies, the title screen, menus, and a Hot Pursuit Quick Race.
- Sustained about 30 race frames per second at the default 2x render scale and
  200% draw distance; menus ran at about 60 FPS. This was a short smoke test.
- Visually checked the car, scenery, HUD, and rear-view mirror in captured frames.
- Opened CoreAudio and captured 74 seconds of nonzero 48 kHz, 16-bit stereo output.
- Created the `sung` profile through scripted keyboard entry. Invoked the game's
  existing Autosave screen through its debug navigation hook; it wrote a 70,880-byte
  save, which a subsequent normal profile selection read successfully.
- Launched the signed local app through macOS LaunchServices using its bundled
  MoltenVK and external game folder.
- Changed the window from 960 x 720 to 1024 x 768 and back, rebuilding the Retina
  swapchain from 1920 x 1440 to 2048 x 1536 and back without errors.
- Exercised the first-run welcome, ISO-picker cancellation, Command+Q, invalid
  disc error, and cancellation during a real import through the Cocoa UI.
- Imported the USA ISO into an isolated first-run installation (3,949 data files,
  about 3.2 GB), reused its saved location without another prompt, and launched
  the packaged app from that installation through to the title screen.
  The original `sung` save remained byte-for-byte unchanged.

Logs and screenshots are under `build/verify/`. Representative evidence:
`startup.log`, `race.png`, `audio.wav`, `save-memorycard.log`, and
`reload-memorycard.log`, and `launcher.log`. The importer CTest and app code-signature
verification pass.
First-run UI and launch evidence is under `build/verify/first-run/`.

A full race finish, long sessions, all tracks/modes, fullscreen transitions,
physical gamepads/rumble, Apple Silicon, and other Macs remain unverified. The save test above checks the
real save/load path; it does not establish end-of-race progression correctness.

The upstream page-protection texture cache optimization currently uses its
portable checksum fallback on Mac. Windows cycle profiling and debug-register
watchpoints are not available in this backend.
