"""Verb: sanitize — an instrumented tree, built, tested and judged.

    sigil.py sanitize                                  # ASan+UBSan, full suite
    sigil.py sanitize --filter 'motion_test|io_test'
    sigil.py sanitize --lane thread --filter motion_test
    sigil.py sanitize --keep                           # leave the tree behind
    sigil.py sanitize --lane coverage                  # llvm-cov instead
    sigil.py sanitize --lane coverage --filter 'motion_test|weave_' --open
    sigil.py sanitize --lane coverage --export-lcov coverage.lcov

THREE LANES, ONE BODY: `address` (ASan+UBSan, the default, in
build-asan/), `thread` (TSan, build-tsan/) and `coverage` (LLVM
source-based profiling, build-coverage/). Each is the `main` composition
plus one instrumentation switch, configured through the preset the setup
verb wrote, so what is left per lane is a preflight, a build, a ctest and
what CMake cannot do. The primary build/ tree is never touched: each
preset reads its vcpkg_installed/ as-is with the manifest install
disabled.

ONE SANITIZED TREE ON DISK AT A TIME. A sanitized tree is a whole second
build of everything the tests reach and is worth nothing once the tests
have had their verdict, so the run deletes its own tree as its last act,
pass or fail, and refuses to start while the other sanitizer lane's tree
stands. `--keep` holds one for a debugger, at the price of carrying it
until it is removed by hand. A configure or build failure leaves the tree
standing: only a finished ctest run is a verdict, and only a verdict makes
the tree disposable. The coverage tree is not on that rule — its profiles
and objects are what the report is read from.

What the uninstrumented vcpkg archives cost these lanes, why each runtime
option is set, and what the report leaves out is scripts/README.md.
"""

import argparse
import os
import shutil
import subprocess
import sys
import webbrowser
from datetime import datetime
from pathlib import Path

from sigil import tree

# The preset each lane configures with, which is also its tree's suffix.
LANES = {"address": "asan", "thread": "tsan", "coverage": "coverage"}
SANITIZER_LANES = ("address", "thread")

# Source paths the coverage report never counts: prebuilt dependencies,
# Qt/moc-generated sources, the build tree's own generated files, and the
# FlatBuffers-generated header.
IGNORE_ALWAYS = [
    r"vcpkg_installed/",
    r"_autogen/",
    r"build-coverage/",
    r"SpellCircle_generated\.h",
]

# Test sources, left out of the report unless --include-tests asks for
# them. Both spellings are matched.
IGNORE_TEST_SOURCES = [r"/tests?/"]


def llvm_tool(tool: str) -> list:
    """Resolves an llvm-* tool invocation for this platform.

    On macOS the tool goes through xcrun, which pins it to the active
    Xcode toolchain — the same Clang that produced the instrumented
    objects, so the profile and coverage-map formats agree. Elsewhere the
    tool comes from $LLVM_ROOT/bin when LLVM_ROOT is set, and from PATH
    otherwise."""
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
    tree.fail(
        f"cannot find {tool} — install an LLVM toolchain and put its bin/ on "
        "PATH, or point LLVM_ROOT at its root"
    )


def must_run(command: list, capture: bool = False) -> subprocess.CompletedProcess:
    """Runs a step whose failure is an infrastructure error, not a finding."""
    printable = " ".join(str(argument) for argument in command)
    print(f"\n$ {printable}")
    result = subprocess.run(
        [str(argument) for argument in command],
        cwd=tree.PROJECT_DIR,
        check=False,
        capture_output=capture,
        text=capture,
    )
    if result.returncode != 0:
        if capture and result.stderr:
            print(result.stderr, file=sys.stderr)
        tree.fail(f"command failed (exit {result.returncode}): {printable}")
    return result


