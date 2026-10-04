"""Verb: flags — the sketch compile line, lifted into a response file.

    sigil.py flags --compdb build/compile_commands.json \\
        --anchor ../grimoire/sketches/Anchor.cpp --config Release \\
        --out build/bin/Release/sketch_flags.rsp [--extra <link input>]

The step itself is src/sketch/cmake/SketchFlags.py, which a host's build
runs through the framework's `sigil_sketch_flags` with its own anchor
unit — the response file is how the live host compiles a sketch, so it
belongs to SigilSketch rather than to the tree's administration. This is
the front door for running it by hand.
"""

import runpy
import sys

from sigil import tree

STEP = tree.PROJECT_DIR / "src" / "sketch" / "cmake" / "SketchFlags.py"


def main(argv: list) -> int:
    if not STEP.exists():
        tree.fail(f"no sketch flags step at {STEP}")
    argv0 = sys.argv
    sys.argv = [str(STEP), *argv]
    try:
        runpy.run_path(str(STEP), run_name="__main__")
    finally:
        sys.argv = argv0
    return 0
