#!/usr/bin/env python3
"""The app frame-rate ledger, as ONE command: every sketch presented in
the real window, compared against a committed baseline.

Drives `Sketchbook --window-bench`, which opens the window at a stated
size and device pixel ratio, presents each registry sketch for a stretch
after a warm-up, and prints one WINDOW line per sketch. This reads those
lines and judges the PRESENTED frame rate against the baseline stored for
the build configuration, printing IDENTICAL / FASTER / SLOWER rows and
one verdict. The plate ledger does this for bytes and the bench ledger
for isolated timings; this does it for what the window actually put on
screen.

Usage (from apps/spell-circle-canvas):
  scripts/app_fps_ledger.py --rebase          # bake the baseline
  scripts/app_fps_ledger.py                   # sweep + compare + verdict
  scripts/app_fps_ledger.py --sketch first_light
  scripts/app_fps_ledger.py --kind set --seconds 4

WHAT THIS MEASURES THAT `--bench` CANNOT. The frame-time gate renders
onto a raster surface at the sketch's declared size and presents nothing
— it is the sketch's own cost isolated, which is what makes it a gate.
Here the frame is drawn through the surface the window presents, at the
window's pixels and its device pixel ratio, and the numbers carry the
host's own overhead: the submit or texture upload that puts the frame on
screen, and for a set the device readback and blit its paint phase
performs. So the two answer different questions and neither replaces the
other.

WHAT A PRESENTED RATE IS BOUNDED BY. The compositor, which means the
display. A sketch comfortably inside its budget reads at the refresh
rate and says nothing more; the interesting rows are the ones BELOW it,
and `work` beside them says how much of the frame was the sketch. A
baseline is therefore per machine AND per display mode, and the file
records the window size and scale it was taken at so a mismatch is
visible rather than silently compared.

THE MACHINE MUST BE QUIET, and the window must be able to present: this
lane opens a real window and a compositor that is busy compositing
something else is measured along with the sketch.

THE BASELINE IS COMMITTED, one file per build configuration, under
bench/app_fps_<config>.json. --rebase writes it from this sweep, and A
NARROWED SWEEP MERGES: with --sketch or --kind it keeps the sketches it
did not present, so adopting one deliberately changed number never
discards a number this run did not take. Only an unnarrowed sweep writes
the file wholesale, which is what drops a sketch that no longer exists.
"""

import argparse
import datetime
import json
import os
import platform
import re
import subprocess
import sys

# Per-sketch tolerance bands on the presented rate, keyed by a regular
# expression matched against the sketch name. Only for sketches whose
# run-to-run spread on a quiet machine honestly exceeds the default; a
# widened band is a statement about the sketch, not a way past a finding.
DEFAULT_TOLERANCE = 0.10
TOLERANCES = {}

# One WINDOW line, and the shape the lane prints it in.
LINE = re.compile(r"^WINDOW (\S+) (.*)$")


def tolerance_for(name):
    for pattern, band in TOLERANCES.items():
        if re.search(pattern, name):
            return band
    return DEFAULT_TOLERANCE


def parse_lines(text):
    """The WINDOW lines, as {name: row} plus the names stood down.

    A row is the key=value pairs after the name, numbers where they read
    as numbers. A line saying SKIPPED is a sketch this machine cannot
    run: not a measurement and not a failure."""
    rows, skipped = {}, {}
    for line in text.splitlines():
        found = LINE.match(line.strip())
        if not found:
            continue
        name, rest = found.group(1), found.group(2)
        if rest.startswith("SKIPPED"):
            skipped[name] = rest[len("SKIPPED") :].strip()
            continue
        row = {}
        for pair in rest.split():
            if "=" not in pair:
                continue
            key, value = pair.split("=", 1)
            row[key] = value
        for key in ("fps", "work", "p99", "draw", "submit", "headroom"):
            if key in row:
                row[key] = float(row[key].removesuffix("ms"))
        rows[name] = row
    return rows, skipped


def run_lane(binary, args):
    cmd = [
        binary,
        "--window-bench",
        f"{args.seconds:g}",
        "--window-size",
        args.size,
        "--window-scale",
        f"{args.scale:g}",
    ]
    if args.sketch:
        cmd += ["--sketch", args.sketch]
    if args.kind:
        cmd += ["--kind", args.kind]
    try:
        r = subprocess.run(
            cmd, capture_output=True, text=True, timeout=args.timeout_seconds
        )
    except subprocess.TimeoutExpired:
        return "", f"still running after {args.timeout_seconds:g}s (killed)"
    if r.returncode != 0:
        return r.stdout, (r.stderr or r.stdout).strip()[-400:]
    return r.stdout, None


def load_baseline(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def write_baseline(path, config, window, rows, merge_into):
    """The baseline, from this sweep and — when it was a subset — what
    the baseline already held. Pass merge_into=None for a whole sweep,
    which is the one run entitled to write the file wholesale and thereby
    to drop what no longer exists."""
    sketches = dict((merge_into or {}).get("sketches", {}))
    sketches.update(rows)
    doc = {
        "config": config,
        "host": platform.node(),
        "machine": platform.machine(),
        "window": window,
        "taken": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="seconds"
        ),
        "sketches": {name: sketches[name] for name in sorted(sketches)},
    }
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(doc, f, indent=1, sort_keys=False)
        f.write("\n")
    return doc


