"""Verb: flatbuffers — regenerate the committed Python schema modules.

    sigil.py flatbuffers

Only the Python side. The C++ header is generated into the build tree by
the SpellCircleSchema target and is not committed, while apps/python is
installed and imported — by TouchDesigner among others — with no CMake
build in reach, which is why its modules stay committed and regenerating
them is a step to run after every schema edit.

The generator writes a package, not just modules: alongside the per-table
modules it emits an empty SpellCircle/__init__.py, and that name is
already taken by the hand-written public API. So generation goes to a
staging directory and only the schema modules are copied over.
"""

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

from sigil import tree

SCHEMA = (
    tree.PROJECT_DIR / "src" / "spellcircle" / "shared" / "schema" / "SpellCircle.fbs"
)
PACKAGE = tree.REPO_DIR / "apps" / "python" / "SpellCircle"


def main(argv: list) -> int:
    argparse.ArgumentParser(
        prog="sigil.py flatbuffers",
        description="regenerate apps/python/SpellCircle's schema modules from "
        "SpellCircle.fbs",
    ).parse_args(argv)

    if not SCHEMA.exists():
        tree.fail(f"no schema at {SCHEMA}")
    if shutil.which("flatc") is None:
        tree.fail("flatc not found — brew install flatbuffers")

    with tempfile.TemporaryDirectory(prefix="sigil_flatbuffers_") as staging:
        subprocess.run(["flatc", "--python", "-o", staging, str(SCHEMA)], check=True)
        written = []
        for module in sorted(Path(staging).glob("SpellCircle/*.py")):
            if module.name == "__init__.py":
                continue
            shutil.copyfile(module, PACKAGE / module.name)
            written.append(module.name)

    print(f"Regenerated from {SCHEMA}:")
    for name in written:
        print(f"  {PACKAGE / name}")
    return 0
