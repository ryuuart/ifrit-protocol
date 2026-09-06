#!/usr/bin/env python3
"""Sanitizer build + test run, as ONE command.

Configures a dedicated tree — build-asan/ for the default ASan+UBSan
lane, build-tsan/ with --thread — through the `asan` or `tsan` preset
that scripts/setup.py writes into CMakeUserPresets.json, builds the test
targets, and runs ctest through the matching test preset, whose
environment carries the sanitizer runtime options.

Usage (from apps/spell-circle-canvas):
  scripts/sanitize.py                                # ASan+UBSan, full suite
  scripts/sanitize.py --filter 'motion_test|io_test'
  scripts/sanitize.py --thread --filter 'motion_test'  # TSan lane
  scripts/sanitize.py --keep                         # leave the tree behind

ONE LANE ON DISK AT A TIME. A sanitized tree is a whole second build of
everything the tests reach, and it is worth nothing once the tests have
had their verdict: the run deletes its own tree as its last act, pass or
fail, and refuses to start while the other lane's tree stands. --keep
holds one for a debugger, at the price of carrying it until it is
removed by hand. A configure or build failure leaves the tree standing.

What the uninstrumented vcpkg archives cost this lane, and why each
runtime option is set, is scripts/README.md.
"""

import argparse
import shutil
import subprocess
import sys

from testtree import PROJECT_DIR, build_targets

PRIMARY_INSTALLED = PROJECT_DIR / "build" / "vcpkg_installed"


def fail(message: str) -> int:
    print(f"\nERROR: {message}", file=sys.stderr)
    return 1


def run(command: list) -> int:
    printable = " ".join(str(argument) for argument in command)
    print(f"\n$ {printable}")
    return subprocess.run(
        [str(argument) for argument in command], cwd=PROJECT_DIR, check=False
    ).returncode


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
        default="RelWithDebInfo",
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

    preset = "tsan" if arguments.thread else "asan"
    other = "asan" if arguments.thread else "tsan"
    lane = "thread" if arguments.thread else "address+undefined"
    build_dir = PROJECT_DIR / f"build-{preset}"
    other_dir = PROJECT_DIR / f"build-{other}"

    # The two sanitized trees never coexist, so a standing one stops the
    # other lane before it configures anything.
    if other_dir.exists():
        return fail(
            f"{other_dir.name}/ is still on disk, so the {lane} lane will not "
            f"start: one sanitized tree at a time. It is there because that "
            f"lane ran with --keep or did not reach a verdict. Read what you "
            f"kept it for, then `rm -rf {other_dir}` and run this again."
        )
    if not PRIMARY_INSTALLED.is_dir():
        return fail(
            f"no vcpkg_installed at {PRIMARY_INSTALLED} — configure the primary "
            f"build first (scripts/setup.py) so {build_dir.name} can reuse "
            "its dependencies"
        )
    if not (PROJECT_DIR / "CMakeUserPresets.json").exists():
        return fail("no CMakeUserPresets.json — run scripts/setup.py first")

    if run(["cmake", "--preset", preset]) != 0:
        return fail(f"configure failed; {build_dir.name}/ is left for inspection")

    targets = arguments.targets
    if targets is None and arguments.filter:
        targets = build_targets(preset, arguments.config, arguments.filter)
        if not targets:
            return fail(f"--filter {arguments.filter!r} matches no buildable tests")
        print(f"targets derived from --filter: {' '.join(targets)}")
    build = ["cmake", "--build", "--preset", preset, "--config", arguments.config]
    build += ["--parallel"]
    if targets:
        build += ["--target", *targets]
    if run(build) != 0:
        return fail(f"build failed; {build_dir.name}/ is left for inspection")

    # A failing test is this script's finding, not an infrastructure
    # error: the ctest output, which carries the sanitizer report for each
    # failure, flows through and the summary below states the outcome.
    test = ["ctest", "--preset", preset, "-C", arguments.config]
    if arguments.filter:
        test += ["-R", arguments.filter]
    returncode = run(test)

    scope = (
        f"tests matching {arguments.filter!r}"
        if arguments.filter
        else "full test suite"
    )
    if returncode == 0:
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
        shutil.rmtree(build_dir, ignore_errors=True)
        print(f"removed {build_dir.name}/ (pass --keep to hold it for a rerun)")
    return 0 if returncode == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
