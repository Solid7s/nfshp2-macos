# v1.0 — First macOS release

An unofficial macOS adaptation of [ZloiKILLER’s NFSHP2 static recompilation](https://github.com/ZloiKILLER/nfshp2-port-win).

**Download `nfshp2-macos-v1.0-intel.zip` below for the app.** The “Source code”
downloads are for developers. A SHA-256 checksum is provided separately.

## You must provide the game

**No ISO, extracted game data, or saves are included.** Supply your own supported
**PlayStation 2** ISO, or a game folder already imported by this port or the
upstream Windows port. PC, Xbox, and GameCube game files are not compatible.

Unzip the download, move **Need for Speed Hot Pursuit 2.app** to Applications,
and open it. First launch offers **Choose ISO…** and **Use Existing Folder…**.
Import displays progress, supports cancellation, and starts the game when ready.
The app remembers the location; no Terminal or build tools are required to play.

Allow about 4 GB for an import. New data and saves go to
`~/Library/Application Support/Need for Speed Hot Pursuit 2/Game/`, outside the app.
Existing imported folders and their saves stay in place.

## Included

- Native macOS window and stereo audio, with MoltenVK graphics.
- First-run ISO importer and installation discovery.
- Fix for lost keyboard releases that could leave steering held.
- Command+F / Option+Enter fullscreen toggle.
- Graphics options for resolution, render scale, 30/60 FPS, draw distance,
  reflections, antialiasing, and texture filtering.
- Bundled SDL3 and MoltenVK, with credits and library license notices.

## Compatibility and validation

**Intel (`x86_64`), macOS 15 or later.** Apple Silicon requires Rosetta and is
**untested**; this is not a Universal or native ARM build.

Tested on macOS 15.7.4 with a Radeon Pro 5500M: USA disc import, movies/menus,
a short Quick Race around 30 FPS at 2× render scale, audio, profile save/reload,
and the first-run setup flow. The importer, input, and setup regression checks pass.
Other disc regions, a full race finish, long sessions, and physical controllers
have not been comprehensively tested.

The app is **ad hoc signed, not notarized**. If macOS blocks it because it cannot
verify the developer, follow [Apple’s instructions](https://support.apple.com/en-us/102445)
to use **System Settings → Privacy & Security → Open Anyway** for this app if you trust it.

## Credits and copyright

Original game: **Electronic Arts and the original development team**.
Original static recompilation/Windows port: **ZloiKILLER**. macOS adaptation: **Solid7s**.
Additional credit to **SDL, MoltenVK, SPIRV-Cross, the SMAA authors, and Silent**.

Need for Speed and Hot Pursuit are trademarks of Electronic Arts. Original game
content and all licensed music, vehicle branding, and other material remain the
property of their respective rights holders. This unofficial project is not
affiliated with, endorsed by, or sponsored by Electronic Arts or other rights holders.
The port does not grant rights to the original game or its assets.

Full [credits](https://github.com/Solid7s/nfshp2-macos/blob/v1.0/CREDITS.md),
[library notices](https://github.com/Solid7s/nfshp2-macos/blob/v1.0/THIRD_PARTY_NOTICES.txt),
and [build instructions](https://github.com/Solid7s/nfshp2-macos/blob/v1.0/MACOS.md) are included in the source.
