# Credits and copyright notices

## Original game and rights holders

**Need for Speed: Hot Pursuit 2** is an Electronic Arts game. Credit belongs to
Electronic Arts, the original development team, and everyone who created the
original game. Need for Speed and Hot Pursuit are trademarks of Electronic Arts.
Original game code, artwork, textures, audio, music, vehicle designs, brand names,
logos, and other licensed material remain the property of their respective
copyright and trademark holders. No ownership of those works is claimed here.

This unofficial fan project is not affiliated with, endorsed by, or sponsored by
Electronic Arts or any other rights holder. Users must supply their own supported
PlayStation 2 disc image or previously imported game files. The repository and
release archive do not include a disc image, extracted game data, or player saves.
The executable contains the statically recompiled runtime and requires external
game data to play; this release does not grant rights to the original game.

## Port authors

- [ZloiKILLER](https://github.com/ZloiKILLER) — the original
  [nfshp2-port-win](https://github.com/ZloiKILLER/nfshp2-port-win) static recompilation,
  recompiler tools, native renderer, runtime, menus, importer, and Windows port.
  This fork is based on upstream version 0.13, commit
  `8ae05937be6865dde1e45c108cecd79f247d6b7a`, and preserves its history and credits.
- [Solid7s](https://github.com/Solid7s) — the macOS adaptation: Cocoa/SDL window and
  audio integration, MoltenVK integration, app packaging, first-run ISO selection
  and import, persistent installation discovery, and Mac input fixes.
- **Silent** — the upstream “Trigger control mappings” PCSX2 patch, providing
  gamepad layouts with throttle and brake on the triggers. The original patch
  and its attribution remain in `recomp/data/patch/SLUS-20362_1D2818AF_triggers.pnach`.

The upstream snapshot does not include a project-wide license file. This fork
does not assign a new license to upstream code, the original game, or third-party
material. Component licenses and existing copyright notices remain in effect.

## Bundled libraries and components

- [SDL 3.4.18](https://github.com/libsdl-org/SDL) — Sam Lantinga and SDL contributors;
  zlib license. Windowing, keyboard/mouse, controllers, and audio. Its HIDAPI and
  yuv2rgb notices are also reproduced with the release.
- [MoltenVK 1.4.2](https://github.com/KhronosGroup/MoltenVK/tree/v1.4.2) — The Brenwill
  Workshop Ltd. and contributors; Apache License 2.0. Vulkan on Apple Metal.
  The bundled library’s install name is adjusted for app packaging.
- [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) — Arm Limited and contributors;
  Apache License 2.0. Used by MoltenVK to translate shaders. MoltenVK 1.4.2 pins
  revision `6c09849fe88c48eaed08413aa022aaa136a3a057`.
- [SMAA](https://github.com/iryoku/smaa) — Jorge Jimenez, Jose I. Echevarria,
  Belen Masia, Fernando Navarro, and Diego Gutierrez; MIT license. Antialiasing
  shaders and lookup tables retained from upstream.

Full license notices are in [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt),
also bundled in the app’s `Contents/Resources/` directory. Library licenses do
not apply to the original game’s copyrighted content.
