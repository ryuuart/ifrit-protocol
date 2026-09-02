#!/usr/bin/env python3
"""The fast gate, as ONE command: the quick checks at once, one verdict.

Runs the checks that answer in seconds beside each other, every one in
its own subprocess with its output held back, and prints one line per
lane as that lane finishes and one verdict line at the end. A failing
lane's captured output is printed under the verdict; a passing lane's is
never printed at all. The run costs the slowest lane, not the sum.

Usage (from apps/spell-circle-canvas):
  scripts/gate.py                # the changed-file gate
  scripts/gate.py --all          # lint and test everything
  scripts/gate.py --tidy         # clang-tidy as a fifth lane
  scripts/gate.py -- --jobs 4    # after --: arguments for plate_ledger.py

THE LANES:

  check   scripts/check.py over the changed files, clang-tidy aside
  tests   the ctest tests the changed files belong to
  plates  the plate ledger's quick tier
  world   the plate ledger's world tier, when the change can move a set
  tidy    clang-tidy over the changed files, with --tidy

WHAT IT DELIBERATELY LEAVES OUT: the plate ledger's full tier, the
sanitizer lanes, and clang-tidy over the whole compile database. Each of
those costs minutes to hours and each is a separate command — they are
the gate before a push. This is the gate before a commit.

IT DOES NOT BUILD, so the verdict is about the tree as it was last
built; `mise run gate` builds first. Every lane reads the primary build/
tree and none of them writes into it.

HOW THE TEST SCOPE IS DERIVED — from the build graph, so it is the
graph's answer rather than a rule about directory names:

  * The tests are read out of the generated CTestTestfile.cmake files,
    followed from the top through their subdirs() entries, which is the
    chain ctest itself reads. A sweep over every such file in the tree
    would also collect the tests a removed directory left behind.
  * Each test's binary is handed to `ninja -t inputs`, which answers
    with the sources and objects that binary is linked from, transitive
    dependencies included. A changed source is looked up directly there.
  * A header is in none of those sets — the graph carries include
    dependencies in its deps log, not in its target graph — so headers
    are resolved against `ninja -t deps`: the objects that recorded
    reading the header, then the tests those objects reach.
  * A file that resolves to no test and could be a build input (a
    source, a header, a shader, a schema, a QML file) means the graph is
    behind the working tree — a file added since the last configure —
    and the lane runs the whole suite and says so on its status line.
    A file the build does not read at all puts no test in scope; a file
    it does read is in the graph and resolves, this repository's
    compile-checked README among them.

A test whose command is not a binary in this tree — a boundary probe run
through cmake, a probe driven by an outside interpreter — has no sources
to match against, so it is reached by --all and by the fallback and not
otherwise.
"""

import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path

from check import BUILD_DIR, CXX_SUFFIXES, PROJECT_DIR, REPO_DIR, changed_files

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_PREFIX = PROJECT_DIR.relative_to(REPO_DIR)

# Suffixes a build input can carry. A changed file with one of these that
# the graph cannot place is the reason to fall back to the whole suite:
# the graph is allowed to be complete about what it knows and is not
# allowed to be trusted about a file added since it was written.
GRAPH_SUFFIXES = CXX_SUFFIXES | {".fbs", ".qml", ".slang", ".metal"}

# The tier that renders the sketches lighting a set is added when the
# change can move one: a source under the libraries that runtime draws
# through, the runtime itself, or a sketch written against it. A sketch
# declares which runtime it is by the header it includes.
WORLD_DIRECTORIES = (
    "src/common/geometry/",
    "src/common/material/",
    "src/common/world/",
    "src/sketch/set/",
)
SET_RUNTIME_HEADER = "sigilsketch/set/Set.h"

# An add_test() line in a generated CTestTestfile.cmake: the test name
# (plain or bracket-quoted) followed by the quoted command, and the
# subdirs() lines that continue the walk.
ADD_TEST_PATTERN = re.compile(
    r"add_test\(\s*(?:\[=*\[)?(?P<name>[^\s\]\)]+?)(?:\]=*\])?\s+"
    r'"(?P<command>[^"]+)"'
)
SUBDIRS_PATTERN = re.compile(r'^subdirs\("([^"]+)"\)', re.MULTILINE)

