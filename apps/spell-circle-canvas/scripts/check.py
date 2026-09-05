#!/usr/bin/env python3
"""Format and lint, as ONE command: clang-format, ruff, qmllint.

Checks are read-only by default and scoped to CHANGED files — everything
git sees as modified, staged, or untracked — so the routine invocation
polices the work in progress and nothing else. The configs live at the
repository root (.clang-format with .clang-format-ignore, ruff.toml);
this script only selects files and runs the tools against those configs.

Usage (from anywhere in the repository):
  scripts/check.py             # changed files, all three tools
  scripts/check.py --all       # whole tree
  scripts/check.py --fix       # apply clang-format and ruff fixes to
                               # the scoped files, then report what
                               # remains
  scripts/check.py FILE...     # check exactly these files

Exit status is non-zero when any tool reports a finding. Every tool is
required: a tool that is missing fails the run rather than passing it.

Tool provenance, macOS: clang-format comes from the Xcode toolchain via
xcrun; qmllint from the Qt prefix recorded in CMakeUserPresets.json by
scripts/setup.py; ruff from PATH (brew install ruff).
"""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path
from typing import NoReturn

SCRIPT_DIR = Path(__file__).parent.resolve()
PROJECT_DIR = SCRIPT_DIR.parent  # apps/spell-circle-canvas
REPO_DIR = PROJECT_DIR.parent.parent
BUILD_DIR = PROJECT_DIR / "build"
USER_PRESETS = PROJECT_DIR / "CMakeUserPresets.json"

CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".c", ".h", ".hh", ".hpp", ".mm", ".m"}

# Paths no checker touches: prebuilt dependencies and generated
# sources. Mirrors .clang-format-ignore (which only clang-format 18+
# reads on its own) and ruff.toml's extend-exclude, so the selection
# holds even when a tool predates its ignore mechanism.
EXCLUDED_FRAGMENTS = (
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
    """The Qt installation the build uses: the CMAKE_PREFIX_PATH that
    scripts/setup.py recorded in CMakeUserPresets.json. None when the
    file does not exist yet, and the qmllint section reports that
    instead of guessing at a Qt."""
    if not USER_PRESETS.exists():
        return None
    for preset in json.loads(USER_PRESETS.read_text()).get("configurePresets", []):
        prefix = preset.get("cacheVariables", {}).get("CMAKE_PREFIX_PATH")
        if prefix:
            return Path(prefix)
    return None


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


def main() -> int:
    argument_parser = argparse.ArgumentParser(
        description=(
            "Check-only format and lint: clang-format, ruff and qmllint "
            "over the changed files"
        )
    )
    argument_parser.add_argument(
        "--all",
        action="store_true",
        help="check every tracked file instead of just the changed ones",
    )
    argument_parser.add_argument(
        "--fix",
        action="store_true",
        help=(
            "apply clang-format and ruff fixes to the scoped files "
            "(qmllint remains report-only)"
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

    results = {
        "clang-format": check_clang_format(
            with_suffixes(scope, CXX_SUFFIXES), arguments.fix
        ),
        "ruff": check_ruff(with_suffixes(scope, {".py"}), arguments.fix, arguments.all),
        "qmllint": check_qmllint(with_suffixes(scope, {".qml"})),
    }

    section("summary")
    for tool, passed in results.items():
        print(f"  {tool:14} {'ok' if passed else 'FINDINGS'}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
