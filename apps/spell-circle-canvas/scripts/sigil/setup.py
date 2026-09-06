"""Verb: setup — discover Qt and vcpkg, write the presets, configure, build.

    sigil.py setup [--config Release|Debug|RelWithDebInfo]
                   [--configure-only | --build-only]

Besides the primary `main` preset (build/), the file it writes carries
one configure, build and test preset for each secondary tree —
`coverage`, `asan`, `tsan` — so the sanitize verb configures with
`cmake --preset <name>` and CMake composes the toolchain, the Qt prefix
and the instrumentation flags itself. What each search root is, what the
composed presets carry and why each instrumentation flag is set is
scripts/README.md.

No library is named here. A library whose dependency needs finding
carries its own find module beside it, so this composes the toolchain,
the Qt prefix, the asset cache and the instrumented trees, and nothing
about what is built with them.
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

from sigil import tree

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


def _version_tuple(version_text: str) -> tuple:
    """Parses numeric version components from a directory name."""
    return tuple(
        int(component)
        for component in re.split(r"[.\-]", version_text)
        if component.isdigit()
    )


def find_qt(platform: str = "macos") -> Path | None:
    """The best Qt installation for the given platform suffix."""
    environment_root = (
        os.environ.get("Qt6_DIR") or os.environ.get("QT_DIR") or os.environ.get("QTDIR")
    )
    if environment_root:
        environment_path = Path(environment_root)
        if environment_path.exists():
            print(f"  Qt: found via env var at {environment_path}")
            return environment_path

    candidates: list = []
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
    best = candidates[0][1]
    version_text = ".".join(str(component) for component in candidates[0][0])
    print(f"  Qt: {best}  (version {version_text})")
    return best


def find_vcpkg() -> Path | None:
    """The vcpkg root directory."""
    environment_root = os.environ.get("VCPKG_ROOT")
    if environment_root:
        environment_path = Path(environment_root)
        if (environment_path / "scripts" / "buildsystems" / "vcpkg.cmake").exists():
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
def instrumented(compile_flags: str, link_flags: str) -> dict:
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
    for name, secondary in SECONDARY_TREES.items():
        presets["configurePresets"].append(
            {
                "name": name,
                "inherits": ["main"],
                "binaryDir": f"${{sourceDir}}/build-{name}",
                "cacheVariables": {
                    **secondary["cacheVariables"],
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
                "environment": secondary["environment"],
            }
        )


def user_presets(qt_installation: Path, vcpkg_root: Path) -> dict:
    """The local Qt/vcpkg CMake preset composition."""
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
                        f"clear;x-azurl,file://{tree.ASSET_CACHE_DIR}/,,readwrite"
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
            {"name": "main", "configurePreset": "main", "configuration": "Release"},
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
    secondary tree, a different instrumentation flag, a Qt that moved —
    has to reach the tree on the next run rather than waiting for someone
    to remember a flag. Rewriting it unconditionally would reconfigure on
    every invocation, so the content is compared first and an unchanged
    file keeps its timestamp.
    """
    tree.ASSET_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    text = json.dumps(user_presets(qt_installation, vcpkg_root), indent=2) + "\n"
    if tree.USER_PRESETS.exists() and tree.USER_PRESETS.read_text() == text:
        print(f"  {tree.USER_PRESETS.name} is current")
        return
    tree.USER_PRESETS.write_text(text)
    print(f"  Wrote {tree.USER_PRESETS}")


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(
        prog="sigil.py setup",
        description="Discover Qt and vcpkg, write CMakeUserPresets.json, "
        "configure and build spell-circle-canvas",
    )
    parser.add_argument(
        "--config",
        default="Release",
        choices=tree.CONFIGURATIONS,
        help="build configuration (default: Release)",
    )
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="skip discovery and configure; only build (requires a configured tree)",
    )
    parser.add_argument(
        "--configure-only",
        action="store_true",
        help="stop after cmake configure, skip the build",
    )
    arguments = parser.parse_args(argv)

    print(f"spell-circle-canvas setup — project: {tree.PROJECT_DIR}\n")

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

        code = tree.run(["cmake", "--preset", "main"])
        if code != 0:
            print(f"\nConfigure failed (exit {code}).", file=sys.stderr)
            return code
        if arguments.configure_only:
            print("\nDone (configure only).")
            return 0

    code = tree.run(
        ["cmake", "--build", "build", "--config", arguments.config, "--parallel"]
    )
    if code != 0:
        print(f"\nBuild failed (exit {code}).", file=sys.stderr)
        return code

    print(
        f"\nBuild complete.  Binary: {tree.bin_dir(arguments.config) / 'SpellCircle'}"
    )
    return 0
