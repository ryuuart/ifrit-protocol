#!/usr/bin/env python3
"""Compile one C++ source for errors only, without touching the build tree.

    syntaxcheck.py <source.cpp> [--like <sibling.cpp>] [--config Release]

The flags come from the recorded compile command of the source itself, or of
--like when the source is new and the build has never seen it. Nothing is
written: the command runs with -fsyntax-only and no output file, so any
number of these may run at once and beside a real build.
"""

import argparse
import json
import shlex
import subprocess
import sys
from pathlib import Path

APPLICATION = Path(__file__).resolve().parents[1]  # apps/spell-circle-canvas, whichever checkout holds this file
DATABASE = APPLICATION / "build" / "compile_commands.json"


def recorded_command(source: Path, configuration: str) -> list[str] | None:
    entries = json.loads(DATABASE.read_text())
    chosen = None
    for entry in entries:
        if Path(entry["file"]).resolve() != source:
            continue
        arguments = entry.get("arguments") or shlex.split(entry["command"])
        output = arguments[arguments.index("-o") + 1] if "-o" in arguments else ""
        if f"/{configuration}/" in output:
            return arguments
        chosen = chosen or arguments
    return chosen


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source")
    parser.add_argument("--like", help="an existing source in the same target")
    parser.add_argument("--config", default="Release")
    options = parser.parse_args()

    source = Path(options.source).resolve()
    if not source.exists():
        print(f"no such source: {source}", file=sys.stderr)
        return 2
    model = Path(options.like).resolve() if options.like else source
    arguments = recorded_command(model, options.config)
    if arguments is None:
        print(
            f"{model} has no recorded compile command; pass --like <a source "
            "already built in the same target>",
            file=sys.stderr,
        )
        return 2

    command = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
            continue
        if argument == "-o":
            skip = True
            continue
        if argument == "-c":
            continue
        if Path(argument).resolve() == model and argument.endswith((".cpp", ".mm", ".cc")):
            continue
        command.append(argument)
    command += ["-fsyntax-only", "-ferror-limit=40", str(source)]
    result = subprocess.run(command, cwd=APPLICATION / "build")
    print("syntax check passed" if result.returncode == 0 else "syntax check FAILED")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
