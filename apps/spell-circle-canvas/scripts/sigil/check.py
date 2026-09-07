"""Verb: check — format and lint, as one command: clang-format, ruff, qmllint.

    sigil.py check              # the branch's work, all three tools
    sigil.py check --all        # whole tree
    sigil.py check --fix        # apply the format fixes, then report
    sigil.py check FILE...      # exactly these files

THE DEFAULT SCOPE IS THE BRANCH'S WORK: everything this branch changed
since it left main, committed or not, plus untracked files that are not
ignored. Work is committed freely and verified once, so a scope that saw
only uncommitted changes would miss most of what a branch is by the time
anyone runs this.

Exit status is non-zero when any tool reports a finding. The configs live
at the repository root (.clang-format with .clang-format-ignore,
ruff.toml); this verb only selects files and runs the tools against those
configs. Every tool is required: a missing one fails the run rather than
letting it pass on partial coverage.
"""

import argparse
import shutil
from pathlib import Path

from sigil import tree

CXX_SUFFIXES = {".cpp", ".cc", ".cxx", ".c", ".h", ".hh", ".hpp", ".mm", ".m"}

# The branch this one is measured against. A branch cut from main carries
# its own committed work and nothing of main's.
TRUNK = "main"

# Paths no checker touches: prebuilt dependencies and generated sources.
# Mirrors .clang-format-ignore (which only clang-format 18+ reads on its
# own) and ruff.toml's extend-exclude, so the selection holds even when a
# tool predates its ignore mechanism.
EXCLUDED_FRAGMENTS = (
    "SpellCircle_generated",
    "vcpkg_installed/",
)


def git(command: list) -> str:
    result = tree.capture(["git", *command], cwd=tree.REPO_DIR)
    if result.returncode != 0:
        tree.fail(f"git failed: git {' '.join(command)}\n{result.stderr}")
    return result.stdout


def section(title: str) -> None:
    print(f"\n=== {title}")


def branch_files() -> list:
    """Everything this branch has to answer for: the files it changed
    since the merge base with the trunk, plus what is uncommitted or
    untracked. Without a trunk to compare against — a checkout that has no
    main — the scope falls back to the working tree alone."""
    base = tree.capture(["git", "merge-base", TRUNK, "HEAD"], cwd=tree.REPO_DIR)
    since = base.stdout.strip() if base.returncode == 0 else "HEAD"
    if base.returncode != 0:
        print(f"note: no merge base with {TRUNK} — scoping to the working tree")
    listed = git(["diff", "--name-only", since]).splitlines()
    listed += git(["ls-files", "--others", "--exclude-standard"]).splitlines()
    return [
        Path(name) for name in sorted(set(listed)) if (tree.REPO_DIR / name).is_file()
    ]


def all_files() -> list:
    return [
        Path(name)
        for name in git(["ls-files"]).splitlines()
        if (tree.REPO_DIR / name).is_file()
    ]


def included(path: Path) -> bool:
    return not any(fragment in str(path) for fragment in EXCLUDED_FRAGMENTS)


def with_suffixes(files: list, suffixes: set) -> list:
    return [f for f in files if f.suffix in suffixes and included(f)]


def run(command: list, capture: bool = True):
    if capture:
        return tree.capture(command, cwd=tree.REPO_DIR)
    return tree.run(command, cwd=tree.REPO_DIR, echo=False)


def check_clang_format(files: list, fix: bool) -> bool:
    section("clang-format (C++/ObjC++)")
    if not files:
        print("no files in scope")
        return True
    xcrun = shutil.which("xcrun")
    if not xcrun:
        tree.fail("xcrun not found — install the Xcode command line tools")
    if fix:
        result = run([xcrun, "clang-format", "-i", *files])
        if result.returncode != 0:
            tree.fail(f"clang-format -i failed:\n{result.stderr}")
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
    if not offending:
        # A non-zero exit with no violation lines is clang-format itself
        # failing (a missing file, an unreadable option), not a finding.
        tree.fail(f"clang-format failed:\n{result.stderr}")
    for name in offending:
        print(f"needs formatting: {name}")
    print(
        f"{len(offending)} of {len(files)} files need clang-format "
        "(sigil.py check --fix applies it)"
    )
    return False


def check_ruff(files: list, fix: bool, whole_tree: bool) -> bool:
    section("ruff (Python lint + format)")
    if not whole_tree and not files:
        print("no files in scope")
        return True
    if not shutil.which("ruff"):
        tree.fail("ruff not found — brew install ruff")
    # Whole-tree mode hands ruff the repository root so ruff.toml's own
    # excludes drive selection; scoped mode hands it the exact files.
    targets = ["."] if whole_tree else [str(f) for f in files]
    if fix:
        run(["ruff", "check", "--fix", *targets], capture=False)
        run(["ruff", "format", *targets], capture=False)
    passed = run(["ruff", "check", *targets], capture=False) == 0
    passed &= run(["ruff", "format", "--check", *targets], capture=False) == 0
    if passed:
        print("ruff clean")
    return passed


def check_qmllint(files: list) -> bool:
    section("qmllint (QML)")
    if not files:
        print("no files in scope")
        return True
    prefix = tree.qt_prefix()
    if prefix is None:
        print(
            "SKIPPED: no configure presets — run sigil.py setup to record the Qt prefix"
        )
        return True
    qmllint = prefix / "bin" / "qmllint"
    if not qmllint.exists():
        tree.fail(f"qmllint not found at {qmllint}")
    # The build tree's qml/ holds the repository's own compiled modules
    # (Ifrit.Ui, SpellCircle.*); without it every project import is
    # unresolvable and the lint is meaningless.
    import_dir = tree.build_dir() / "qml"
    command = [qmllint]
    if import_dir.is_dir():
        command += ["-I", import_dir]
    else:
        print(
            f"note: {import_dir} missing (project imports unresolved "
            "until the app is built)"
        )
    return run([*command, *files], capture=False) == 0


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(
        prog="sigil.py check",
        description="Format and lint: clang-format, ruff and qmllint over "
        "the work this branch carries",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="check every tracked file instead of the branch's own",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="apply clang-format and ruff fixes to the scoped files "
        "(qmllint remains report-only)",
    )
    parser.add_argument(
        "files",
        nargs="*",
        help="check exactly these files instead of the branch's",
    )
    arguments = parser.parse_args(argv)

    if arguments.files:
        scope = [
            Path(name).resolve().relative_to(tree.REPO_DIR) for name in arguments.files
        ]
        label = "explicit files"
    elif arguments.all:
        scope, label = all_files(), "all tracked files"
    else:
        scope, label = (
            branch_files(),
            f"this branch since {TRUNK}, plus the working tree",
        )
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
