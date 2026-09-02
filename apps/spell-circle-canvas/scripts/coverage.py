#!/usr/bin/env python3
"""Source-based code coverage, as ONE command: build, test, merge, report.

Configures a dedicated instrumented tree (build-coverage/) with
SPELLCIRCLE_COVERAGE=ON, reusing the primary build's preset composition
(vcpkg toolchain, Qt prefix) from CMakeUserPresets.json, builds the test
targets, runs ctest with per-process profile emission, merges the raw
profiles with llvm-profdata, and prints the llvm-cov summary plus an HTML
report under build/coverage/html/ — in the PRIMARY build directory, beside
the other build artifacts. Each run replaces the report wholesale, and
RUN.txt beside it records the invocation and scope that produced it.

Usage (from apps/spell-circle-canvas):
  scripts/coverage.py                                # full suite
  scripts/coverage.py --filter 'motion_test|weave_'
  scripts/coverage.py --export-lcov coverage.lcov    # for CI consumers
  scripts/coverage.py --open                         # open the HTML index

The primary build/ tree is never touched. Instrumented objects live only
in build-coverage/, and vcpkg's manifest install is disabled there: the
dependency archives are read from the primary tree's vcpkg_installed/
as-is, so the coverage tree never duplicates them. Prebuilt dependencies
carry no coverage mapping, which is deliberate — the report covers this
repository's sources.

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
import sys
import webbrowser
from datetime import datetime
from pathlib import Path

from buildtree import (
    PRIMARY_BUILD_DIR,
    PROJECT_DIR,
    build,
    configure_shared_tree,
    derived_targets,
    fail,
    preset_environment,
    resolve_preset,
    run,
    test_executables,
)

COVERAGE_BUILD_DIR = PROJECT_DIR / "build-coverage"

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


def run_tests(configuration: str, test_filter: str | None, raw_dir: Path,
              environment: dict) -> None:
    """Runs ctest with each test process writing its own raw profile.

    %p (process id) keeps concurrently running tests from clobbering one
    file; %m (module signature) keeps profiles from differently
    instrumented binaries apart so the merge stays well-formed.
    """
    if raw_dir.exists():
        shutil.rmtree(raw_dir)
    raw_dir.mkdir(parents=True)
    test_environment = environment.copy()
    test_environment["LLVM_PROFILE_FILE"] = str(raw_dir / "%p-%m.profraw")
    command = [
        "ctest", "--test-dir", COVERAGE_BUILD_DIR, "-C", configuration,
        "--output-on-failure",
    ]
    if test_filter:
        command += ["-R", test_filter]
    run(command, PROJECT_DIR, env=test_environment)


def merge_profiles(raw_dir: Path, profdata: Path) -> None:
    raw_profiles = sorted(raw_dir.glob("*.profraw"))
    if not raw_profiles:
        fail(
            f"no raw profiles in {raw_dir} — did every test skip, or was "
            "the tree configured without SPELLCIRCLE_COVERAGE?"
        )
    print(f"\n{len(raw_profiles)} raw profiles")
    run(
        [*llvm_tool("llvm-profdata"), "merge", "-sparse",
         *raw_profiles, "-o", profdata],
        PROJECT_DIR,
    )


def coverage_object_arguments(executables: list[Path]) -> list:
    """llvm-cov takes the first binary positionally, the rest via -object."""
    arguments: list = [executables[0]]
    for executable in executables[1:]:
        arguments += ["-object", executable]
    return arguments


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

    resolved = resolve_preset("main")
    environment = preset_environment(resolved)

    # Instrumentation intermediates stay with the instrumented tree; the
    # human-facing report lands in the PRIMARY build directory, where the
    # other build artifacts (plate baselines, fetched assets) already live.
    # Each run REPLACES the report wholesale — what sits there is always
    # the last run, never an accumulation — and RUN.txt says which run
    # that was, so a filtered subset can never pass for the full suite.
    coverage_dir = COVERAGE_BUILD_DIR / "coverage"
    raw_dir = coverage_dir / "raw"
    profdata = coverage_dir / "coverage.profdata"
    report_dir = PRIMARY_BUILD_DIR / "coverage"
    html_dir = report_dir / "html"

    configure_shared_tree(
        COVERAGE_BUILD_DIR, {"SPELLCIRCLE_COVERAGE": "ON"},
        resolved, environment,
    )

    targets = arguments.targets
    if targets is None and arguments.filter:
        targets = derived_targets(COVERAGE_BUILD_DIR, arguments.config,
                                  arguments.filter)
        if not targets:
            fail(f"--filter {arguments.filter!r} matches no buildable tests")
        print(f"targets derived from --filter: {' '.join(targets)}")
    build(COVERAGE_BUILD_DIR, targets or [], arguments.config, environment)

    run_tests(arguments.config, arguments.filter, raw_dir, environment)
    merge_profiles(raw_dir, profdata)

    executables = [
        executable
        for executable in test_executables(COVERAGE_BUILD_DIR,
                                           arguments.config,
                                           arguments.filter)
        if executable.exists()
    ]
    if not executables:
        fail("no test executables found in the coverage tree")

    ignore_patterns = list(IGNORE_ALWAYS)
    if not arguments.include_tests:
        ignore_patterns += IGNORE_TEST_SOURCES
    ignore_regex = "|".join(ignore_patterns)

    object_arguments = coverage_object_arguments(executables)
    common_arguments = [
        *object_arguments,
        f"-instr-profile={profdata}",
        f"-ignore-filename-regex={ignore_regex}",
    ]

    run([*llvm_tool("llvm-cov"), "report", *common_arguments], PROJECT_DIR)

    if report_dir.exists():
        shutil.rmtree(report_dir)
    report_dir.mkdir(parents=True)
    run(
        [*llvm_tool("llvm-cov"), "show", "--format=html",
         f"-output-dir={html_dir}", *common_arguments],
        PROJECT_DIR,
    )

    if arguments.export_lcov:
        lcov_path = Path(arguments.export_lcov)
        result = run(
            [*llvm_tool("llvm-cov"), "export", "-format=lcov",
             *common_arguments],
            PROJECT_DIR,
            capture=True,
        )
        lcov_path.write_text(result.stdout)
        print(f"lcov export: {lcov_path}")

    # The report's provenance, beside the report. A reader who finds
    # build/coverage/ a week from now learns what produced it without
    # trusting anyone's memory: the exact invocation, the scope, and when.
    scope = (f"test filter: {arguments.filter}" if arguments.filter
             else "full test suite")
    (report_dir / "RUN.txt").write_text(
        f"generated: {datetime.now().isoformat(timespec='seconds')}\n"
        f"command:   {' '.join(sys.argv)}\n"
        f"config:    {arguments.config}\n"
        f"scope:     {scope}\n"
        f"objects:   {', '.join(e.name for e in executables)}\n")

    index = html_dir / "index.html"
    if arguments.open:
        webbrowser.open(index.as_uri())
    print(f"\nHTML report: {index}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