def main():
    ap = argparse.ArgumentParser(
        description="presented-frame-rate sweep over the sketch registry in "
        "the real window, judged against a committed baseline"
    )
    ap.add_argument("--config", default="Release")
    ap.add_argument(
        "--seconds",
        type=float,
        default=2.5,
        metavar="S",
        help="measured stretch per sketch, after the lane's own warm-up (default 2.5)",
    )
    ap.add_argument(
        "--size",
        default="1440x900",
        metavar="WxH",
        help="the window's logical size (default 1440x900)",
    )
    ap.add_argument(
        "--scale",
        type=float,
        default=2.0,
        metavar="N",
        help="Qt's scale factor for the window (default 2)",
    )
    ap.add_argument("--sketch", metavar="NAME", help="present only this one")
    ap.add_argument("--kind", choices=["canvas", "set"], help="present only these")
    ap.add_argument(
        "--timeout-seconds",
        type=float,
        default=3600,
        metavar="S",
        help="ceiling on the whole sweep (default 3600)",
    )
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="write the baseline from this sweep. A sweep narrowed by "
        "--sketch or --kind merges, so only an unnarrowed rebase rewrites "
        "the file wholesale",
    )
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    binary = os.path.join(
        root,
        "build",
        "bin",
        args.config,
        "Sketchbook.app",
        "Contents",
        "MacOS",
        "Sketchbook",
    )
    baseline_path = os.path.join(root, "bench", f"app_fps_{args.config}.json")
    if not os.path.exists(binary):
        sys.exit(f"no Sketchbook at {binary} — build the Sketchbook target first")

    print(
        f"presenting in a {args.size} window at scale {args.scale:g}, "
        f"{args.seconds:g}s each, config {args.config}\n"
    )
    output, error = run_lane(binary, args)
    rows, skipped = parse_lines(output)
    if error:
        print(f"  FAILED  {error}")
    for name in sorted(skipped):
        print(f"  SKIPPED    {name} ({skipped[name]})")
    if not rows:
        print("\nnothing presented; the baseline is untouched")
        return 1
    print(f"  presented  {len(rows)} sketches\n")

    # What the window actually was, read off the lines rather than off the
    # request: a display that will not give the size asked for gives
    # another, and the baseline has to record the one that was measured.
    window = rows[next(iter(rows))].get("window", args.size)

    baseline = load_baseline(baseline_path)
    if args.rebase or baseline is None:
        if not args.rebase:
            print(
                f"no baseline at {baseline_path} — writing one (this sweep "
                f"becomes the baseline)"
            )
        subset = bool(args.sketch or args.kind)
        doc = write_baseline(
            baseline_path,
            args.config,
            window,
            rows,
            baseline if subset else None,
        )
        print(
            f"baseline written: {baseline_path} "
            f"({len(doc['sketches'])} sketches, {len(rows)} from this sweep)"
        )
        for name in sorted(rows):
            print(f"  {name:<28} {rows[name]['fps']:6.1f} fps")
        if subset:
            kept = len(doc["sketches"]) - len(rows)
            print(
                f"  merged {len(rows)} of {len(doc['sketches'])}; {kept} kept "
                f"at the value the baseline already held"
            )
        return 1 if error else 0

    if baseline.get("host") and baseline["host"] != platform.node():
        print(
            f"WARNING: baseline was taken on {baseline['host']}, this is "
            f"{platform.node()} — numbers are per machine\n"
        )
    if baseline.get("window") and baseline["window"] != window:
        print(
            f"WARNING: baseline was taken at {baseline['window']}, this run "
            f"is {window} — a presented rate is not comparable across them\n"
        )

    identical, faster, slower, new, missing = [], [], [], [], []
    for name in sorted(rows):
        base = baseline.get("sketches", {}).get(name)
        row = rows[name]
        if base is None:
            new.append(name)
            print(f"  NEW        {name:<28} {row['fps']:6.1f} fps")
            continue
        ratio = row["fps"] / base["fps"] if base.get("fps") else 1.0
        band = tolerance_for(name)
        delta = ratio - 1.0
        if delta < -band:
            status, bucket = "SLOWER", slower
        elif delta > band:
            status, bucket = "FASTER", faster
        else:
            status, bucket = "IDENTICAL", identical
        bucket.append(name)
        print(
            f"  {status:<10} {name:<28} {base['fps']:6.1f} -> {row['fps']:6.1f} fps"
            f"  {delta:+6.1%} (band ±{band:.0%})   work "
            f"{base.get('work', 0.0):5.2f} -> {row['work']:5.2f} ms"
        )
    for name in baseline.get("sketches", {}):
        if name not in rows and name not in skipped and not (args.sketch or args.kind):
            missing.append(name)
    for name in missing:
        print(f"  MISSING    {name} (in baseline, not presented — rebase to drop)")

    print(
        f"\n{len(identical)} identical, {len(faster)} faster, {len(slower)} slower, "
        f"{len(new)} new, {len(missing)} missing, {len(skipped)} skipped"
    )
    if slower:
        print("SLOWER beyond band:")
        for name in slower:
            print(f"  {name}   <-- FINDING")
    if not slower and not error:
        print("VERDICT: within band")
    return 1 if slower or error else 0


if __name__ == "__main__":
    sys.exit(main())
