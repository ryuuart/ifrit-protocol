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
  scripts/coverage.py --filter 'motion_test|weave_test'
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
import json
import os
import re
import shutil
import subprocess
import sys
import webbrowser
from datetime import datetime
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent  # apps/spell-circle-canvas
USER_PRESETS = PROJECT_DIR / "CMakeUserPresets.json"
PROJECT_PRESETS = PROJECT_DIR / "CMakePresets.json"
PRIMARY_BUILD_DIR = PROJECT_DIR / "build"
COVERAGE_BUILD_DIR = PROJECT_DIR / "build-coverage"

# Source paths the report never counts: prebuilt dependencies, vendored
# code, Qt/moc-generated sources, the build tree's own generated files,
# and the FlatBuffers-generated header.
IGNORE_ALWAYS = [
    r"vcpkg_installed/",
    r"thirdparty/",
    r"_autogen/",
    r"build-coverage/",
    r"SpellCircle_generated\.h",
]

# Test sources all live in directories named test/; excluded from the
# report unless --include-tests asks for them.
IGNORE_TEST_SOURCES = [r"/test/"]


def fail(message: str) -> "NoReturn":
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


def configure(resolved: dict, environment: dict) -> None:
    """Configures the instrumented tree, reusing the primary build's
    toolchain, Qt prefix, and installed vcpkg dependencies."""
    installed_dir = PRIMARY_BUILD_DIR / "vcpkg_installed"
    if not installed_dir.is_dir():
        fail(
            f"no vcpkg_installed at {installed_dir} — configure the primary "
            "build first (scripts/setup.py) so the coverage tree can reuse "
            "its dependencies"
        )

    command = [
        "cmake",
        "-S", PROJECT_DIR,
        "-B", COVERAGE_BUILD_DIR,
        "-G", resolved.get("generator", "Ninja Multi-Config"),
        "-DSPELLCIRCLE_COVERAGE=ON",
        # Reuse the primary tree's installed dependencies read-only; with
        # the manifest install off, configure never writes into them.
        f"-DVCPKG_INSTALLED_DIR={installed_dir}",
        "-DVCPKG_MANIFEST_INSTALL=OFF",
    ]
    for key, value in resolved.get("cacheVariables", {}).items():
        command.append(f"-D{key}={value}")
    run(command, PROJECT_DIR, env=environment)


# An add_test() line in a generated CTestTestfile.cmake: the test name
# (plain or bracket-quoted) followed by the quoted executable path.
ADD_TEST_PATTERN = re.compile(
    r'add_test\(\s*(?:\[=*\[)?(?P<name>[^\s\]\)]+?)(?:\]=*\])?\s+'
    r'"(?P<executable>[^"]+)"'
)


def test_executables(configuration: str,
                     test_filter: str | None) -> list[Path]:
    """The distinct test executables in the coverage tree, in test order.

    Parsed from the generated CTestTestfile.cmake files rather than from
    `ctest --show-only`, because ctest omits a test's command until its
    binary exists — and target derivation needs the paths before anything
    is built. The multi-config generator writes one add_test entry per
    configuration; only paths under this configuration's bin directory
    count. Tests driven by an outside interpreter (a system python, say)
    contribute no instrumented objects and are skipped.
    """
    configuration_marker = f"/bin/{configuration}/"
    executables: list[Path] = []
    for testfile in sorted(COVERAGE_BUILD_DIR.rglob("CTestTestfile.cmake")):
        for match in ADD_TEST_PATTERN.finditer(testfile.read_text()):
            if test_filter and not re.search(test_filter,
                                             match.group("name")):
                continue
            executable = Path(match.group("executable"))
            if not executable.is_relative_to(COVERAGE_BUILD_DIR):
                continue
            if configuration_marker not in str(executable):
                continue
            if executable not in executables:
                executables.append(executable)
    return executables


def build(targets: list[str], configuration: str, environment: dict) -> None:
    command = [
        "cmake", "--build", COVERAGE_BUILD_DIR, "--config", configuration,
        "--parallel",
    ]
    if targets:
        command += ["--target", *targets]
    run(command, PROJECT_DIR, env=environment)


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

    configure(resolved, environment)

    targets = arguments.targets
    if targets is None and arguments.filter:
        # Executable basenames double as CMake target names, app bundles
        # included (the bundle directory carries the target's name).
        targets = sorted(
            {executable.name
             for executable in test_executables(arguments.config,
                                                arguments.filter)}
        )
        if not targets:
            fail(f"--filter {arguments.filter!r} matches no buildable tests")
        print(f"targets derived from --filter: {' '.join(targets)}")
    build(targets or [], arguments.config, environment)

    run_tests(arguments.config, arguments.filter, raw_dir, environment)
    merge_profiles(raw_dir, profdata)

    executables = [
        executable
        for executable in test_executables(arguments.config,
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
