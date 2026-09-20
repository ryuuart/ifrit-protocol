"""Verb: check — format and lint, as one command: clang-format, ruff, qmllint.

    sigil.py check              # the branch's work, all three tools
    sigil.py check --all        # whole tree
    sigil.py check --fix        # apply the format fixes, then report
    sigil.py check FILE...      # exactly these files
    sigil.py check --docs       # …and the documentation tier as well

THE DEFAULT SCOPE IS THE BRANCH'S WORK: everything this branch changed
since it left main, committed or not, plus untracked files that are not
ignored. Work is committed freely and verified once, so a scope that saw
only uncommitted changes would miss most of what a branch is by the time
anyone runs this.

Exit status is non-zero when any tool reports a finding. The configs live
at the repository root (.clang-format with .clang-format-ignore,
ruff.toml); this verb only selects files and runs the tools against those
configs. Every tool is required: a missing one fails the run rather than
letting it pass on partial coverage — except the documentation tier,
which is opt-in: it needs Doxygen and a configured tree, and what it
finds is a page that reads wrong rather than code that is wrong.
"""

import argparse
import re
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
    "apps/python/sigil/stubs/",
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
    # Explicit file arguments must respect the same generated-source and
    # formatting exclusions as a directory scan.
    targets = ["--force-exclude", *(["."] if whole_tree else [str(f) for f in files])]
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
    # (Ifrit.Qt, SpellCircle.*); without it every project import is
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


# Doxygen states an undocumented entity two ways: a whole compound, or
# one member of one. The first is a finding; the second is the house
# convention working as intended.
UNDOCUMENTED_COMPOUND = re.compile(r"warning: Compound (\S+) is not documented")
UNDOCUMENTED_MEMBER = re.compile(
    r"warning: Member .* \((\w+)\) of .* is not documented"
)

# Printed above the audit report, so its counts are read as the
# convention holding rather than as work nobody has done.
CONVENTION = """A type carries the prose, and its self-evident fields and one-line
accessors do not repeat it, so members are counted rather than listed and
nothing here fails the run. A compound with no comment is the finding: it
has no page at all."""


def docs_scope(files: list):
    """The manifest, and the libraries any of those files documents.

    None when there is no configured tree: the manifest is the only
    thing this tier needs from one, and configuring a tree is a
    heavier thing than a check should decide to do.
    """
    manifest = tree.build_dir() / "docs-manifest.txt"
    if not manifest.exists():
        return None, []
    from sigil import docs

    return manifest, docs.libraries_for(manifest, [tree.REPO_DIR / f for f in files])


def check_docs(files: list) -> bool:
    """Every warning Doxygen has about the comments themselves.

    A comment can be perfectly written and still never reach a page: an
    @file that ate its first word as a filename, an @ingroup naming a
    group that belongs to another library, a stray token that closes the
    block early. None of that is visible in the source and all of it is
    visible here.
    """
    section("doxygen (documentation reaches the page)")
    if not files:
        print("no files in scope")
        return True
    manifest, names = docs_scope(files)
    if manifest is None:
        print("SKIPPED: no build tree — run sigil.py setup to write the manifest")
        return True
    if not names:
        print("no documented library in scope")
        return True
    from sigil import docs

    findings = docs.warnings_sweep(manifest, names)
    total = 0
    offending = 0
    for name in sorted(findings):
        lines = findings[name]
        if not lines:
            continue
        offending += 1
        total += sum(1 for line in lines if "warning:" in line)
        print(f"\n{name}:")
        for line in lines:
            print(f"  {line}")
    if not total:
        print(f"{len(names)} libraries clean")
        return True
    print(f"\n{total} findings in {offending} of {len(names)} libraries swept")
    return False


def report_undocumented(files: list) -> None:
    """The audit tier: what carries no comment at all.

    Never a gate. A type carries the prose and its self-evident fields
    do not repeat it, so a count over members measures the convention
    rather than the documentation. What is worth reading is the list of
    whole compounds, which is a work queue. A namespace with no comment
    of its own is the same kind of finding and cannot appear here:
    Doxygen writes no page for one and warns about none either, so the
    inventory the docs build writes is what sees those.
    """
    section("doxygen (what carries no comment)")
    manifest, names = docs_scope(files)
    if manifest is None or not names:
        print("SKIPPED: no build tree, or no documented library in scope")
        return
    from sigil import docs

    print(CONVENTION)
    findings = docs.warnings_sweep(manifest, names, undocumented=True)
    for name in sorted(findings):
        compounds = []
        members: dict = {}
        for line in findings[name]:
            compound = UNDOCUMENTED_COMPOUND.search(line)
            member = UNDOCUMENTED_MEMBER.search(line)
            if compound:
                compounds.append(compound.group(1))
            elif member:
                members[member.group(1)] = members.get(member.group(1), 0) + 1
        counted = ", ".join(
            f"{count} {kind}" for kind, count in sorted(members.items())
        )
        print(f"\n{name}: {len(compounds)} compounds with no comment")
        for compound in sorted(compounds):
            print(f"  {compound}")
        if counted:
            print(f"  members with none: {counted}")


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
        "--docs",
        action="store_true",
        help="also run Doxygen over the scoped libraries and report every "
        "comment that does not reach a page (needs a configured tree)",
    )
    parser.add_argument(
        "--docs-undocumented",
        action="store_true",
        help="with --docs, also list what carries no comment at all; that "
        "report never fails the run",
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
        "ruff": check_ruff(
            with_suffixes(scope, {".py", ".pyi"}), arguments.fix, arguments.all
        ),
        "qmllint": check_qmllint(with_suffixes(scope, {".qml"})),
    }
    if arguments.docs or arguments.docs_undocumented:
        documents = with_suffixes(scope, {".h", ".hpp", ".md"})
        results["doxygen"] = check_docs(documents)
        if arguments.docs_undocumented:
            report_undocumented(documents)

    section("summary")
    for tool, passed in results.items():
        print(f"  {tool:14} {'ok' if passed else 'FINDINGS'}")
    return 0 if all(results.values()) else 1
