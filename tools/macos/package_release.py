#!/usr/bin/env python3
"""Archive the signed app and public docs, excluding all local game/user data."""
import hashlib
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    app = ROOT / "recompmac/Need for Speed Hot Pursuit 2.app"
    expected = {
        "Contents/Info.plist", "Contents/MacOS/speed",
        "Contents/Frameworks/libMoltenVK.dylib", "Contents/Resources/Game.icns",
        "Contents/Resources/CREDITS.md", "Contents/Resources/THIRD_PARTY_NOTICES.txt",
        "Contents/_CodeSignature/CodeResources",
    }
    actual = {p.relative_to(app).as_posix() for p in app.rglob("*") if p.is_file()}
    if actual != expected or any(p.is_symlink() for p in app.rglob("*")):
        raise SystemExit(f"Unexpected app contents; rebuild/review before release: {actual ^ expected}")
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    for relative in ("Contents/MacOS/speed", "Contents/Frameworks/libMoltenVK.dylib"):
        arch = subprocess.check_output(["lipo", "-archs", str(app / relative)], text=True).strip()
        if arch != "x86_64":
            raise SystemExit(f"Update the release architecture label before packaging: {arch}")
    version = plistlib.loads((app / "Contents/Info.plist").read_bytes())["CFBundleShortVersionString"]
    output = ROOT / "dist"
    output.mkdir(exist_ok=True)
    archive = output / f"nfshp2-macos-v{version}-intel.zip"
    with tempfile.TemporaryDirectory(prefix="hp2-release-") as temp:
        folder = Path(temp) / f"Need for Speed Hot Pursuit 2 v{version}"
        folder.mkdir()
        shutil.copytree(app, folder / app.name)
        for name in ("README.md", "CREDITS.md", "THIRD_PARTY_NOTICES.txt", "MACOS.md", "README-WINDOWS.md", "TECHNICAL.md"):
            shutil.copy2(ROOT / name, folder / name)
        subprocess.run(["ditto", "-c", "-k", "--norsrc", "--noextattr", "--keepParent", str(folder), str(archive)], check=True)
    checksum = hashlib.sha256(archive.read_bytes()).hexdigest()
    (output / f"{archive.name}.sha256").write_text(f"{checksum}  {archive.name}\n")
    print(f"Release: {archive}\nSHA-256: {checksum}")


if __name__ == "__main__":
    main()
