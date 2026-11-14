#!/usr/bin/env python3
"""Package a portable app with first-run import; register local data per user."""
import os
from pathlib import Path
import plistlib
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def copy_built(source, destination):
    # Replace the inode, never truncate an executable/library that a running
    # game may still have mapped. That session can finish on its old build.
    fd, name = tempfile.mkstemp(prefix=".hp2-update-", dir=destination.parent)
    os.close(fd)
    temporary = Path(name)
    try:
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    finally:
        if temporary.exists():
            temporary.unlink()


def main():
    game = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / "game").resolve()
    binary = ROOT / "recompmac/speed"
    if not binary.is_file():
        sys.exit("Build the game first: ./macos.sh build")
    valid_game = all((game / name).is_file() for name in (
        "data/index.txt", "data/SLUS_203.62", "data/IRX253/STREAM.IRX", "data/lang/ENGLISH.txt"))
    if len(sys.argv) > 1 and not valid_game:
        sys.exit(f"Not an imported game folder: {game}")
    cache = (ROOT / "build/macos/CMakeCache.txt").read_text()
    molten = next(Path(line.split("=", 1)[1]) for line in cache.splitlines()
                  if line.startswith("MOLTENVK_LIBRARY:FILEPATH="))
    app = ROOT / "recompmac/Need for Speed Hot Pursuit 2.app"
    contents = app / "Contents"
    for sub in ("MacOS", "Resources", "Frameworks"):
        (contents / sub).mkdir(parents=True, exist_ok=True)
    copy_built(binary, contents / "MacOS/speed")
    dylib = contents / "Frameworks/libMoltenVK.dylib"
    copy_built(molten, dylib)
    dylib.chmod(0o644)
    subprocess.run(["install_name_tool", "-id", "@rpath/libMoltenVK.dylib", str(dylib)], check=True)
    # Remove the old machine-specific bundle hint; a shared app starts setup
    # independently on every account. Saves and data stay outside the bundle.
    (contents / "Resources/game-location.txt").unlink(missing_ok=True)
    info = {
        "CFBundleExecutable": "speed",
        "CFBundleIdentifier": "local.nfshp2.macos",
        "CFBundleName": "Need for Speed Hot Pursuit 2",
        "CFBundleDisplayName": "Need for Speed: Hot Pursuit 2",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": "1.0",
        "CFBundleVersion": "100",
        "CFBundleIconFile": "Game.icns",
        "NSHighResolutionCapable": True,
        "NSSupportsAutomaticGraphicsSwitching": True,
        "LSMinimumSystemVersion": "15.0",
        "NSHumanReadableCopyright": "Unofficial macOS port. Original game: Electronic Arts and respective rights holders. See bundled credits.",
    }
    with (contents / "Info.plist").open("wb") as f:
        plistlib.dump(info, f)
    for name in ("CREDITS.md", "THIRD_PARTY_NOTICES.txt"):
        copy_built(ROOT / name, contents / "Resources" / name)
    # Reuse the upstream icon's largest PNG; no new artwork or game data.
    ico = (ROOT / "recomp/runtime/speed.ico").read_bytes()
    count = struct.unpack_from("<H", ico, 4)[0]
    entries = [struct.unpack_from("<BBBBHHII", ico, 6 + i * 16) for i in range(count)]
    entry = max(entries, key=lambda e: (e[0] or 256) * (e[1] or 256))
    with tempfile.TemporaryDirectory(prefix="hp2-icon-") as tmp:
        source = Path(tmp) / "source.png"
        source.write_bytes(ico[entry[7]:entry[7] + entry[6]])
        subprocess.run(["sips", "-s", "format", "icns", str(source), "--out",
                        str(contents / "Resources/Game.icns")], check=True, stdout=subprocess.DEVNULL)
    subprocess.run(["codesign", "--force", "--sign", "-", str(dylib)], check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    support = Path.home() / "Library/Application Support/Need for Speed Hot Pursuit 2"
    location = support / "game-location.txt"
    if valid_game and (len(sys.argv) > 1 or not location.exists()):
        support.mkdir(parents=True, exist_ok=True)
        fd, name = tempfile.mkstemp(prefix=".location-", dir=support)
        try:
            with os.fdopen(fd, "w") as f:
                f.write(str(game) + "\n")
            os.replace(name, location)
        finally:
            if os.path.exists(name):
                os.unlink(name)
        print(f"This account will keep using its existing data and saves: {game}")
    print(f"App: {app}")
    print("Other users are prompted to choose their PS2 ISO or an existing game folder on first launch.")


if __name__ == "__main__":
    main()
