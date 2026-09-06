#!/usr/bin/env python3
"""Setup and build script for spell-circle-canvas.

Locates Qt and vcpkg, writes CMakeUserPresets.json, then configures and
builds.

Usage:
    python scripts/setup.py [--config Release|Debug] [--build-only] [--configure-only]

Besides the primary `main` preset (build/), the file carries one
configure, build and test preset for each secondary tree — `coverage`,
`asan`, `tsan` — so coverage.py and sanitize.py configure with
`cmake --preset <name>` and CMake composes the toolchain, the Qt prefix
and the instrumentation flags itself. What each search root is, what the
composed presets carry and why each instrumentation flag is set is
scripts/README.md.
"""

import argparse
import json
import os
import re
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

# Where vcpkg looks for a download before it fetches one, and where
# scripts/stage_asset.py puts an archive that cannot be fetched at all —
# an SDK behind an account. Consulted first and written back to, so a
# port whose file is here never reaches the network and every other port
# still downloads normally.
ASSET_CACHE_DIR = Path.home() / ".local" / "opt" / "vcpkg-assets"

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
        os.environ.get("Qt6_DIR") or os.environ.get("QT_DIR") or os.environ.get("QTDIR")
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
        toolchain = environment_path / "scripts" / "buildsystems" / "vcpkg.cmake"
        if toolchain.exists():
            print(f"  vcpkg: found via VCPKG_ROOT at {environment_path}")
            return environment_path

    for candidate in VCPKG_SEARCH_ROOTS:
        if (candidate / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
            print(f"  vcpkg: {candidate}")
            return candidate

    return None


# The secondary trees: each is the `main` composition plus the compile and
# link flags of one instrumentation, in its own build-<name>/ directory,
# reading the primary tree's vcpkg_installed/ as-is with the manifest
# install disabled so it never writes there and never duplicates the
# dependency archives. Their test presets carry the runtime options the
# tools need. What each flag and each runtime option is for is
# scripts/README.md.
#
# workaround: -include cmake/SkiaSanitizerAbi.h, for instrumented Skia
# headers meeting an uninstrumented archive. The header states what the
# pin is and what it costs.
# workaround: detect_container_overflow=0, for the uninstrumented vcpkg
# archives. detect_leaks=0 beside it is a platform limitation, not a
# dependency one.
def instrumented(compile_flags: str, link_flags: str) -> dict[str, str]:
    """The cache variables that carry one instrumentation into a tree: the
    compile flags on C++, the link flags on every kind of binary."""
    return {
        "CMAKE_CXX_FLAGS": compile_flags,
        "CMAKE_EXE_LINKER_FLAGS": link_flags,
        "CMAKE_SHARED_LINKER_FLAGS": link_flags,
        "CMAKE_MODULE_LINKER_FLAGS": link_flags,
    }


SECONDARY_TREES = {
    "coverage": {
        "cacheVariables": instrumented(
            "-fprofile-instr-generate -fcoverage-mapping",
            "-fprofile-instr-generate",
        ),
        "environment": {
            "LLVM_PROFILE_FILE": (
                "${sourceDir}/build-coverage/coverage/raw/%p-%m.profraw"
            ),
        },
    },
    "asan": {
        "cacheVariables": instrumented(
            "-fsanitize=address,undefined -fno-omit-frame-pointer "
            "-fno-sanitize-recover=undefined "
            "-include ${sourceDir}/cmake/SkiaSanitizerAbi.h",
            "-fsanitize=address,undefined",
        ),
        "environment": {
            "ASAN_OPTIONS": "detect_leaks=0:detect_container_overflow=0",
            "UBSAN_OPTIONS": "print_stacktrace=1",
        },
    },
    "tsan": {
        "cacheVariables": instrumented(
            "-fsanitize=thread -fno-omit-frame-pointer",
            "-fsanitize=thread",
        ),
        "environment": {
            "TSAN_OPTIONS": (
                "halt_on_error=1:second_deadlock_stack=1:"
                "suppressions=${sourceDir}/ThreadSanitizerSuppressions.txt"
            ),
        },
    },
}


SECONDARY_CONFIGURATION = "RelWithDebInfo"

# The multi-config generator builds the configuration a build names and
# reads none as CMAKE_DEFAULT_BUILD_TYPE. vcpkg's toolchain reads
# CMAKE_BUILD_TYPE to put the release prefix ahead of the debug tree, and
# reads it unset as Debug; naming it here is what keeps a Release link
# free of debug archives.
MAIN_CACHE = {
    "CMAKE_DEFAULT_BUILD_TYPE": "Release",
    "CMAKE_BUILD_TYPE": "Release",
}


def secondary_presets(presets: dict) -> None:
    """Adds the configure, build and test preset of every secondary tree."""
    for name, tree in SECONDARY_TREES.items():
        presets["configurePresets"].append(
            {
                "name": name,
                "inherits": ["main"],
                "binaryDir": f"${{sourceDir}}/build-{name}",
                "cacheVariables": {
                    **tree["cacheVariables"],
                    "VCPKG_INSTALLED_DIR": "${sourceDir}/build/vcpkg_installed",
                    "VCPKG_MANIFEST_INSTALL": "OFF",
                    "CMAKE_DEFAULT_BUILD_TYPE": SECONDARY_CONFIGURATION,
                },
            }
        )
        # An instrumented tree wants symbols, not a debug build: the
        # instrumentation reads optimised code fine, and only a debugging
        # session asks for Debug.
        presets["buildPresets"].append(
            {
                "name": name,
                "configurePreset": name,
                "configuration": SECONDARY_CONFIGURATION,
            }
        )
        presets["testPresets"].append(
            {
                "name": name,
                "configurePreset": name,
                "configuration": SECONDARY_CONFIGURATION,
                "output": {"outputOnFailure": True},
                "environment": tree["environment"],
            }
        )


def user_presets(qt_installation: Path, vcpkg_root: Path) -> dict:
    """The local Qt/vcpkg CMake preset composition.

    No library is named here. A library whose dependency needs finding
    carries its own find module (src/common/substance/cmake, and
    src/common/scry/cmake beside it), so this file composes the toolchain,
    the Qt prefix, the asset cache and the instrumented trees and nothing
    about what is built with them."""
    main_inherits = ["vcpkg", "qt"]
    presets = {
        "version": 4,
        "configurePresets": [
            {
                "name": "vcpkg",
                "cacheVariables": {
                    "CMAKE_TOOLCHAIN_FILE": str(
                        vcpkg_root / "scripts" / "buildsystems" / "vcpkg.cmake"
                    )
                },
                "environment": {
                    "VCPKG_ROOT": str(vcpkg_root),
                    "X_VCPKG_ASSET_SOURCES": (
                        f"clear;x-azurl,file://{ASSET_CACHE_DIR}/,,readwrite"
                    ),
                },
            },
            {
                "name": "qt",
                "cacheVariables": {"CMAKE_PREFIX_PATH": str(qt_installation)},
                "environment": {"PATH": f"{qt_installation / 'bin'}:$penv{{PATH}}"},
            },
            # Release unless asked otherwise: the multi-config generator
            # builds whatever configuration a build names, and a build that
            # names none gets this one rather than Debug.
            {
                "name": "main",
                "inherits": main_inherits + ["ninja"],
                "cacheVariables": MAIN_CACHE,
            },
            {
                "name": "main-xcode",
                "inherits": main_inherits + ["xcode"],
                "cacheVariables": MAIN_CACHE,
            },
        ],
        "buildPresets": [
            {
                "name": "main",
                "configurePreset": "main",
                "configuration": "Release",
            },
            {
                "name": "main-xcode",
                "configurePreset": "main-xcode",
                "configuration": "Release",
            },
        ],
        "testPresets": [],
    }
    secondary_presets(presets)
    return presets


def write_user_presets(qt_installation: Path, vcpkg_root: Path) -> None:
    """Writes the preset file when what it would say has moved.

    The file is generated, so an edit to what generates it — another
    secondary tree, a different instrumentation flag — has to reach the
    tree on the next run. Rewriting it unconditionally would reconfigure
    on every invocation, so the content is compared first and an unchanged
    file keeps its timestamp.
    """
    ASSET_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    text = json.dumps(user_presets(qt_installation, vcpkg_root), indent=2) + "\n"
    if USER_PRESETS.exists() and USER_PRESETS.read_text() == text:
        print(f"  {USER_PRESETS.name} is current")
        return
    USER_PRESETS.write_text(text)
    print(f"  Wrote {USER_PRESETS}")


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
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="Build configuration (default: Release)",
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
    arguments = argument_parser.parse_args()

    print(f"spell-circle-canvas setup — project: {PROJECT_DIR}\n")

    if not arguments.build_only:
        print("Locating dependencies...")
        qt_installation = find_qt()
        if qt_installation is None:
            print(
                "\nERROR: No Qt >= "
                f"{'.'.join(str(component) for component in QT_MINIMUM_VERSION)} "
                "installation found.\n"
                "Install Qt via the Qt Installer and place it under one of:\n"
                + "\n".join(f"  {path}" for path in QT_SEARCH_ROOTS)
                + "\nOr set Qt6_DIR / QT_DIR in your environment.",
                file=sys.stderr,
            )
            return 1

        vcpkg_root = find_vcpkg()
        if vcpkg_root is None:
            print(
                "\nERROR: vcpkg not found.\n"
                "Clone https://github.com/microsoft/vcpkg and bootstrap it, "
                "then place it at one of:\n"
                + "\n".join(f"  {path}" for path in VCPKG_SEARCH_ROOTS)
                + "\nOr set VCPKG_ROOT in your environment.",
                file=sys.stderr,
            )
            return 1

        write_user_presets(qt_installation, vcpkg_root)

        return_code = configure()
        if return_code != 0:
            print(f"\nConfigure failed (exit {return_code}).", file=sys.stderr)
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
