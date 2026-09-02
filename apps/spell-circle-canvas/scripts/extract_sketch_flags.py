#!/usr/bin/env python3
"""Lifts the compiler flags for the sketch anchor TU into a response file.

The single source of truth for how a sketch builds is the target graph
itself, not a hand-maintained flag list: this reads the compilation
database CMake writes, finds the entry for the anchor translation unit,
strips the per-object bookkeeping, and writes what is left as a clang
response file — one argument per line.

Usage (invoked by the build; the paths are all absolute):
  scripts/extract_sketch_flags.py --compdb build/compile_commands.json \\
      --anchor src/sketch/sketches/Anchor.cpp --config Release \\
      --out build/bin/Release/sketch_flags.rsp [--extra <link input>]
"""

import argparse
import json
import shlex
import sys
from pathlib import Path

# Per-object bookkeeping the compile line carries and a sketch must not:
# the two that name this object, and the dependency-file flags the
# generator appends. Everything else — includes, defines, std, arch,
# sysroot, warnings, optimization — is how the target builds and passes
# through.
FLAGS_WITH_VALUE = {"-c", "-o", "-MF", "-MT", "-MQ"}
FLAGS_ALONE = {"-MD", "-MMD"}


def find_command(compdb: Path, anchor: str, config: str) -> str:
    """The compile command for the anchor, as one shell-quoted string."""
    entries = json.loads(compdb.read_text())
    candidates = [entry for entry in entries if entry.get("file") == anchor]
    for entry in candidates:
        # Multi-config generators list the anchor once per config; match
        # the object path ("<target>.dir/<config>/...") against ours.
        output = entry.get("output")
        if output is None or f"/{config}/" in output or len(entries) == 1:
            return entry["command"]
    sys.exit(f"no compile_commands.json entry for {anchor} (config {config})")


def response_lines(command: str, anchor: str, extra: list[str]) -> list[str]:
    args = shlex.split(command)
    args.pop(0)  # the compiler itself

    flags = []
    skip_next = False
    for arg in args:
        if skip_next:
            skip_next = False
            continue
        if arg in FLAGS_WITH_VALUE:
            skip_next = True
            continue
        if arg in FLAGS_ALONE or arg == anchor:
            continue
        flags.append(arg)
    flags.extend(item for item in extra if item)

    # A double quote is grouping to the parser that reads a response
    # file, so a define whose value is a string literal (-DNAME="text")
    # would arrive with its quotes eaten and expand to bare tokens
    # wherever a sketch used it. Reading the database unquoted them; this
    # puts them back, so the response file means what the compile line it
    # was lifted from meant.
    return [flag.replace('"', '\\"') for flag in flags]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compdb", type=Path, required=True)
    parser.add_argument("--anchor", required=True)
    parser.add_argument("--config", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument(
        "--extra",
        default="",
        help="extra link inputs, ;-separated, appended after the flags",
    )
    args = parser.parse_args()

    # The database is written at generate time by the Ninja and Makefiles
    # generators (it describes the build graph, so it exists before the
    # first compile) — a missing file means an unsupported generator, not
    # an incomplete build.
    if not args.compdb.exists():
        sys.exit(
            f"{args.compdb} not found — CMAKE_EXPORT_COMPILE_COMMANDS is only "
            "supported by the Ninja and Makefiles generators; configure with "
            "one of those to build the sketch host (see scripts/setup.py)"
        )

    command = find_command(args.compdb, args.anchor, args.config)
    lines = response_lines(command, args.anchor, args.extra.split(";"))
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("".join(f"{line}\n" for line in lines))


if __name__ == "__main__":
    main()