def configure_and_build(preset: str, args) -> int:
    """Configure, derive the targets a filter needs, build. Returns the
    first non-zero exit code, or 0."""
    code = tree.run(["cmake", "--preset", preset])
    if code != 0:
        return code
    targets = args.targets
    if targets is None and args.filter:
        targets = tree.build_targets(preset, args.config, args.filter)
        if not targets:
            tree.fail(f"--filter {args.filter!r} matches no buildable tests")
        print(f"targets derived from --filter: {' '.join(targets)}")
    build = [
        "cmake",
        "--build",
        "--preset",
        preset,
        "--config",
        args.config,
        "--parallel",
    ]
    if targets:
        build += ["--target", *targets]
    return tree.run(build)


def ctest(preset: str, args) -> int:
    command = ["ctest", "--preset", preset, "-C", args.config]
    if args.filter:
        command += ["-R", args.filter]
    return tree.run(command)


def sanitizer_lane(lane: str, args) -> int:
    preset = LANES[lane]
    other = LANES[next(name for name in SANITIZER_LANES if name != lane)]
    build_dir = tree.build_dir(preset)
    other_dir = tree.build_dir(other)

    # The two sanitized trees never coexist, so a standing one stops the
    # other lane before it configures anything.
    if other_dir.exists():
        tree.fail(
            f"{other_dir.name}/ is still on disk, so the {lane} lane will not "
            f"start: one sanitized tree at a time. It is there because that "
            f"lane ran with --keep or did not reach a verdict. Read what you "
            f"kept it for, then `rm -rf {other_dir}` and run this again."
        )
    tree.require_primary_tree(build_dir.name)

    if configure_and_build(preset, args) != 0:
        tree.fail(f"{build_dir.name}/ is left for inspection")

    # A failing test is this verb's finding, not an infrastructure error:
    # the ctest output, which carries the sanitizer report for each
    # failure, flows through and the summary below states the outcome.
    code = ctest(preset, args)
    scope = f"tests matching {args.filter!r}" if args.filter else "full test suite"
    if code == 0:
        print(f"\nSANITIZE PASS [{lane}]: {scope} clean")
    else:
        print(
            f"\nSANITIZE FAIL [{lane}]: {scope} — sanitizer reports are in the "
            "failing tests' output above; each names the check, the faulting "
            "access, and the stacks involved. Rerun with --keep to hold the "
            "tree and take a failing test into a debugger",
            file=sys.stderr,
        )
    # The verdict is recorded, so the tree has done its work.
    if not args.keep:
        shutil.rmtree(build_dir, ignore_errors=True)
        print(f"removed {build_dir.name}/ (pass --keep to hold it for a rerun)")
    return 0 if code == 0 else 1


