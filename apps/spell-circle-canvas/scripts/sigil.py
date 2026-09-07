#!/usr/bin/env python3
"""The build's administration, as one command: sigil.py <verb>.

    python3 scripts/sigil.py <verb> [flags]
    python3 scripts/sigil.py <verb> --help

Nine verbs over one package. `scripts/README.md` is the canon for what
each does and what it refuses; `--help` on a verb is the canon for its
flags. Every mise task is one of these under a one-word name.
"""

import importlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

VERBS = {
    "setup": "discover Qt and vcpkg, write the presets, configure and build",
    "check": "clang-format, ruff and qmllint over the branch's work",
    "plates": "plate sweep over the sketch registry: cpu, device, promotion",
    "bench": "timing sweep: the benchmark binaries, or the window lane",
    "sanitize": "an instrumented tree: address, thread or coverage",
    "docs": "the per-library Doxygen sites, opened or served",
    "assets": "fetch the demo assets, or stage an SDK archive",
    "flags": "lift the sketch compile line out of the compilation database",
    "flatbuffers": "regenerate the committed Python schema modules",
}


def usage() -> int:
    print(__doc__.strip())
    print("\nVerbs:")
    for verb, description in VERBS.items():
        print(f"  {verb:<12} {description}")
    return 0


def main(argv: list) -> int:
    if not argv or argv[0] in ("-h", "--help", "help"):
        return usage()
    verb, rest = argv[0], argv[1:]
    if verb not in VERBS:
        print(f"no such verb: {verb}\n", file=sys.stderr)
        usage()
        return 2
    # Imported on demand: a verb pays for its own imports and nothing for
    # the other eight.
    return importlib.import_module(f"sigil.{verb}").main(rest)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
