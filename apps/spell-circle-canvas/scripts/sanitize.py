#!/usr/bin/env python3
"""Sanitizer build + test run, as ONE command.

Configures a dedicated tree — build-asan/ for the default ASan+UBSan
lane, build-tsan/ with --thread — with SPELLCIRCLE_SANITIZE set, reusing
the primary build's preset composition (vcpkg toolchain, Qt prefix) from
CMakeUserPresets.json, builds the test targets, and runs ctest with the
sanitizer runtimes configured to fail tests on findings and print usable
reports. The two lanes are separate trees because ASan and TSan cannot
share a process, and separate from build-coverage/ because each cache
variable change recompiles everything it touches.

Usage (from apps/spell-circle-canvas):
  scripts/sanitize.py                                # ASan+UBSan, full suite
  scripts/sanitize.py --filter 'motion_test|loader_test'
  scripts/sanitize.py --thread --filter 'motion_test'  # TSan lane
  scripts/sanitize.py --keep                         # leave the tree behind

ONE LANE ON DISK AT A TIME. A sanitized tree is a whole second build of
everything the tests reach, and it is worth nothing once the tests have
had their verdict: the run deletes its own tree as its last act, pass or
fail. --keep holds it — for rerunning one test under a debugger, or
reading a report against the objects that produced it — at the price of
carrying that tree until it is removed by hand. So a lane REFUSES TO
START while the other lane's tree stands: the two never share the
machine, and a tree that outlived its run is always a --keep somebody
has to decide about rather than something to silently build beside.

A configure or build failure leaves the tree standing on purpose. Only a
finished ctest run is a verdict, and only a verdict makes the tree
disposable; a build that did not get that far is one to look at.

The primary build/ tree is never touched, and no dependency is added:
the sanitizer runtimes ship with the compiler, and vcpkg's manifest
install is disabled here — the dependency archives are read from the
primary tree's vcpkg_installed/ as-is. Those archives are uninstrumented,
which still catches this repository's bugs (the interceptors wrap every
allocation and libc call regardless of who compiled the caller); the
casualties are three, all of them the same boundary: a check that needs
both sides instrumented, disabled at ASAN_OPTIONS below; dependency
headers that change their own layout under instrumentation, which the
build pins back to what the archives were compiled with; and a
dependency whose headers this tree instantiates too, whose accesses the
thread lane therefore sees while the ordering compiled into its archive
stays invisible — those go in ThreadSanitizerSuppressions.txt, one entry
per dependency, each stating why the ordering is real.

With --filter, only the targets those tests need are built (derived from
the filtered test list; override with --targets). A failing test prints
its full output, sanitizer report included, and the run exits nonzero.
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from buildtree import (
    PROJECT_DIR,
    build,
    configure_shared_tree,
    derived_targets,
    fail,
    preset_environment,
    resolve_preset,
)

# ASan runtime options:
#   detect_leaks=0 — LeakSanitizer does not support macOS on Apple
#     Silicon; with it left on the runtime aborts at startup before any
#     test runs.
#   detect_container_overflow=0 — the container-overflow check compares
#     a container's size against annotations the contained memory only
#     gets when the code that grew it was instrumented. vcpkg archives
#     are not, so a std::string or vector that crossed the dependency
#     boundary reports false overflows. Every other ASan check is
#     unaffected by mixed instrumentation and stays on.
ASAN_OPTIONS = "detect_leaks=0:detect_container_overflow=0"

# Stacks on every UBSan line, not just the first frame; without this a
# report names a file:line and nothing about how execution got there.
UBSAN_OPTIONS = "print_stacktrace=1"

# TSan runtime options:
#   halt_on_error=1 — first race fails the test instead of accumulating a
#     scroll of reports from the same root cause.
#   second_deadlock_stack=1 — lock-inversion reports show both
#     acquisition stacks, without which one side is a guess.
#   suppressions — the dependencies whose ordering this lane cannot see,
#     each with its reason written in the file. Nothing of this
#     repository's own is in it.
TSAN_SUPPRESSIONS = PROJECT_DIR / "ThreadSanitizerSuppressions.txt"
TSAN_OPTIONS = (
    f"halt_on_error=1:second_deadlock_stack=1:suppressions={TSAN_SUPPRESSIONS}"
)


def refuse_if_other_lane_stands(other: Path, this_lane: str) -> None:
    """The two sanitized trees never coexist, so a standing one stops the
    other lane before it configures anything."""
    if not other.exists():
        return
    fail(
        f"{other.name}/ is still on disk, so the {this_lane} lane will not "
        f"start: one sanitized tree at a time. It is there because that "
        f"lane ran with --keep or did not reach a verdict. Read what you "
        f"kept it for, then `rm -rf {other}` and run this again."
    )


def discard_tree(build_dir: Path) -> None:
    """The tree, once the tests have had their verdict."""
    shutil.rmtree(build_dir, ignore_errors=True)
    print(f"removed {build_dir.name}/ (pass --keep to hold it for a rerun)")


def main() -> int:
    argument_parser = argparse.ArgumentParser(
        description=(
            "Sanitized build + test run, in a dedicated build-asan/ (or "
            "build-tsan/) tree that reuses the primary build's vcpkg "
            "dependencies"
        )
    )
    argument_parser.add_argument(
        "--thread",
        action="store_true",
        help=(
            "build and run under ThreadSanitizer (build-tsan/) instead of "
            "the default ASan+UBSan lane (build-asan/)"
        ),
    )
    argument_parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="build configuration for the sanitized tree (default: Debug)",
    )
    argument_parser.add_argument(
        "--filter",
        metavar="REGEX",
        help=(
            "run only the tests matching this regex (passed to ctest -R); "
            "also narrows the build to the targets those tests need"
        ),
    )
    argument_parser.add_argument(
        "--targets",
        nargs="+",
        metavar="TARGET",
        help=(
            "build exactly these targets instead of deriving them from "
            "--filter (default without --filter: everything)"
        ),
    )
    argument_parser.add_argument(
        "--keep",
        action="store_true",
        help=(
            "leave the sanitized tree on disk after the run instead of "
            "deleting it; the other lane then refuses to start until it "
            "is removed by hand"
        ),
    )
    arguments = argument_parser.parse_args()

    if arguments.thread:
        sanitizers = "thread"
        build_dir = PROJECT_DIR / "build-tsan"
        other_dir = PROJECT_DIR / "build-asan"
    else:
        sanitizers = "address;undefined"
        build_dir = PROJECT_DIR / "build-asan"
        other_dir = PROJECT_DIR / "build-tsan"
    refuse_if_other_lane_stands(
        other_dir, "thread" if arguments.thread else "address+undefined"
    )

    resolved = resolve_preset("main")
    environment = preset_environment(resolved)

    configure_shared_tree(
        build_dir,
        {"SPELLCIRCLE_SANITIZE": sanitizers},
        resolved,
        environment,
    )

    targets = arguments.targets
    if targets is None and arguments.filter:
        targets = derived_targets(build_dir, arguments.config, arguments.filter)
        if not targets:
            fail(f"--filter {arguments.filter!r} matches no buildable tests")
        print(f"targets derived from --filter: {' '.join(targets)}")
    build(build_dir, targets or [], arguments.config, environment)

    test_environment = environment.copy()
    if arguments.thread:
        test_environment["TSAN_OPTIONS"] = TSAN_OPTIONS
    else:
        test_environment["ASAN_OPTIONS"] = ASAN_OPTIONS
        test_environment["UBSAN_OPTIONS"] = UBSAN_OPTIONS

    command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "-C",
        arguments.config,
        "--output-on-failure",
    ]
    if arguments.filter:
        command += ["-R", arguments.filter]

    # Run directly rather than through buildtree.run(): a failing test is
    # this script's finding, not an infrastructure error, so the ctest
    # output (which carries the sanitizer report for each failure) flows
    # through and the summary below states the outcome.
    printable = " ".join(command)
    print(f"\n$ {printable}")
    result = subprocess.run(command, cwd=PROJECT_DIR, env=test_environment, check=False)

    lane = "thread" if arguments.thread else "address+undefined"
    scope = (
        f"tests matching {arguments.filter!r}"
        if arguments.filter
        else "full test suite"
    )
    if result.returncode == 0:
        print(f"\nSANITIZE PASS [{lane}]: {scope} clean")
    else:
        print(
            f"\nSANITIZE FAIL [{lane}]: {scope} — sanitizer reports are in "
            "the failing tests' output above; each names the check, the "
            "faulting access, and the stacks involved. Rerun with --keep "
            "to hold the tree and take a failing test into a debugger",
            file=sys.stderr,
        )
    # The verdict is recorded, so the tree has done its work.
    if not arguments.keep:
        discard_tree(build_dir)
    return 0 if result.returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