def coverage_lane(args) -> int:
    preset = LANES["coverage"]
    build_dir = tree.build_dir(preset)
    tree.require_primary_tree(build_dir.name)

    # Instrumentation intermediates stay with the instrumented tree; the
    # human-facing report lands in the PRIMARY build directory, where the
    # other build artifacts already live. Each run REPLACES the report
    # wholesale, and RUN.txt says which run that was, so a filtered subset
    # can never pass for the full suite.
    raw_dir = build_dir / "coverage" / "raw"
    profdata = build_dir / "coverage" / "coverage.profdata"
    report_dir = tree.build_dir() / "coverage"
    html_dir = report_dir / "html"

    if configure_and_build(preset, args) != 0:
        tree.fail(f"{build_dir.name}/ is left for inspection")

    if raw_dir.exists():
        shutil.rmtree(raw_dir)
    raw_dir.mkdir(parents=True)
    ctest(preset, args)

    raw_profiles = sorted(raw_dir.glob("*.profraw"))
    if not raw_profiles:
        tree.fail(
            f"no raw profiles in {raw_dir} — did every test skip, or was the "
            "tree configured without the coverage flags?"
        )
    print(f"\n{len(raw_profiles)} raw profiles")
    must_run(
        [*llvm_tool("llvm-profdata"), "merge", "-sparse", *raw_profiles, "-o", profdata]
    )

    objects = tree.test_executables(preset, args.config, args.filter)
    if not objects:
        tree.fail("no test executables found in the coverage tree")

    ignore = list(IGNORE_ALWAYS) + ([] if args.include_tests else IGNORE_TEST_SOURCES)
    # llvm-cov takes the first binary positionally, the rest via -object.
    common: list = [objects[0]]
    for executable in objects[1:]:
        common += ["-object", executable]
    common += [
        f"-instr-profile={profdata}",
        f"-ignore-filename-regex={'|'.join(ignore)}",
    ]

    must_run([*llvm_tool("llvm-cov"), "report", *common])

    if report_dir.exists():
        shutil.rmtree(report_dir)
    report_dir.mkdir(parents=True)
    must_run(
        [
            *llvm_tool("llvm-cov"),
            "show",
            "--format=html",
            f"-output-dir={html_dir}",
            *common,
        ]
    )

    if args.export_lcov:
        lcov_path = Path(args.export_lcov)
        result = must_run(
            [*llvm_tool("llvm-cov"), "export", "-format=lcov", *common], capture=True
        )
        lcov_path.write_text(result.stdout)
        print(f"lcov export: {lcov_path}")

    # The report's provenance, beside the report. A reader who finds
    # build/coverage/ a week from now learns what produced it without
    # trusting anyone's memory: the exact invocation, the scope, and when.
    scope = f"test filter: {args.filter}" if args.filter else "full test suite"
    (report_dir / "RUN.txt").write_text(
        f"generated: {datetime.now().isoformat(timespec='seconds')}\n"
        f"command:   {' '.join(sys.argv)}\n"
        f"config:    {args.config}\n"
        f"scope:     {scope}\n"
        f"objects:   {', '.join(executable.name for executable in objects)}\n"
    )

    index = html_dir / "index.html"
    if args.open:
        webbrowser.open(index.as_uri())
    print(f"\nHTML report: {index}")
    return 0


def main(argv: list) -> int:
    ap = argparse.ArgumentParser(
        prog="sigil.py sanitize",
        description="an instrumented build and test run in a dedicated tree "
        "that reuses the primary build's vcpkg dependencies",
    )
    ap.add_argument(
        "--lane",
        choices=tuple(LANES),
        default="address",
        help="address (default): ASan+UBSan in build-asan/. thread: TSan in "
        "build-tsan/. coverage: LLVM source-based profiling in "
        "build-coverage/, with an llvm-cov report",
    )
    ap.add_argument(
        "--config",
        default="RelWithDebInfo",
        choices=tree.CONFIGURATIONS,
        help="build configuration for the instrumented tree (default: RelWithDebInfo)",
    )
    ap.add_argument(
        "--filter",
        metavar="REGEX",
        help="run only the tests matching this regex (passed to ctest -R); "
        "also narrows the build to the targets those tests need",
    )
    ap.add_argument(
        "--targets",
        nargs="+",
        metavar="TARGET",
        help="build exactly these targets instead of deriving them from "
        "--filter (default without --filter: everything)",
    )
    ap.add_argument(
        "--keep",
        action="store_true",
        help="leave a sanitized tree on disk after the run; the other lane "
        "then refuses to start until it is removed by hand",
    )
    ap.add_argument(
        "--include-tests",
        action="store_true",
        help="coverage lane: count test sources in the report (excluded by "
        "default: test code covering test code is noise)",
    )
    ap.add_argument(
        "--export-lcov",
        metavar="FILE",
        help="coverage lane: also export the coverage data in lcov format",
    )
    ap.add_argument(
        "--open",
        action="store_true",
        help="coverage lane: open the HTML report index when done",
    )
    args = ap.parse_args(argv)

    if args.lane == "coverage":
        return coverage_lane(args)
    return sanitizer_lane(args.lane, args)
