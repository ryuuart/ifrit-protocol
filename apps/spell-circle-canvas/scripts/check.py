#!/usr/bin/env python3
"""Format and lint gate, as ONE command: clang-format, ruff, qmllint,
clang-tidy.

Checks are read-only by default and scoped to CHANGED files — everything
git sees as modified, staged, or untracked — so the routine invocation
polices the work in progress and nothing else. The configs live at the
repository root (.clang-format with .clang-format-ignore, .clang-tidy,
.clangd, ruff.toml); this script only selects files and runs the tools
against those configs.

Usage (from anywhere in the repository):
  scripts/check.py             # changed files, all four tools
  scripts/check.py --all       # whole tree (clang-tidy stays scoped to
                               # changed files; see --tidy-all)
  scripts/check.py --fix       # apply clang-format and ruff fixes to
                               # the scoped files, then report what
                               # remains
  scripts/check.py --tidy-all  # clang-tidy over every translation unit
                               # in the compile database (slow: a full
                               # semantic analysis per TU)
  scripts/check.py --skip-tidy # just the fast checks
  scripts/check.py --tidy-only # just clang-tidy — the other half of
                               # --skip-tidy, for running the two at
                               # once as separate lanes
  scripts/check.py FILE...     # check exactly these files

Exit status is non-zero when any tool reports a finding.

Tool provenance, macOS: clang-format comes from the Xcode toolchain via
xcrun; qmllint from the Qt prefix recorded in CMakeUserPresets.json by
scripts/setup.py; ruff from PATH (brew install ruff); clang-tidy from a
Homebrew LLVM (brew install llvm — Apple's toolchain does not ship it).
Without clang-tidy the tidy section is skipped and says so; the other
three are required. clang-tidy and the .clangd config both need the
compile database at apps/spell-circle-canvas/build (configured with
CMAKE_EXPORT_COMPILE_COMMANDS, which setup.py's presets already do).
The database records /usr/bin/c++ without -isysroot — the Apple driver
injects the SDK path itself at build time — so the non-Apple clang-tidy
is passed the SDK root explicitly here; without it every standard
header is unresolvable. That SDK root also moves the default
/usr/local/include search under the SDK, so a package installed there —
Ultralight's headers — stops resolving, and the directory is appended
after the system ones to restore it. A translation unit whose headers do
not resolve still reports findings, and they describe an AST the
compiler never built: every one of them is noise.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from typing import NoReturn

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent  # apps/spell-circle-canvas
REPO_DIR = PROJECT_DIR.parent.parent
BUILD_DIR = PROJECT_DIR / "build"
USER_PRESETS = PROJECT_DIR / "CMakeUserPresets.json"
PROJECT_PRESETS = PROJECT_DIR / "CMakePresets.json"

CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".c", ".h", ".hh", ".hpp", ".mm", ".m"}
TIDY_SUFFIXES = {".cpp", ".cc", ".cxx", ".mm"}

# Paths no checker touches: vendored and generated sources. Mirrors
# .clang-format-ignore (which only clang-format 18+ reads on its own)
# and ruff.toml's extend-exclude, so the selection holds even when a
# tool predates its ignore mechanism.
EXCLUDED_FRAGMENTS = (
    "thirdparty/",
    "SpellCircle_generated",
    "vcpkg_installed/",
)


def fail(message: str) -> NoReturn:
    print(f"\nERROR: {message}", file=sys.stderr)
    sys.exit(1)


def run(command: list, capture: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(
        [str(argument) for argument in command],
        cwd=REPO_DIR,
        check=False,
        capture_output=capture,
        text=True,
    )


def section(title: str) -> None:
    print(f"\n=== {title}")


def qt_prefix() -> Path | None:
    """The Qt installation the build uses, from the configure presets.

    Walks the "main" preset's inherits chain across CMakeUserPresets.json
    and CMakePresets.json merging cacheVariables the way CMake does (the
    preset itself wins over anything it inherits), then reads
    CMAKE_PREFIX_PATH. Returns None when no presets exist yet — setup.py
    has not been run — and the qmllint section reports that instead of
    guessing at a Qt.
    """
    presets_by_name: dict[str, dict] = {}
    for presets_file in (PROJECT_PRESETS, USER_PRESETS):
        if not presets_file.exists():
            continue
        document = json.loads(presets_file.read_text())
        for preset in document.get("configurePresets", []):
            presets_by_name.setdefault(preset["name"], preset)
    if "main" not in presets_by_name:
        return None

    def cache_variables(name: str) -> dict:
        preset = presets_by_name.get(name, {})
        inherits = preset.get("inherits", [])
        if isinstance(inherits, str):
            inherits = [inherits]
        merged: dict = {}
        for parent in reversed(inherits):
            merged.update(cache_variables(parent))
        merged.update(preset.get("cacheVariables", {}))
        return merged

    prefix = cache_variables("main").get("CMAKE_PREFIX_PATH")
    return Path(prefix) if prefix else None


def changed_files() -> list[Path]:
    """Everything git currently sees as work: modified against HEAD
    (staged or not) plus untracked files that are not ignored."""
    listed: list[str] = []
    for command in (
        ["git", "diff", "--name-only", "HEAD"],
        ["git", "ls-files", "--others", "--exclude-standard"],
    ):
        result = run(command)
        if result.returncode != 0:
            fail(f"git failed: {' '.join(command)}\n{result.stderr}")
        listed += result.stdout.splitlines()
    unique = sorted(set(listed))
    return [Path(name) for name in unique if (REPO_DIR / name).is_file()]


def all_files() -> list[Path]:
    result = run(["git", "ls-files"])
    if result.returncode != 0:
        fail("git ls-files failed")
    return [
        Path(name) for name in result.stdout.splitlines() if (REPO_DIR / name).is_file()
    ]


def included(path: Path) -> bool:
    return not any(fragment in str(path) for fragment in EXCLUDED_FRAGMENTS)


def with_suffixes(files: list[Path], suffixes: set) -> list[Path]:
    return [f for f in files if f.suffix in suffixes and included(f)]


def check_clang_format(files: list[Path], fix: bool) -> bool:
    section("clang-format (C++/ObjC++)")
    if not files:
        print("no files in scope")
        return True
    xcrun = shutil.which("xcrun")
    if not xcrun:
        fail("xcrun not found — install the Xcode command line tools")
    if fix:
        result = run([xcrun, "clang-format", "-i", *files])
        if result.returncode != 0:
            fail(f"clang-format -i failed:\n{result.stderr}")
        print(f"formatted {len(files)} files")
        return True
    result = run([xcrun, "clang-format", "--dry-run", "-Werror", *files])
    if result.returncode == 0:
        print(f"{len(files)} files clean")
        return True
    # One line per offending file, not the full replacement dump.
    offending = sorted(
        {
            line.split(":", 1)[0]
            for line in result.stderr.splitlines()
            if "clang-format-violations" in line
        }
    )
    for name in offending:
        print(f"needs formatting: {name}")
    print(
        f"{len(offending)} of {len(files)} files need clang-format "
        "(scripts/check.py --fix applies it)"
    )
    return False


def check_ruff(files: list[Path], fix: bool, whole_tree: bool) -> bool:
    section("ruff (Python lint + format)")
    if not whole_tree and not files:
        print("no files in scope")
        return True
    if not shutil.which("ruff"):
        fail("ruff not found — brew install ruff")
    # Whole-tree mode hands ruff the repository root so ruff.toml's own
    # excludes drive selection; scoped mode hands it the exact files.
    targets = ["."] if whole_tree else [str(f) for f in files]
    ok = True
    if fix:
        run(["ruff", "check", "--fix", *targets], capture=False)
        run(["ruff", "format", *targets], capture=False)
    check = run(["ruff", "check", *targets], capture=False)
    if check.returncode != 0:
        ok = False
    fmt = run(["ruff", "format", "--check", *targets], capture=False)
    if fmt.returncode != 0:
        ok = False
    if ok:
        print("ruff clean")
    return ok


def check_qmllint(files: list[Path]) -> bool:
    section("qmllint (QML)")
    if not files:
        print("no files in scope")
        return True
    prefix = qt_prefix()
    if prefix is None:
        print(
            "SKIPPED: no configure presets — run scripts/setup.py to "
            "record the Qt prefix"
        )
        return True
    qmllint = prefix / "bin" / "qmllint"
    if not qmllint.exists():
        fail(f"qmllint not found at {qmllint}")
    # The build tree's qml/ holds the repository's own compiled modules
    # (Ifrit.Ui, SpellCircle.*); without it every project import is
    # unresolvable and the lint is meaningless.
    import_dir = BUILD_DIR / "qml"
    command = [qmllint]
    if import_dir.is_dir():
        command += ["-I", import_dir]
    else:
        print(
            f"note: {import_dir} missing (project imports unresolved "
            "until the app is built)"
        )
    result = run([*command, *files], capture=False)
    if result.returncode == 0:
        print(f"{len(files)} files pass")
        return True
    return False


def find_clang_tidy() -> str | None:
    """Homebrew LLVM first (Apple's toolchain has no clang-tidy), then
    PATH for non-brew installs."""
    for candidate in (
        "/opt/homebrew/opt/llvm/bin/clang-tidy",
        "/usr/local/opt/llvm/bin/clang-tidy",
    ):
        if Path(candidate).exists():
            return candidate
    return shutil.which("clang-tidy")


def check_clang_tidy(files: list[Path], tidy_all: bool) -> bool:
    section("clang-tidy (C++ static analysis)")
    tidy = find_clang_tidy()
    if not tidy:
        print("SKIPPED: clang-tidy not installed — brew install llvm")
        return True
    database_path = BUILD_DIR / "compile_commands.json"
    if not database_path.exists():
        print(
            f"SKIPPED: no compile database at {database_path} — "
            "configure the build first (scripts/setup.py)"
        )
        return True
    database = {entry["file"] for entry in json.loads(database_path.read_text())}
    if tidy_all:
        # Generated translation units (moc compilations under *_autogen,
        # anything else the build wrote) are not this repository's sources:
        # they are tidied nowhere, and one from an unbuilt configuration
        # cannot even be parsed.
        # Boundary probes are translation units that MUST fail to compile
        # (their ctest pins the error text); analyzing one reports its
        # deliberate failure as a finding.
        # The suffix restriction is not cosmetic: the compile database also
        # carries the macOS app's Swift translation units, and a C++
        # analyzer handed one fails outright rather than reporting nothing.
        candidates = sorted(
            Path(f).relative_to(REPO_DIR)
            for f in database
            if included(Path(f))
            and Path(f).suffix in TIDY_SUFFIXES
            and not Path(f).is_relative_to(BUILD_DIR)
            and Path(f).name != "BoundaryProbe.cpp"
        )
    else:
        # Only translation units the compile database knows; a changed
        # header is analyzed when a TU that includes it is tidied
        # (HeaderFilterRegex in .clang-tidy surfaces its findings).
        candidates = [
            f
            for f in files
            if f.suffix in TIDY_SUFFIXES
            and str(REPO_DIR / f) in database
            and f.name != "BoundaryProbe.cpp"
        ]
        skipped = [
            f
            for f in files
            if f.suffix in TIDY_SUFFIXES and str(REPO_DIR / f) not in database
        ]
        for name in skipped:
            print(f"not in compile database (skipped): {name}")
    if not candidates:
        print("no translation units in scope")
        return True
    sdk = subprocess.run(
        ["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False
    ).stdout.strip()
    command = [tidy, "-p", BUILD_DIR, "--quiet"]
    if sdk:
        command.append(f"--extra-arg=-isysroot{sdk}")
        # Restored after the SDK's own directories, never before them:
        # a hand-installed SDK under /usr/local must resolve without
        # shadowing a system header of the same name.
        command.append("--extra-arg=-idirafter/usr/local/include")
    print(f"{len(candidates)} translation units")
    # One clang-tidy process per translation unit: a single process fed
    # many TUs reuses parse state across them and can report phantom
    # errors in headers the current TU never misparsed.
    workers = min(len(candidates), max(1, (os.cpu_count() or 4) - 2))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        results = list(pool.map(lambda tu: run([*command, tu]), candidates))
    # clang-tidy exits zero when it only emitted warnings (this repo's
    # WarningsAsErrors is deliberately empty), so the exit code cannot
    # be the verdict: parse the findings out of the diagnostics instead.
    # The verdict counts only findings in this repository's own sources:
    # HeaderFilterRegex scopes the checks, but a hard parse error in an
    # external header still prints, and external toolchains are not this
    # gate's to police.
    diagnostic = re.compile(r"^(?P<path>[^:\n]+):\d+:\d+: (?:warning|error): ")
    in_scope: list[str] = []
    external = 0
    for tu, result in zip(candidates, results):
        findings_here = False
        for line in result.stdout.splitlines():
            match = diagnostic.match(line)
            if not match:
                continue
            findings_here = True
            path = Path(match.group("path"))
            if not path.is_absolute():
                path = REPO_DIR / path
            try:
                inside = path.resolve().is_relative_to(REPO_DIR)
            except OSError:
                inside = False
            if inside and not any(
                fragment in str(path) for fragment in EXCLUDED_FRAGMENTS
            ):
                in_scope.append(line)
            else:
                external += 1
        if result.returncode != 0 and not findings_here:
            fail(f"clang-tidy failed on {tu}:\n{result.stderr[-2000:]}")
    for line in in_scope:
        print(line)
    if external:
        print(f"{external} findings outside the repository's sources (ignored)")
    if in_scope:
        print(f"{len(in_scope)} clang-tidy findings")
        return False
    print("clang-tidy clean")
    return True


def main() -> int:
    argument_parser = argparse.ArgumentParser(
        description=(
            "Check-only format and lint gate: clang-format, ruff, "
            "qmllint, and clang-tidy over the changed files"
        )
    )
    argument_parser.add_argument(
        "--all",
        action="store_true",
        help=(
            "check every tracked file instead of just the changed ones "
            "(clang-tidy keeps the changed-file scope; add --tidy-all "
            "for the full database)"
        ),
    )
    argument_parser.add_argument(
        "--fix",
        action="store_true",
        help=(
            "apply clang-format and ruff fixes to the scoped files "
            "(qmllint and clang-tidy remain report-only)"
        ),
    )
    argument_parser.add_argument(
        "--tidy-all",
        action="store_true",
        help=(
            "run clang-tidy over every translation unit in the compile "
            "database (a full semantic analysis per TU — expect a long "
            "run)"
        ),
    )
    tidy_scoping = argument_parser.add_mutually_exclusive_group()
    tidy_scoping.add_argument(
        "--skip-tidy",
        action="store_true",
        help=(
            "run only the fast checks; clang-tidy costs a semantic "
            "analysis per translation unit and can take its own pass"
        ),
    )
    tidy_scoping.add_argument(
        "--tidy-only",
        action="store_true",
        help=(
            "run only clang-tidy — the fast checks are the other half of "
            "--skip-tidy, so the two together cover this command once "
            "each and can run side by side"
        ),
    )
    argument_parser.add_argument(
        "files",
        nargs="*",
        help="check exactly these files instead of the changed set",
    )
    arguments = argument_parser.parse_args()

    if arguments.files:
        scope = [Path(name).resolve().relative_to(REPO_DIR) for name in arguments.files]
        label = "explicit files"
    elif arguments.all:
        scope, label = all_files(), "all tracked files"
    else:
        scope, label = changed_files(), "changed files"
    print(f"scope: {label} ({len(scope)} candidates)")

    # clang-tidy stays on the changed set even under --all: analyzing
    # every translation unit is a deliberate, separate decision
    # (--tidy-all), not a side effect of widening the fast checks.
    tidy_scope = scope if arguments.files else changed_files()

    results = {}
    if not arguments.tidy_only:
        results["clang-format"] = check_clang_format(
            with_suffixes(scope, CXX_SUFFIXES), arguments.fix
        )
        results["ruff"] = check_ruff(
            with_suffixes(scope, {".py"}), arguments.fix, arguments.all
        )
        results["qmllint"] = check_qmllint(with_suffixes(scope, {".qml"}))
    if arguments.skip_tidy:
        section("clang-tidy (C++ static analysis)")
        print("skipped (--skip-tidy)")
    else:
        results["clang-tidy"] = check_clang_tidy(
            with_suffixes(tidy_scope, CXX_SUFFIXES), arguments.tidy_all
        )

    section("summary")
    for tool, passed in results.items():
        print(f"  {tool:14} {'ok' if passed else 'FINDINGS'}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
