# Need for Speed: Hot Pursuit 2 — macOS port

An unofficial macOS adaptation of [ZloiKILLER’s Windows static recompilation](https://github.com/ZloiKILLER/nfshp2-port-win)
of the **PlayStation 2** game. Maintained by [Solid7s](https://github.com/Solid7s).
The game runs through a native macOS window and audio backend, using Vulkan through MoltenVK for graphics.

**You must provide your own game files.** No ISO, extracted game data, or saves are
included in this repository or the app download. Supply a supported PS2 disc image
(`.iso`) from your own copy, or a game folder already imported by this port or the
upstream Windows port. The Windows PC, Xbox, and GameCube editions are not supported.

## Download and play

1. Download **`nfshp2-macos-v1.0-intel.zip`** from [Releases](https://github.com/Solid7s/nfshp2-macos/releases/tag/v1.0).
   The automatically generated “Source code” archives are for building, not playing.
2. Unzip it and move **Need for Speed Hot Pursuit 2.app** to Applications or another folder of your choice.
3. Open the app. On first launch, choose **Choose ISO…** to import your disc image,
   or **Use Existing Folder…** to select an imported game folder containing `data/`.
4. Wait for the import to finish. Progress and cancellation are available, and the
   game starts automatically. Future launches remember the installation.

The app includes its runtime dependencies; no Terminal, Homebrew, compiler, or PS2
BIOS is needed to use the download. Allow about **4 GB** of disk space for an import.
The ISO is read without mounting or modifying it and is not needed after import.

This release is ad hoc signed, **not Apple-notarized**. If macOS blocks the app
because it cannot verify its developer, first attempt to open it, then use
**System Settings → Privacy & Security → Open Anyway** for this app if you trust
the download. See [Apple’s instructions](https://support.apple.com/en-us/102445).

## Compatibility

- **v1.0 is Intel (`x86_64`) only**, with macOS 15 or later required by the app bundle.
- Tested on macOS 15.7.4 with an Intel Mac and Radeon Pro 5500M. Short tests reached
  a Quick Race at about 30 FPS with 2× render scale, working audio, and profile save/load.
- **Apple Silicon is untested.** This build requires Rosetta there; it is not a
  native ARM or Universal app, and performance or graphics compatibility is not promised.
- Full race completion, long sessions, every track/mode, and physical controllers
  have not been comprehensively tested. This is the first macOS release, with limited hardware coverage.

The importer recognizes these upstream-supported PS2 releases:

| Disc | Serial | macOS testing |
| --- | --- | --- |
| USA | SLUS-20362 | Imported and played |
| Europe v1.00 / v2.00 | SLES-50731 | Not tested |
| Asia | SLPM-65181 | Not tested |
| Korea | SLPM-67527 | Not tested |

## Settings and saves

New imports are stored outside the app:

```text
~/Library/Application Support/Need for Speed Hot Pursuit 2/Game/
```

Within that folder, `hp2.ini` holds settings, `saves/` holds player profiles, and
`speed.log` holds diagnostics. If you choose an existing installation, its files
and saves stay in that location. Replacing the `.app` does not replace your saves.
If you move the game folder, the app asks you to select it again.

Open **Options → Graphics** for resolution, 1×–8× render scale, aspect ratio,
30/60 FPS, VSync, draw distance, reflections, model detail, antialiasing, texture
filtering, and color adjustments. Settings save automatically. **Options → Controls**
contains keyboard and gamepad bindings.

Default keyboard controls: arrows and Enter in menus; Up accelerates, Down brakes,
Left/Right steer, Space is the handbrake, C changes camera, and Esc pauses or goes back.
**Command+F** or **Option+Enter** toggles fullscreen.

## Building and development

See [MACOS.md](MACOS.md) for macOS dependencies, import/build commands, architecture,
and validation details. The source checkout does not include the generated game
C++; building the playable executable also requires your own supported game files.
See [TECHNICAL.md](TECHNICAL.md) for the upstream runtime and recompiler, and
[README-WINDOWS.md](README-WINDOWS.md) for the original Windows instructions.

## Credits and rights

- **Electronic Arts and the original development team** — Need for Speed: Hot Pursuit 2.
- **ZloiKILLER** — the original static recompilation, Windows port, renderer, and runtime on which this work is based.
- **Solid7s** — macOS adaptation, app packaging, first-run import, and Mac input fixes.
- **SDL, MoltenVK, SPIRV-Cross, SMAA, and Silent** — libraries and contributions detailed in [CREDITS.md](CREDITS.md).

Need for Speed and Hot Pursuit are trademarks of Electronic Arts. The original
game and its content remain the property of Electronic Arts and their respective
copyright holders; music, vehicles, logos, and other licensed material retain
their respective owners’ rights. This is an unofficial fan project, not affiliated
with, endorsed by, or sponsored by Electronic Arts or any other rights holder.
Providing this port does not grant rights to the original game or its assets.

See [CREDITS.md](CREDITS.md) and [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt)
for attribution and bundled library licenses.
