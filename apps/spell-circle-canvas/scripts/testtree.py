"""The tests a configured tree registers, read from ctest.

coverage.py and sanitize.py each maintain a secondary tree beside the
primary build/ and need two things from its test registration before
and after building: which targets a test filter needs built, and which
executables the selected tests run. Both come from `ctest --show-only`,
so neither script parses generated files. This is a module, not a
command — it has no main.
"""

import json
import os
import subprocess
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent  # apps/spell-circle-canvas


def tests(preset: str, configuration: str, test_filter: str | None) -> list:
    """Every test the preset's tree registers, as (name, command) pairs,
    narrowed by the ctest regex when one is given. A test whose binary
    is not built yet has no command."""
    command = ["ctest", "--preset", preset, "-C", configuration, "--show-only=json-v1"]
    if test_filter:
        command += ["-R", test_filter]
    result = subprocess.run(
        command, cwd=PROJECT_DIR, capture_output=True, text=True, check=True
    )
    return [
        (test["name"], test.get("command"))
        for test in json.loads(result.stdout)["tests"]
    ]


def build_targets(preset: str, configuration: str, test_filter: str) -> list:
    """The targets a test filter needs built. A test in this repository
    is registered under its executable's target name, so an unbuilt test
    names its target and a built one names its binary; a test driven by
    something outside the tree — a build tool, an interpreter — needs no
    target of its own and is left out."""
    binaries = str(PROJECT_DIR / f"build-{preset}" / "bin" / configuration)
    targets = set()
    for name, command in tests(preset, configuration, test_filter):
        if command is None:
            targets.add(name)
        elif command[0].startswith(binaries + os.sep):
            targets.add(Path(command[0]).name)
    return sorted(targets)


def executables(preset: str, configuration: str, test_filter: str | None) -> list:
    """The distinct built test binaries the selected tests run, in test
    order."""
    binaries = str(PROJECT_DIR / f"build-{preset}" / "bin" / configuration)
    found: list = []
    for _, command in tests(preset, configuration, test_filter):
        if not command or not command[0].startswith(binaries + os.sep):
            continue
        executable = Path(command[0])
        if executable.exists() and executable not in found:
            found.append(executable)
    return found
