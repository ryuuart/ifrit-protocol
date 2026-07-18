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
QT_MINIMUM_VERSION = (6, 11)

# Search roots for Qt installations (platform-suffix assumed: macos)
QT_SEARCH_ROOTS = [
    Path.home() / ".local" / "opt" / "Qt",
    Path("/usr/local/opt/Qt"),
    Path("/opt/homebrew/opt/qt"),
    Path("/opt/Qt"),
]

# Search roots for vcpkg
VCPKG_SEARCH_ROOTS = [
    Path.home() / ".local" / "share" / "vcpkg",
    Path.home() / "vcpkg",
    Path("/usr/local/share/vcpkg"),
    Path("/opt/vcpkg"),
]


def _version_tuple(version_text: str) -> tuple[int, ...]:
    """Parses numeric version components from a directory name."""
    try:
        return tuple(
            int(component)
            for component in re.split(r"[.\-]", version_text)
            if component.isdigit()
        )
    except ValueError:
        return (0,)


def find_qt(platform: str = "macos") -> Path | None:
    """Return the best Qt installation path for the given platform suffix."""
    environment_root = (
        os.environ.get("Qt6_DIR")
        or os.environ.get("QT_DIR")
        or os.environ.get("QTDIR")
    )
    if environment_root:
        environment_path = Path(environment_root)
        if environment_path.exists():
            print(f"  Qt: found via env var at {environment_path}")
            return environment_path

    candidates: list[tuple[tuple[int, ...], Path]] = []

    for root in QT_SEARCH_ROOTS:
        if not root.exists():
            continue
        for version_directory in root.iterdir():
            if not version_directory.is_dir():
                continue
            version = _version_tuple(version_directory.name)
            if version < QT_MINIMUM_VERSION:
                continue
            platform_directory = version_directory / platform
            if platform_directory.exists():
                candidates.append((version, platform_directory))

    if not candidates:
        return None

    candidates.sort(key=lambda candidate: candidate[0], reverse=True)
    best_installation = candidates[0][1]
    version_text = ".".join(str(component) for component in candidates[0][0])
    print(f"  Qt: {best_installation}  (version {version_text})")
    return best_installation


def find_vcpkg() -> Path | None:
    """Return the vcpkg root directory."""
    environment_root = os.environ.get("VCPKG_ROOT")
    if environment_root:
        environment_path = Path(environment_root)
        toolchain = (
            environment_path / "scripts" / "buildsystems" / "vcpkg.cmake"
        )
        if toolchain.exists():
            print(f"  vcpkg: found via VCPKG_ROOT at {environment_path}")
            return environment_path

    for candidate in VCPKG_SEARCH_ROOTS:
        if (candidate / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
            print(f"  vcpkg: {candidate}")
            return candidate

    return None


def write_user_presets(qt_installation: Path, vcpkg_root: Path) -> None:
    """Writes the local Qt/vcpkg CMake preset composition."""
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
                    "CMAKE_PREFIX_PATH": str(qt_installation)
                },
                "environment": {
                    "PATH": f"{qt_installation / 'bin'}:$penv{{PATH}}"
                },
            },
            {
                "name": "main",
                "inherits": ["vcpkg", "qt", "ninja"],
            },
            {
                "name": "main-xcode",
                "inherits": ["vcpkg", "qt", "xcode"],
            }
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


def run(command: list[str], working_directory: Path) -> int:
    """Runs a subprocess visibly and returns its exit code."""
    print(f"\n$ {' '.join(str(argument) for argument in command)}")
    result = subprocess.run(command, cwd=working_directory, check=False)
    return result.returncode


def configure() -> int:
    """Configures the application with the composed ``main`` preset."""
    return run(["cmake", "--preset", "main"], PROJECT_DIR)


def build(configuration: str) -> int:
    """Builds the selected multi-configuration CMake configuration."""
    return run(
        [
            "cmake",
            "--build",
            "build",
            "--config",
            configuration,
            "--parallel",
        ],
        PROJECT_DIR,
    )


def main() -> int:
    """Locates dependencies, configures CMake, and builds the application."""
    argument_parser = argparse.ArgumentParser(
        description="Setup and build spell-circle-canvas"
    )
    argument_parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build configuration (default: Debug)",
    )
    argument_parser.add_argument(
        "--build-only",
        action="store_true",
        help="Skip configure; only build (requires existing build dir)",
    )
    argument_parser.add_argument(
        "--configure-only",
        action="store_true",
        help="Stop after cmake configure, skip build",
    )
    argument_parser.add_argument(
        "--force",
        action="store_true",
        help=(
            "Overwrite existing CMakeUserPresets.json even if it already "
            "exists"
        ),
    )
    arguments = argument_parser.parse_args()

    print(f"spell-circle-canvas setup — project: {PROJECT_DIR}\n")

    if not arguments.build_only:
        if USER_PRESETS.exists() and not arguments.force:
            print(f"CMakeUserPresets.json already exists (use --force to regenerate).")
        else:
            print("Locating dependencies...")
            qt_installation = find_qt()
            if qt_installation is None:
                print(
                    "\nERROR: No Qt >= "
                    f"{'.'.join(str(component) for component in QT_MINIMUM_VERSION)} "
                    "installation found.\n"
                    f"Install Qt via the Qt Installer and place it under one of:\n"
                    + "\n".join(f"  {path}" for path in QT_SEARCH_ROOTS)
                    + "\nOr set Qt6_DIR / QT_DIR in your environment.",
                    file=sys.stderr,
                )
                return 1

            vcpkg_root = find_vcpkg()
            if vcpkg_root is None:
                print(
                    "\nERROR: vcpkg not found.\n"
                    "Clone https://github.com/microsoft/vcpkg and bootstrap it, then place it at one of:\n"
                    + "\n".join(f"  {path}" for path in VCPKG_SEARCH_ROOTS)
                    + "\nOr set VCPKG_ROOT in your environment.",
                    file=sys.stderr,
                )
                return 1

            write_user_presets(qt_installation, vcpkg_root)

        return_code = configure()
        if return_code != 0:
            print(
                f"\nConfigure failed (exit {return_code}).", file=sys.stderr
            )
            return return_code

        if arguments.configure_only:
            print("\nDone (configure only).")
            return 0

    return_code = build(arguments.config)
    if return_code != 0:
        print(f"\nBuild failed (exit {return_code}).", file=sys.stderr)
        return return_code

    executable_path = BUILD_DIR / "bin" / arguments.config / "SpellCircle"
    print(f"\nBuild complete.  Binary: {executable_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