# A deps-log entry header, `<object>: #deps N, deps mtime M (VALID)`,
# followed by one indented line per file that object recorded reading.
DEPS_ENTRY_PATTERN = re.compile(r"^(?P<object>\S.*?): #deps ")


@dataclass
class Outcome:
    """One lane's verdict, its wall time, and whatever it printed."""

    name: str
    ok: bool
    seconds: float = 0.0
    output: str = ""
    note: str = ""
    ran: bool = True


@dataclass
class Lane:
    """A lane before it runs: a name, and the work that answers for it."""

    name: str
    work: Callable[[], Outcome]


def run_command(command: list, timeout: float) -> tuple[bool, str, str]:
    """Runs one command to completion with its output captured, and
    answers with its verdict, a note for the status line, and everything
    it printed.

    A command that outruns the timeout is killed WITH EVERYTHING IT
    STARTED: the lanes drive test binaries and renderers of their own,
    and signalling only the script would leave those running against the
    build tree the next lane is reading. The two streams are merged so
    the captured output reads in the order it was written.
    """
    process = subprocess.Popen(
        [str(argument) for argument in command],
        cwd=PROJECT_DIR,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        start_new_session=True,
    )
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        output, _ = process.communicate()
        return False, f"timed out after {timeout:g}s", output
    return process.returncode == 0, "", output


def counted(count: int, noun: str) -> str:
    return f"{count} {noun}" if count == 1 else f"{count} {noun}s"


def project_relative(path: Path) -> str:
    """A repository-relative path spelled from the application directory,
    which is how this file's directory rules are written. A path outside
    the application is returned unchanged and matches none of them."""
    try:
        return str(path.relative_to(PROJECT_PREFIX))
    except ValueError:
        return str(path)


def tests_in_tree(configuration: str) -> dict:
    """Test name to the executable it runs, for the tests ctest will run.

    Walks the CTestTestfile.cmake chain from the top of the build tree
    through its subdirs() entries. The multi-config generator writes one
    add_test entry per configuration, so only the command under this
    configuration's bin directory counts. A test whose command is not a
    binary in this tree is left out: it names no sources to scope by.
    """
    marker = f"/bin/{configuration}/"
    tests: dict = {}

    def walk(directory: Path) -> None:
        testfile = directory / "CTestTestfile.cmake"
        if not testfile.is_file():
            return
        text = testfile.read_text()
        for match in ADD_TEST_PATTERN.finditer(text):
            command = Path(match.group("command"))
            if marker in str(command) and command.is_relative_to(BUILD_DIR):
                tests.setdefault(match.group("name"), command)
        for match in SUBDIRS_PATTERN.finditer(text):
            walk(directory / match.group(1))

    walk(BUILD_DIR)
    return tests


def ninja_manifest(configuration: str) -> str | None:
    """The manifest to read the graph from: the multi-config generator
    writes one per configuration beside the dispatching build.ninja."""
    if (BUILD_DIR / f"build-{configuration}.ninja").is_file():
        return f"build-{configuration}.ninja"
    if (BUILD_DIR / "build.ninja").is_file():
        return "build.ninja"
    return None


