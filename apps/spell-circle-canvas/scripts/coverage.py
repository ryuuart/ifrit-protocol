#!/usr/bin/env python3
"""Source-based code coverage, as ONE command: build, test, merge, report.

Configures a dedicated instrumented tree (build-coverage/) through the
`coverage` preset that scripts/setup.py writes into CMakeUserPresets.json
(the `main` composition plus the coverage flags), builds the test
targets, runs ctest through the matching test preset — whose environment
points every test process at its own raw profile — merges the raw
profiles with llvm-profdata, and prints the llvm-cov summary plus an HTML
report under build/coverage/html/ — in the PRIMARY build directory,
beside the other build artifacts. Each run replaces the report wholesale,
and RUN.txt beside it records the invocation and scope that produced it.

Usage (from apps/spell-circle-canvas):
  scripts/coverage.py                                # full suite
  scripts/coverage.py --filter 'motion_test|weave_'
  scripts/coverage.py --export-lcov coverage.lcov    # for CI consumers
  scripts/coverage.py --open                         # open the HTML index

The primary build/ tree is never touched. Instrumented objects live only
in build-coverage/, and the preset reads the primary tree's
vcpkg_installed/ as-is with the manifest install disabled, so the
coverage tree never duplicates the dependency archives. Prebuilt
dependencies carry no coverage mapping, which is deliberate — the report
covers this repository's sources.

With --filter, only the targets those tests need are built (derived from
the filtered test list; override with --targets). A full run builds
everything, so the first one costs a complete instrumented build; later
runs rebuild incrementally. Coverage of test sources themselves is
excluded by default (test code exercising test code is noise); pass
--include-tests to count it.

The pipeline is Clang's, not Apple's. A Windows port built with clang-cl
keeps it unchanged — the same instrumentation flags, the same
llvm-profdata/llvm-cov from an LLVM distribution; only the tool locator
differs (xcrun pins the Xcode toolchain on macOS, LLVM_ROOT or PATH
resolves the tools elsewhere). MSVC's cl.exe has no source-based
coverage; the alternative there is OpenCppCoverage, PDB-based binary
instrumentation of an ordinary build — deliberately not integrated until
Windows binaries exist to run it on.
"""

import argparse
import os
import shutil
import subprocess
import sys
import webbrowser
from datetime import datetime
from pathlib import Path
from typing import NoReturn

from testtree import PROJECT_DIR, build_targets, executables

PRESET = "coverage"
COVERAGE_BUILD_DIR = PROJECT_DIR / "build-coverage"
PRIMARY_BUILD_DIR = PROJECT_DIR / "build"
# Where the test preset's LLVM_PROFILE_FILE puts the raw profiles.
RAW_DIR = COVERAGE_BUILD_DIR / "coverage" / "raw"

# Source paths the report never counts: prebuilt dependencies,
# Qt/moc-generated sources, the build tree's own generated files, and
# the FlatBuffers-generated header.
IGNORE_ALWAYS = [
    r"vcpkg_installed/",
    r"_autogen/",
    r"build-coverage/",
    r"SpellCircle_generated\.h",
]

# Test sources live in directories named test/ or tests/; excluded from
# the report unless --include-tests asks for them. Both spellings are
# matched because a directory that moved or was named the other way would
# otherwise start counting toward coverage silently, and a regression
# that shows up as a BETTER number is one nobody goes looking for.
IGNORE_TEST_SOURCES = [r"/tests?/"]


def fail(message: str) -> NoReturn:
    print(f"\nERROR: {message}", file=sys.stderr)
    sys.exit(1)


def run(command: list, capture: bool = False) -> subprocess.CompletedProcess:
    """Runs a subprocess visibly (or captured) and fails loudly on error."""
    printable = " ".join(str(argument) for argument in command)
    print(f"\n$ {printable}")
    result = subprocess.run(
        [str(argument) for argument in command],
        cwd=PROJECT_DIR,
        check=False,
        capture_output=capture,
        text=capture,
    )
    if result.returncode != 0:
        if capture and result.stderr:
            print(result.stderr, file=sys.stderr)
        fail(f"command failed (exit {result.returncode}): {printable}")
    return result


def llvm_tool(tool: str) -> list:
    """Resolves an llvm-* tool invocation for this platform.

    On macOS the tool goes through xcrun, which pins it to the active
    Xcode toolchain — the same Clang that produced the instrumented
    objects, so the profile and coverage-map formats agree. Elsewhere
    (or without xcrun) the tool comes from $LLVM_ROOT/bin when LLVM_ROOT
    is set, and from PATH otherwise.
    """
    if sys.platform == "darwin":
        xcrun = shutil.which("xcrun")
        if xcrun:
            return [xcrun, tool]
    llvm_root = os.environ.get("LLVM_ROOT")
    if llvm_root:
        resolved = shutil.which(str(Path(llvm_root) / "bin" / tool))
        if resolved:
            return [resolved]
    resolved = shutil.which(tool)
    if resolved:
        return [resolved]
    fail(
        f"cannot find {tool} — install an LLVM toolchain and put its bin/ "
        "on PATH, or point LLVM_ROOT at its root"
    )


