#!/usr/bin/env python3
"""The browser's rows say what they know and nothing more.

A row is what the browser shows about a sketch before one is opened. Two
of its fields are facts of the registry alone, and both are asserted
here without a window: a sketch compiled into the host names the runtime
it draws through, read off its kind; a file opened by path is compiled
when it is opened, so its row carries NO runtime until it has been built
— an empty kind, not a guess from the folder it stands in.

Usage (invoked by the build; the paths are all absolute):
  catalog_rows.py --sketchbook <Sketchbook binary> \\
      --canvas <stem of a canvas sketch in the registry> --work <scratch dir>
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sketchbook", type=Path, required=True)
    parser.add_argument("--canvas", required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()

    # A FILE OPENED BY PATH: a draft that has never been built, standing
    # in a directory of its own, as a sketch outside this checkout does.
    args.work.mkdir(parents=True, exist_ok=True)
    draft = args.work / "book_probe_draft.cpp"
    draft.write_text("// book_probe_draft.cpp — a draft nothing has built\n")

    command = [str(args.sketchbook), "--catalog", str(draft)]
    print("$ " + " ".join(command), flush=True)
    result = subprocess.run(command, capture_output=True, text=True)
    sys.stderr.write(result.stderr)
    if result.returncode != 0:
        sys.exit(f"exited {result.returncode}")
    rows = [json.loads(line) for line in result.stdout.splitlines() if line]
    if not rows:
        sys.exit("--catalog printed no rows")

    by_key = {row["key"]: row for row in rows}
    compiled_in = by_key.get(args.canvas)
    if compiled_in is None:
        sys.exit(f"no row for the registry sketch {args.canvas}")
    if compiled_in["kind"] != "canvas":
        sys.exit(
            f"{args.canvas} is a canvas sketch and its row says "
            f"{compiled_in['kind']!r} — the row must name the runtime read "
            "off the kind"
        )

    # The externals are appended after every compiled-in entry, so the
    # draft is the last row.
    last = rows[-1]
    if last["key"] != draft.stem:
        sys.exit(f"the last row is {last['key']!r}, not the draft {draft.stem!r}")
    if last["kind"] != "":
        sys.exit(
            f"the unbuilt draft's row says {last['kind']!r} — a file opened by "
            "path has no runtime until it has been built"
        )
    print(
        f"{len(rows)} rows: {args.canvas} draws through {compiled_in['kind']}, "
        f"{draft.stem} has no runtime yet",
        flush=True,
    )


if __name__ == "__main__":
    main()