def graph_tool(manifest: str, tool: str, *arguments: str) -> subprocess.Popen:
    return subprocess.Popen(
        ["ninja", "-C", str(BUILD_DIR), "-f", manifest, "-t", tool, *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )


def input_owners(tests: dict, manifest: str) -> dict:
    """Every input of every test binary, mapped back to the tests it
    reaches. Sources come out of the graph absolute and objects relative;
    both are kept, because a header is resolved to objects and a source
    to itself."""

    def inputs(test: tuple) -> tuple:
        name, executable = test
        relative = str(executable.relative_to(BUILD_DIR))
        process = graph_tool(manifest, "inputs", relative)
        output, _ = process.communicate()
        # A stale entry naming a target the current graph no longer has
        # answers with an error and no inputs; it owns nothing.
        return name, output.split() if process.returncode == 0 else []

    owners: dict = {}
    with ThreadPoolExecutor(max_workers=8) as pool:
        for name, listed in pool.map(inputs, tests.items()):
            for entry in listed:
                owners.setdefault(entry, set()).add(name)
    return owners


def objects_reading(headers: set, manifest: str) -> dict:
    """The objects that recorded reading each of these files, from the
    deps log. Streamed rather than collected: the log holds every header
    of every translation unit ever compiled in this tree, and all that is
    wanted from it is which entries name one of a handful of files."""
    found: dict = {header: set() for header in headers}
    process = graph_tool(manifest, "deps")
    current = None
    for line in process.stdout:
        if line.startswith("    "):
            dependency = line[4:].rstrip("\n")
            if current and dependency in found:
                found[dependency].add(current)
        else:
            match = DEPS_ENTRY_PATTERN.match(line)
            current = match.group("object") if match else None
    process.wait()
    return found


def scoped_tests(changed: list, configuration: str) -> tuple:
    """The tests the changed files belong to, or None for the whole suite.

    None is the answer whenever the graph cannot speak for a file the
    build could be compiling; the second element of the pair says why, in
    the words the lane's status line carries.
    """
    if not shutil.which("ninja"):
        return None, "whole suite: no ninja to read the build graph with"
    manifest = ninja_manifest(configuration)
    if manifest is None:
        return None, f"whole suite: no build graph for {configuration}"
    tests = tests_in_tree(configuration)
    if not tests:
        return None, f"whole suite: no {configuration} tests in the build tree"

    owners = input_owners(tests, manifest)
    selected: set = set()
    unresolved: list = []
    for path in changed:
        absolute = str(REPO_DIR / path)
        if absolute in owners:
            selected |= owners[absolute]
        elif path.suffix in GRAPH_SUFFIXES:
            unresolved.append(absolute)

    if unresolved:
        for header, objects in objects_reading(set(unresolved), manifest).items():
            for object_path in objects:
                selected |= owners.get(object_path, set())
            if objects:
                unresolved.remove(header)
    if unresolved:
        return None, (
            "whole suite: the build graph does not place "
            f"{counted(len(unresolved), 'changed file')}"
        )
    if not selected:
        return [], "nothing the tests are built from changed"
    return sorted(selected), counted(len(selected), "test")


def tests_lane(arguments: argparse.Namespace, changed: list) -> Outcome:
    """Derives the scope and runs ctest over it.

    The derivation runs inside the lane rather than before the lanes
    start, so it costs nothing anyone waits for: the other lanes are
    already running while the graph is being read.
    """
    started = time.monotonic()
    if arguments.all:
        names, note = None, "whole suite"
    else:
        names, note = scoped_tests(changed, arguments.config)
    if names == []:
        return Outcome("tests", ok=True, note=note, ran=False)
    command = [
        "ctest",
        "--test-dir",
        BUILD_DIR,
        "-C",
        arguments.config,
        "--output-on-failure",
    ]
    if names:
        command += ["-R", f"^({'|'.join(names)})$"]
    ok, timed_out, output = run_command(command, arguments.timeout_seconds)
    return Outcome("tests", ok, time.monotonic() - started, output, timed_out or note)


def command_lane(name: str, command: list, timeout: float) -> Outcome:
    started = time.monotonic()
    ok, timed_out, output = run_command(command, timeout)
    return Outcome(name, ok, time.monotonic() - started, output, timed_out)


def touches_a_set(changed: list) -> bool:
    """Whether the change can move a sketch that lights a set."""
    for path in changed:
        spelled = project_relative(path)
        if spelled.startswith(WORLD_DIRECTORIES):
            return True
        if spelled.startswith("src/sketch/sketches/") and path.suffix == ".cpp":
            source = REPO_DIR / path
            if source.is_file() and SET_RUNTIME_HEADER in source.read_text():
                return True
    return False


def report(outcome: Outcome) -> None:
    status = "PASS" if outcome.ok else "FAIL"
    if not outcome.ran:
        status = "SKIP"
    timing = f"{outcome.seconds:6.1f}s" if outcome.ran else " " * 7
    # Flushed: the status lines are the only sign of life while the slower
    # lanes are still running, and a redirected run buffers them otherwise.
    print(
        f"  {outcome.name:<7} {status}  {timing}  {outcome.note}".rstrip(), flush=True
    )


def main() -> int:
    argument_parser = argparse.ArgumentParser(
        description=(
            "The fast gate: lint, the scoped tests and the quick plate "
            "tier side by side, one verdict"
        )
    )
    argument_parser.add_argument(
        "--all",
        action="store_true",
        help=(
            "check every tracked file and run every test, instead of "
            "scoping both to what changed"
        ),
    )
    argument_parser.add_argument(
        "--tidy",
        action="store_true",
        help=(
            "add clang-tidy over the changed files as a lane of its own "
            "(a semantic analysis per translation unit; the whole compile "
            "database stays with scripts/check.py --tidy-all)"
        ),
    )
    argument_parser.add_argument(
        "--config",
        default="Release",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help=(
            "the configuration the tests and the plate tiers read "
            "(default: Release, which is what the plate baselines are)"
        ),
    )
    argument_parser.add_argument(
        "--timeout-seconds",
        type=float,
        default=600,
        metavar="S",
        help=(
            "per-lane ceiling, in seconds (default 600). A lane still "
            "running at the ceiling is killed with everything it started "
            "and fails by name, so one wedged lane cannot hold the "
            "verdict the others already have"
        ),
    )
    argument_parser.add_argument(
        "forwarded",
        nargs="*",
        metavar="ARG",
        help="after --: arguments passed on to plate_ledger.py",
    )
    arguments = argument_parser.parse_args()

    changed = changed_files()
    timeout = arguments.timeout_seconds

    def lane_of(name: str, command: list) -> Lane:
        return Lane(name, lambda: command_lane(name, command, timeout))

    ledger = [
        sys.executable,
        SCRIPT_DIR / "plate_ledger.py",
        "--config",
        arguments.config,
        *arguments.forwarded,
    ]
    check = [sys.executable, SCRIPT_DIR / "check.py", "--skip-tidy"]
    if arguments.all:
        check.append("--all")

    lanes = [
        lane_of("check", check),
        Lane("tests", lambda: tests_lane(arguments, changed)),
        lane_of("plates", [*ledger, "--tier", "quick"]),
    ]
    # The world tier is half the registry rather than a wider sweep of the
    # same one, so it is asked for when the change can move a set — and
    # under --all, which asks for everything the fast lanes cover.
    if arguments.all or touches_a_set(changed):
        lanes.append(lane_of("world", [*ledger, "--tier", "world"]))
    if arguments.tidy:
        lanes.append(
            lane_of("tidy", [sys.executable, SCRIPT_DIR / "check.py", "--tidy-only"])
        )

    print(
        f"{len(lanes)} lanes, config {arguments.config}: "
        f"{', '.join(lane.name for lane in lanes)}"
    )
    outcomes: dict = {}
    with ThreadPoolExecutor(max_workers=len(lanes)) as pool:
        for future in as_completed([pool.submit(lane.work) for lane in lanes]):
            outcome = future.result()
            outcomes[outcome.name] = outcome
            report(outcome)

    failed = [lane.name for lane in lanes if not outcomes[lane.name].ok]
    print()
    if not failed:
        print("GATE: pass")
        return 0
    print(f"GATE: fail — {', '.join(failed)}")
    for name in failed:
        print(f"\n=== {name}")
        print(outcomes[name].output.rstrip())
    return 1


if __name__ == "__main__":
    sys.exit(main())
