"""Shared plumbing for orchestrators that maintain a secondary build tree.

coverage.py and sanitize.py each configure a dedicated tree beside the
primary build/ (instrumented for coverage, or built under a sanitizer),
build test targets there, and run ctest. Everything those steps have in
common lives here: resolving the primary build's configure preset,
materializing its environment, the visible-subprocess runner, the
configure call that shares the primary tree's installed vcpkg
dependencies read-only, test-executable discovery, and the build step.
This is a module, not a command — it has no main.
"""

import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import NoReturn

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent  # apps/spell-circle-canvas
USER_PRESETS = PROJECT_DIR / "CMakeUserPresets.json"
PROJECT_PRESETS = PROJECT_DIR / "CMakePresets.json"
PRIMARY_BUILD_DIR = PROJECT_DIR / "build"


def fail(message: str) -> NoReturn:
    print(f"\nERROR: {message}", file=sys.stderr)
    sys.exit(1)


def run(command: list, working_directory: Path, env: dict | None = None,
        capture: bool = False) -> subprocess.CompletedProcess:
    """Runs a subprocess visibly (or captured) and fails loudly on error."""
    printable = " ".join(str(argument) for argument in command)
    print(f"\n$ {printable}")
    result = subprocess.run(
        [str(argument) for argument in command],
        cwd=working_directory,
        env=env,
        check=False,
        capture_output=capture,
        text=capture,
    )
    if result.returncode != 0:
        if capture and result.stderr:
            print(result.stderr, file=sys.stderr)
        fail(f"command failed (exit {result.returncode}): {printable}")
    return result


def resolve_preset(name: str) -> dict:
    """Resolves a configure preset by walking its inherits chain.

    Merges cacheVariables and environment across CMakeUserPresets.json and
    CMakePresets.json the way CMake does: earlier entries in an inherits
    list win, and the preset itself wins over everything it inherits.
    """
    presets_by_name: dict[str, dict] = {}
    for presets_file in (PROJECT_PRESETS, USER_PRESETS):
        if not presets_file.exists():
            continue
        document = json.loads(presets_file.read_text())
        for preset in document.get("configurePresets", []):
            presets_by_name.setdefault(preset["name"], preset)

    if name not in presets_by_name:
        fail(
            f"no configure preset named {name!r} — run scripts/setup.py "
            "first to write CMakeUserPresets.json"
        )

    def merge(preset_name: str) -> dict:
        preset = presets_by_name[preset_name]
        resolved: dict = {"cacheVariables": {}, "environment": {}}
        inherits = preset.get("inherits", [])
        if isinstance(inherits, str):
            inherits = [inherits]
        for parent_name in reversed(inherits):
            parent = merge(parent_name)
            for key in ("cacheVariables", "environment"):
                resolved[key].update(parent[key])
            for key in ("generator", "binaryDir"):
                if key in parent:
                    resolved[key] = parent[key]
        for key in ("cacheVariables", "environment"):
            resolved[key].update(preset.get(key, {}))
        for key in ("generator", "binaryDir"):
            if key in preset:
                resolved[key] = preset[key]
        return resolved

    return merge(name)


def preset_environment(resolved: dict) -> dict:
    """Materializes the preset's environment on top of the process one."""
    environment = os.environ.copy()
    for key, value in resolved.get("environment", {}).items():
        environment[key] = re.sub(
            r"\$penv\{(\w+)\}",
            lambda match: os.environ.get(match.group(1), ""),
            str(value),
        )
    return environment


def configure_shared_tree(build_dir: Path, definitions: dict,
                          resolved: dict, environment: dict) -> None:
    """Configures a secondary tree, reusing the primary build's toolchain,
    Qt prefix, and installed vcpkg dependencies.

    The primary tree's vcpkg_installed/ is read as-is with the manifest
    install disabled, so configure never writes into it and the secondary
    tree never duplicates the dependency archives. `definitions` carries
    the cache variables that make this tree what it is (the coverage or
    sanitizer switch); they are applied on top of the preset's own.
    """
    installed_dir = PRIMARY_BUILD_DIR / "vcpkg_installed"
    if not installed_dir.is_dir():
        fail(
            f"no vcpkg_installed at {installed_dir} — configure the primary "
            f"build first (scripts/setup.py) so {build_dir.name} can reuse "
            "its dependencies"
        )

    command = [
        "cmake",
        "-S", PROJECT_DIR,
        "-B", build_dir,
        "-G", resolved.get("generator", "Ninja Multi-Config"),
        f"-DVCPKG_INSTALLED_DIR={installed_dir}",
        "-DVCPKG_MANIFEST_INSTALL=OFF",
    ]
    for key, value in resolved.get("cacheVariables", {}).items():
        command.append(f"-D{key}={value}")
    for key, value in definitions.items():
        command.append(f"-D{key}={value}")
    run(command, PROJECT_DIR, env=environment)


# An add_test() line in a generated CTestTestfile.cmake: the test name
# (plain or bracket-quoted) followed by the quoted executable path.
ADD_TEST_PATTERN = re.compile(
    r'add_test\(\s*(?:\[=*\[)?(?P<name>[^\s\]\)]+?)(?:\]=*\])?\s+'
    r'"(?P<executable>[^"]+)"'
)


def test_executables(build_dir: Path, configuration: str,
                     test_filter: str | None) -> list[Path]:
    """The distinct test executables in the tree, in test order.

    Parsed from the generated CTestTestfile.cmake files rather than from
    `ctest --show-only`, because ctest omits a test's command until its
    binary exists — and target derivation needs the paths before anything
    is built. The multi-config generator writes one add_test entry per
    configuration; only paths under this configuration's bin directory
    count. Tests driven by an outside interpreter (a system python, say)
    are skipped: they name no buildable executable in this tree.
    """
    configuration_marker = f"/bin/{configuration}/"
    executables: list[Path] = []
    for testfile in sorted(build_dir.rglob("CTestTestfile.cmake")):
        for match in ADD_TEST_PATTERN.finditer(testfile.read_text()):
            if test_filter and not re.search(test_filter,
                                             match.group("name")):
                continue
            executable = Path(match.group("executable"))
            if not executable.is_relative_to(build_dir):
                continue
            if configuration_marker not in str(executable):
                continue
            if executable not in executables:
                executables.append(executable)
    return executables


def derived_targets(build_dir: Path, configuration: str,
                    test_filter: str) -> list[str]:
    """The CMake targets a test filter needs built.

    Executable basenames double as CMake target names, app bundles
    included (the bundle directory carries the target's name).
    """
    return sorted(
        {executable.name
         for executable in test_executables(build_dir, configuration,
                                            test_filter)}
    )


def build(build_dir: Path, targets: list[str], configuration: str,
          environment: dict) -> None:
    command = [
        "cmake", "--build", build_dir, "--config", configuration,
        "--parallel",
    ]
    if targets:
        command += ["--target", *targets]
    run(command, PROJECT_DIR, env=environment)
