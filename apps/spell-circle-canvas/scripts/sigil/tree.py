"""Where the build tree is, and how a verb talks to it.

Every verb needs some of the same arithmetic — which directory a preset
builds into, where a configuration's binaries land, where Sketchbook is
inside its bundle, what Qt prefix the presets recorded, what tests a
configured tree registers — and a copy per verb is a copy that can
disagree. This is that arithmetic, once. It is a module, not a verb: it
has no main.

The primary tree is `build/` and each secondary tree is
`build-<preset>/`, which is the layout the setup verb writes into
CMakeUserPresets.json. The generator is multi-config, so a configuration
is a directory under `bin/` rather than a tree of its own.
"""

import json
import os
import subprocess
import sys
from pathlib import Path
from typing import NoReturn

SCRIPTS_DIR = Path(__file__).resolve().parent.parent
PROJECT_DIR = SCRIPTS_DIR.parent  # apps/spell-circle-canvas
REPO_DIR = PROJECT_DIR.parent.parent
USER_PRESETS = PROJECT_DIR / "CMakeUserPresets.json"

# Where vcpkg looks for a download before it fetches one, and where the
# assets verb puts an archive that cannot be fetched at all — an SDK
# behind an account. Consulted first and written back to, so a port whose
# file is here never reaches the network and every other port still
# downloads normally.
ASSET_CACHE_DIR = Path.home() / ".local" / "opt" / "vcpkg-assets"

# Sketchbook is an app bundle, so every headless run goes through the
# binary inside it rather than the bundle.
SKETCHBOOK_IN_BUNDLE = "Sketchbook.app/Contents/MacOS/Sketchbook"

CONFIGURATIONS = ["Debug", "Release", "RelWithDebInfo"]


def fail(message: str) -> NoReturn:
    print(f"\nERROR: {message}", file=sys.stderr)
    sys.exit(1)


def build_dir(preset: str = "main") -> Path:
    """The binary directory of one preset's tree."""
    return PROJECT_DIR / ("build" if preset == "main" else f"build-{preset}")


def bin_dir(configuration: str, preset: str = "main") -> Path:
    return build_dir(preset) / "bin" / configuration


def benches_dir(configuration: str) -> Path:
    """Where the `benches` target puts the benchmark binaries."""
    return bin_dir(configuration) / "benches"


def sketchbook(configuration: str) -> Path:
    """The Sketchbook binary, or a refusal naming the target to build."""
    path = bin_dir(configuration) / SKETCHBOOK_IN_BUNDLE
    if not path.exists():
        fail(f"no Sketchbook at {path} — build the Sketchbook target first")
    return path


def presets() -> list:
    """The configure presets the local file carries, empty without one."""
    if not USER_PRESETS.exists():
        return []
    return json.loads(USER_PRESETS.read_text()).get("configurePresets", [])


def qt_prefix() -> Path | None:
    """The Qt installation the build uses: the CMAKE_PREFIX_PATH the setup
    verb recorded. None when the file does not exist yet, so a caller can
    say so instead of guessing at a Qt."""
    for preset in presets():
        prefix = preset.get("cacheVariables", {}).get("CMAKE_PREFIX_PATH")
        if prefix:
            return Path(prefix)
    return None


def require_primary_tree(what: str) -> None:
    """A secondary tree reads the primary tree's dependencies as they
    stand, so it cannot be configured before the primary one is."""
    installed = build_dir() / "vcpkg_installed"
    if not installed.is_dir():
        fail(
            f"no vcpkg_installed at {installed} — configure the primary build "
            f"first (sigil.py setup) so {what} can reuse its dependencies"
        )
    if not USER_PRESETS.exists():
        fail("no CMakeUserPresets.json — run sigil.py setup first")


def run(command: list, cwd: Path = PROJECT_DIR, echo: bool = True) -> int:
    """Runs a subprocess visibly and returns its exit code."""
    printable = " ".join(str(argument) for argument in command)
    if echo:
        print(f"\n$ {printable}")
    return subprocess.run(
        [str(argument) for argument in command], cwd=cwd, check=False
    ).returncode


def capture(
    command: list, cwd: Path = PROJECT_DIR, check: bool = False
) -> subprocess.CompletedProcess:
    """Runs a subprocess and hands back what it printed."""
    return subprocess.run(
        [str(argument) for argument in command],
        cwd=cwd,
        check=check,
        capture_output=True,
        text=True,
    )


def registered_tests(preset: str, configuration: str, test_filter: str | None) -> list:
    """Every test the preset's tree registers, as (name, command) pairs,
    narrowed by the ctest regex when one is given. A test whose binary is
    not built yet has no command. Read from ctest rather than from
    generated files, so nothing here parses the build's output."""
    command = ["ctest", "--preset", preset, "-C", configuration, "--show-only=json-v1"]
    if test_filter:
        command += ["-R", test_filter]
    result = capture(command, check=True)
    return [
        (test["name"], test.get("command"))
        for test in json.loads(result.stdout)["tests"]
    ]


def build_targets(preset: str, configuration: str, test_filter: str) -> list:
    """The targets a test filter needs built. A built GoogleTest case
    names its binary in its command; an unbuilt binary registers one
    `<target>_NOT_BUILT` entry in place of its cases, and a test the build
    itself runs — a header self-test — is registered under its own target
    name. A test driven by something outside the tree — a build tool, an
    interpreter — needs no target of its own and is left out."""
    binaries = str(bin_dir(configuration, preset))
    targets = set()
    for name, command in registered_tests(preset, configuration, test_filter):
        if name.endswith("_NOT_BUILT"):
            targets.add(name.removesuffix("_NOT_BUILT"))
        elif command is None:
            targets.add(name)
        elif command[0].startswith(binaries + os.sep):
            targets.add(Path(command[0]).name)
    return sorted(targets)


def test_executables(preset: str, configuration: str, test_filter: str | None) -> list:
    """The distinct built test binaries the selected tests run, in test
    order."""
    binaries = str(bin_dir(configuration, preset))
    found: list = []
    for _, command in registered_tests(preset, configuration, test_filter):
        if not command or not command[0].startswith(binaries + os.sep):
            continue
        executable = Path(command[0])
        if executable.exists() and executable not in found:
            found.append(executable)
    return found
