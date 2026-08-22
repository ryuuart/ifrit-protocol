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

The primary build/ tree is never touched, and no dependency is added:
the sanitizer runtimes ship with the compiler, and vcpkg's manifest
install is disabled here — the dependency archives are read from the
primary tree's vcpkg_installed/ as-is. Those archives are uninstrumented,
which still catches this repository's bugs (the interceptors wrap every
allocation and libc call regardless of who compiled the caller); the
casualties are two, both of them the same boundary: a check that needs
both sides instrumented, disabled at ASAN_OPTIONS below, and dependency
headers that change their own layout under instrumentation, which the
build pins back to what the archives were compiled with.

With --filter, only the targets those tests need are built (derived from
the filtered test list; override with --targets). A failing test prints
its full output, sanitizer report included, and the run exits nonzero.
"""

import argparse
import subprocess
import sys

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
TSAN_OPTIONS = "halt_on_error=1:second_deadlock_stack=1"


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
    arguments = argument_parser.parse_args()

    if arguments.thread:
        sanitizers = "thread"
        build_dir = PROJECT_DIR / "build-tsan"
    else:
        sanitizers = "address;undefined"
        build_dir = PROJECT_DIR / "build-asan"

    resolved = resolve_preset("main")
    environment = preset_environment(resolved)

    configure_shared_tree(
        build_dir, {"SPELLCIRCLE_SANITIZE": sanitizers},
        resolved, environment,
    )

    targets = arguments.targets
    if targets is None and arguments.filter:
        targets = derived_targets(build_dir, arguments.config,
                                  arguments.filter)
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
        "ctest", "--test-dir", str(build_dir), "-C", arguments.config,
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
    result = subprocess.run(command, cwd=PROJECT_DIR, env=test_environment,
                            check=False)

    lane = "thread" if arguments.thread else "address+undefined"
    scope = (f"tests matching {arguments.filter!r}" if arguments.filter
             else "full test suite")
    if result.returncode == 0:
        print(f"\nSANITIZE PASS [{lane}]: {scope} clean")
        return 0
    print(
        f"\nSANITIZE FAIL [{lane}]: {scope} — sanitizer reports are in "
        "the failing tests' output above; each names the check, the "
        "faulting access, and the stacks involved",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
