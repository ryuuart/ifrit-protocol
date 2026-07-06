#!/usr/bin/env python3
"""
Setup and build script for spell-circle-canvas.

Locates Qt and vcpkg, writes CMakeUserPresets.json, then configures and builds.

Usage:
    python scripts/setup.py [--config Debug|Release] [--build-only] [--configure-only]
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent  # apps/spell-circle-canvas
USER_PRESETS = PROJECT_DIR / "CMakeUserPresets.json"
BUILD_DIR = PROJECT_DIR / "build"

# Qt minimum version required by CMakeLists.txt
QT_MIN = (6, 11)

# Search roots for Qt installations (platform-suffix assumed: macos)
QT_SEARCH_ROOTS = [
    Path.home() / ".local" / "opt" / "Qt",
    Path("/usr/local/opt/Qt"),
    Path("/opt/homebrew/opt/qt"),
    Path("/opt/Qt"),
]

# Search roots for vcpkg
VCPKG_SEARCH = [
    Path.home() / ".local" / "share" / "vcpkg",
    Path.home() / "vcpkg",
    Path("/usr/local/share/vcpkg"),
    Path("/opt/vcpkg"),
]


def _version_tuple(s: str) -> tuple[int, ...]:
    try:
        return tuple(int(x) for x in re.split(r"[.\-]", s) if x.isdigit())
    except Exception:
        return (0,)


def find_qt(platform: str = "macos") -> Path | None:
    """Return the best Qt installation path for the given platform suffix."""
    env_root = os.environ.get("Qt6_DIR") or os.environ.get("QT_DIR") or os.environ.get("QTDIR")
    if env_root:
        p = Path(env_root)
        if p.exists():
            print(f"  Qt: found via env var at {p}")
            return p

    candidates: list[tuple[tuple[int, ...], Path]] = []

    for root in QT_SEARCH_ROOTS:
        if not root.exists():
            continue
        for version_dir in root.iterdir():
            if not version_dir.is_dir():
                continue
            ver = _version_tuple(version_dir.name)
            if ver < QT_MIN:
                continue
            platform_dir = version_dir / platform
            if platform_dir.exists():
                candidates.append((ver, platform_dir))

    if not candidates:
        return None

    candidates.sort(key=lambda c: c[0], reverse=True)
    best = candidates[0][1]
    print(f"  Qt: {best}  (version {'.'.join(str(x) for x in candidates[0][0])})")
    return best


def find_vcpkg() -> Path | None:
    """Return the vcpkg root directory."""
    env_root = os.environ.get("VCPKG_ROOT")
    if env_root:
        p = Path(env_root)
        if (p / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
            print(f"  vcpkg: found via VCPKG_ROOT at {p}")
            return p

    for candidate in VCPKG_SEARCH:
        if (candidate / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
            print(f"  vcpkg: {candidate}")
            return candidate

    return None


def write_user_presets(qt_path: Path, vcpkg_root: Path) -> None:
    presets = {
        "version": 4,
        "configurePresets": [
            {
                "name": "vcpkg",
                "cacheVariables": {
                    "CMAKE_TOOLCHAIN_FILE": str(vcpkg_root / "scripts" / "buildsystems" / "vcpkg.cmake")
                },
                "environment": {
                    "VCPKG_ROOT": str(vcpkg_root)
                },
            },
            {
                "name": "qt",
                "cacheVariables": {
                    "CMAKE_PREFIX_PATH": str(qt_path)
                },
                "environment": {
                    "PATH": f"{qt_path / 'bin'}:$penv{{PATH}}"
                },
            },
            {
                "name": "main",
                "inherits": ["vcpkg", "qt", "ninja"],
            },
        ],
        "buildPresets": [
            {
                "name": "main",
                "configurePreset": "main",
            }
        ],
    }
    USER_PRESETS.write_text(json.dumps(presets, indent=2) + "\n")
    print(f"  Wrote {USER_PRESETS.relative_to(Path.cwd()) if USER_PRESETS.is_relative_to(Path.cwd()) else USER_PRESETS}")


def run(cmd: list[str], cwd: Path) -> int:
    print(f"\n$ {' '.join(str(c) for c in cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode


def configure() -> int:
    return run(["cmake", "--preset", "main"], cwd=PROJECT_DIR)


def build(config: str) -> int:
    return run(
        ["cmake", "--build", "build", "--config", config, "--parallel"],
        cwd=PROJECT_DIR,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Setup and build spell-circle-canvas")
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release", "RelWithDebInfo"],
                        help="Build configuration (default: Debug)")
    parser.add_argument("--build-only", action="store_true",
                        help="Skip configure; only build (requires existing build dir)")
    parser.add_argument("--configure-only", action="store_true",
                        help="Stop after cmake configure, skip build")
    parser.add_argument("--force", action="store_true",
                        help="Overwrite existing CMakeUserPresets.json even if it already exists")
    args = parser.parse_args()

    print(f"spell-circle-canvas setup — project: {PROJECT_DIR}\n")

    if not args.build_only:
        if USER_PRESETS.exists() and not args.force:
            print(f"CMakeUserPresets.json already exists (use --force to regenerate).")
        else:
            print("Locating dependencies...")
            qt = find_qt()
            if qt is None:
                print(
                    f"\nERROR: No Qt >= {'.'.join(str(x) for x in QT_MIN)} installation found.\n"
                    f"Install Qt via the Qt Installer and place it under one of:\n"
                    + "\n".join(f"  {r}" for r in QT_SEARCH_ROOTS)
                    + "\nOr set Qt6_DIR / QT_DIR in your environment.",
                    file=sys.stderr,
                )
                return 1

            vcpkg = find_vcpkg()
            if vcpkg is None:
                print(
                    "\nERROR: vcpkg not found.\n"
                    "Clone https://github.com/microsoft/vcpkg and bootstrap it, then place it at one of:\n"
                    + "\n".join(f"  {p}" for p in VCPKG_SEARCH)
                    + "\nOr set VCPKG_ROOT in your environment.",
                    file=sys.stderr,
                )
                return 1

            write_user_presets(qt, vcpkg)

        rc = configure()
        if rc != 0:
            print(f"\nConfigure failed (exit {rc}).", file=sys.stderr)
            return rc

        if args.configure_only:
            print("\nDone (configure only).")
            return 0

    rc = build(args.config)
    if rc != 0:
        print(f"\nBuild failed (exit {rc}).", file=sys.stderr)
        return rc

    binary = BUILD_DIR / "bin" / args.config / "SpellCircle"
    print(f"\nBuild complete.  Binary: {binary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