def main() -> int:
    argument_parser = argparse.ArgumentParser(
        description=(
            "Instrumented build + test run + llvm-cov report, in a "
            "dedicated build-coverage/ tree that reuses the primary "
            "build's vcpkg dependencies"
        )
    )
    argument_parser.add_argument(
        "--config",
        default="Debug",
        choices=["Debug", "Release", "RelWithDebInfo"],
        help="build configuration for the instrumented tree (default: Debug)",
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
        "--include-tests",
        action="store_true",
        help=(
            "count test sources in the report (excluded by default: test "
            "code covering test code is noise)"
        ),
    )
    argument_parser.add_argument(
        "--export-lcov",
        metavar="FILE",
        help="also export the coverage data in lcov format to FILE",
    )
    argument_parser.add_argument(
        "--open",
        action="store_true",
        help="open the HTML report index when done",
    )
    arguments = argument_parser.parse_args()

    if not (PRIMARY_BUILD_DIR / "vcpkg_installed").is_dir():
        fail(
            f"no vcpkg_installed under {PRIMARY_BUILD_DIR} — configure the "
            "primary build first (scripts/setup.py) so build-coverage can "
            "reuse its dependencies"
        )
    if not (PROJECT_DIR / "CMakeUserPresets.json").exists():
        fail("no CMakeUserPresets.json — run scripts/setup.py first")

    # Instrumentation intermediates stay with the instrumented tree; the
    # human-facing report lands in the PRIMARY build directory, where the
    # other build artifacts (plate baselines, fetched assets) already live.
    # Each run REPLACES the report wholesale — what sits there is always
    # the last run, never an accumulation — and RUN.txt says which run
    # that was, so a filtered subset can never pass for the full suite.
    profdata = COVERAGE_BUILD_DIR / "coverage" / "coverage.profdata"
    report_dir = PRIMARY_BUILD_DIR / "coverage"
    html_dir = report_dir / "html"

    run(["cmake", "--preset", PRESET])

    targets = arguments.targets
    if targets is None and arguments.filter:
        targets = build_targets(PRESET, arguments.config, arguments.filter)
        if not targets:
            fail(f"--filter {arguments.filter!r} matches no buildable tests")
        print(f"targets derived from --filter: {' '.join(targets)}")
    build = ["cmake", "--build", "--preset", PRESET, "--config", arguments.config]
    build += ["--parallel"]
    if targets:
        build += ["--target", *targets]
    run(build)

    if RAW_DIR.exists():
        shutil.rmtree(RAW_DIR)
    RAW_DIR.mkdir(parents=True)
    test = ["ctest", "--preset", PRESET, "-C", arguments.config]
    if arguments.filter:
        test += ["-R", arguments.filter]
    run(test)

    raw_profiles = sorted(RAW_DIR.glob("*.profraw"))
    if not raw_profiles:
        fail(
            f"no raw profiles in {RAW_DIR} — did every test skip, or was "
            "the tree configured without the coverage flags?"
        )
    print(f"\n{len(raw_profiles)} raw profiles")
    run(
        [*llvm_tool("llvm-profdata"), "merge", "-sparse", *raw_profiles, "-o", profdata]
    )

    objects = executables(PRESET, arguments.config, arguments.filter)
    if not objects:
        fail("no test executables found in the coverage tree")

    ignore_patterns = list(IGNORE_ALWAYS)
    if not arguments.include_tests:
        ignore_patterns += IGNORE_TEST_SOURCES
    # llvm-cov takes the first binary positionally, the rest via -object.
    common_arguments: list = [objects[0]]
    for executable in objects[1:]:
        common_arguments += ["-object", executable]
    common_arguments += [
        f"-instr-profile={profdata}",
        f"-ignore-filename-regex={'|'.join(ignore_patterns)}",
    ]

    run([*llvm_tool("llvm-cov"), "report", *common_arguments])

    if report_dir.exists():
        shutil.rmtree(report_dir)
    report_dir.mkdir(parents=True)
    run(
        [
            *llvm_tool("llvm-cov"),
            "show",
            "--format=html",
            f"-output-dir={html_dir}",
            *common_arguments,
        ]
    )

    if arguments.export_lcov:
        lcov_path = Path(arguments.export_lcov)
        result = run(
            [*llvm_tool("llvm-cov"), "export", "-format=lcov", *common_arguments],
            capture=True,
        )
        lcov_path.write_text(result.stdout)
        print(f"lcov export: {lcov_path}")

    # The report's provenance, beside the report. A reader who finds
    # build/coverage/ a week from now learns what produced it without
    # trusting anyone's memory: the exact invocation, the scope, and when.
    scope = (
        f"test filter: {arguments.filter}" if arguments.filter else "full test suite"
    )
    (report_dir / "RUN.txt").write_text(
        f"generated: {datetime.now().isoformat(timespec='seconds')}\n"
        f"command:   {' '.join(sys.argv)}\n"
        f"config:    {arguments.config}\n"
        f"scope:     {scope}\n"
        f"objects:   {', '.join(e.name for e in objects)}\n"
    )

    index = html_dir / "index.html"
    if arguments.open:
        webbrowser.open(index.as_uri())
    print(f"\nHTML report: {index}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
