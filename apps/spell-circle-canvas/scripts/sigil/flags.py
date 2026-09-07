"""Verb: flags — the sketch compile line, lifted into a response file.

    sigil.py flags --compdb build/compile_commands.json \\
        --anchor src/sketch/sketches/Anchor.cpp --config Release \\
        --out build/bin/Release/sketch_flags.rsp [--extra <link input>]

The step itself is src/sketch/cmake/SketchFlags.py, beside the sketch
host whose build runs it — the response file is how the live host
compiles a sketch, so it belongs to SigilSketch rather than to the app's
administration. This is the front door for running it by hand; the build
calls the file directly.
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
