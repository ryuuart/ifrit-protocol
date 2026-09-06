"""Verb: bench — timing sweeps judged against a committed baseline.

    sigil.py bench                          # every *_bench binary
    sigil.py bench --rebase
    sigil.py bench --benches weave_bench geometry_bench
    sigil.py bench --repetitions 7 --min-time 0.2
    sigil.py bench --lane fps               # the real window instead
    sigil.py bench --lane fps --sketch first_light
    sigil.py bench --lane fps --kind set --seconds 4

TWO LANES, ONE BODY. The `bench` lane (the default) runs every `*_bench`
binary the build produced under bin/<config>/benches with Google
Benchmark's JSON reporter and takes the median real time of each
benchmark. The `fps` lane presents each sketch in the real window through
Sketchbook's `--window-bench` and takes the presented frame rate. They
measure different things — the frame-time gate cannot see the host's own
overhead, and the window lane needs a display and exclusive use of the
machine — so they stay two lanes rather than one command; what they share
is the whole of what happens to a number after it is taken.

What each lane measures, how a number is taken and judged, and why each
widened band stands where it does is scripts/README.md. Use a Release
build for either, on a quiet machine.
"""

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile

from sigil import baseline, tree

DEFAULT_TOLERANCE = 0.10

# Per-benchmark bands, keyed by a regular expression matched against the
# full benchmark name ("binary:BM_Name/args"). Only for benchmarks whose
# run-to-run spread on a quiet machine honestly exceeds the default; what
# each one is answering is the table in scripts/README.md.
BENCH_TOLERANCES = {
    r":BM_Noise": 0.15,
    r"_Cold": 0.15,
    r"ReplaceWholeParagraph_Cold": 0.15,
    r"^weave_bench:BM_Draw": 0.15,
    r"^scry_bench:BM_Page_ChangeLatency": 0.75,
    r"^world_bench:BM_ChainOnDevice/20000/": 0.35,
}

TIME_UNITS = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}

# One WINDOW line, and the shape the fps lane prints it in.
WINDOW_LINE = re.compile(r"^WINDOW (\S+) (.*)$")


def format_time(nanoseconds):
    for unit, scale in (("s", 1e9), ("ms", 1e6), ("us", 1e3)):
        if nanoseconds >= scale:
            return f"{nanoseconds / scale:8.3f} {unit}"
    return f"{nanoseconds:8.1f} ns"


def compare(pairs, tolerances, higher_is_better, line):
    """Judges (name, measured, base) triples and prints one row each.

    Returns the five lists the verdict counts. @p line renders one row
    given (status, name, measured, base, delta, band); a base of None is
    a row the baseline has never seen."""
    identical, faster, slower, new = [], [], [], []
    for name, measured, base in pairs:
        if base is None:
            new.append(name)
            print(line("NEW", name, measured, None, None, None))
            continue
        band = baseline.band_for(name, tolerances, DEFAULT_TOLERANCE)
        status, delta = baseline.judge(measured, base, band, higher_is_better)
        {"SLOWER": slower, "FASTER": faster, "IDENTICAL": identical}[status].append(
            name
        )
        print(line(status, name, measured, base, delta, band))
    return identical, faster, slower, new


# ------------------------------------------------------------ the bench lane


def discover(bin_dir):
    """Every *_bench executable in the build's benches directory, by name."""
    found = {}
    if not os.path.isdir(bin_dir):
        return found
    for entry in sorted(os.listdir(bin_dir)):
        path = os.path.join(bin_dir, entry)
        if (
            entry.endswith("_bench")
            and os.access(path, os.X_OK)
            and os.path.isfile(path)
        ):
            found[entry] = path
    return found


def run_bench(name, binary, repetitions, min_time, warmup, timeout):
    """One binary through the JSON reporter. Returns ({bench_name: row},
    error) where each row holds the median real and CPU time in ns over
    the repetitions after the first, and the repetition count used."""
    out = os.path.join(tempfile.mkdtemp(prefix="sigil_bench_"), f"{name}.json")
    command = [
        binary,
        f"--benchmark_out={out}",
        "--benchmark_out_format=json",
        f"--benchmark_repetitions={repetitions}",
        f"--benchmark_min_time={min_time:g}s",
        f"--benchmark_min_warmup_time={warmup:g}",
        # Aggregates are computed here, over the repetitions that survive
        # the warm-up discard; the library's own would include the first.
        "--benchmark_report_aggregates_only=false",
        "--benchmark_display_aggregates_only=true",
    ]
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, timeout=timeout
        )
    except subprocess.TimeoutExpired:
        return {}, f"still running after {timeout:g}s (killed)"
    if result.returncode != 0 or not os.path.exists(out):
        return {}, (result.stderr or result.stdout).strip()[-300:]
    with open(out) as handle:
        report = json.load(handle)

    samples = {}
    for entry in report.get("benchmarks", []):
        if entry.get("run_type") != "iteration":
            continue
        if entry.get("error_occurred"):
            return {}, f"{entry['name']}: {entry.get('error_message', 'error')}"
        key = entry.get("run_name", entry["name"])
        samples.setdefault(key, []).append(
            (
                entry.get("repetition_index", 0),
                entry["real_time"] * TIME_UNITS[entry["time_unit"]],
                entry["cpu_time"] * TIME_UNITS[entry["time_unit"]],
                entry["time_unit"],
            )
        )

    rows = {}
    for key, repeats in samples.items():
        repeats.sort()
        kept = repeats[1:] if len(repeats) > 1 else repeats
        rows[key] = {
            "real_ns": statistics.median(repeat[1] for repeat in kept),
            "cpu_ns": statistics.median(repeat[2] for repeat in kept),
            "unit": kept[0][3],
            "repetitions": len(kept),
        }
    return rows, None


def merged_benches(results, standing):
    """This sweep's arms over what the baseline already held.

    A SUBSET IS A SUBSET AT EVERY LEVEL. --benches names some binaries and
    --filter names some arms inside them, and either way what was not
    measured has to survive, so the merge is per arm rather than per
    binary. Pass standing=None for a whole sweep, which is the one run
    entitled to write the file wholesale."""
    benches = {
        name: dict(rows) for name, rows in (standing or {}).get("benches", {}).items()
    }
    for name, rows in results.items():
        benches.setdefault(name, {}).update(rows)
    return benches


def bench_lane(args) -> int:
    bin_dir = str(tree.benches_dir(args.config))
    baseline_path = str(tree.PROJECT_DIR / "bench" / f"baseline_{args.config}.json")

    available = discover(bin_dir)
    if args.benches:
        missing = [name for name in args.benches if name not in available]
        if missing:
            sys.exit(
                f"no such bench binary in {bin_dir}: {' '.join(missing)} — "
                f"build the `benches` target first"
            )
        selected = {name: available[name] for name in args.benches}
    else:
        selected = available
    if not selected:
        sys.exit(f"no *_bench binaries in {bin_dir} — build the `benches` target first")

    # ONE BINARY AT A TIME. Timing wants a quiet machine, and binaries run
    # beside each other contend for it, so there is no parallel lane to
    # take: numbers taken that way are comparable to nothing.
    print(
        f"{len(selected)} benches, config {args.config}, "
        f"{args.repetitions} repetitions (first discarded), "
        f"min {args.min_time:g}s each\n"
    )

    results, errors = {}, {}
    for name, binary in selected.items():
        rows, error = run_bench(
            name,
            binary,
            args.repetitions,
            args.min_time,
            args.warmup,
            args.timeout_seconds,
        )
        if args.filter and rows:
            rows = {
                key: row for key, row in rows.items() if re.search(args.filter, key)
            }
        if error is not None:
            errors[name] = error
            print(f"  FAILED  {name}: {error}")
        else:
            results[name] = rows
            print(f"  ran     {name:<26} {len(rows)} benchmarks")

    if not results:
        print("\nnothing ran; the baseline is untouched")
        return 1

    standing = baseline.load(baseline_path)
    if args.rebase or standing is None:
        if not args.rebase:
            print(
                f"\nno baseline at {baseline_path} — writing one (this sweep "
                f"becomes the baseline)"
            )
        subset = bool(args.benches or args.filter)
        document = baseline.write(
            baseline_path,
            args.config,
            "benches",
            merged_benches(results, standing if subset else None),
        )
        print(
            f"\nbaseline written: {baseline_path} "
            f"({len(document['benches'])} benches, {len(results)} from this sweep)"
        )
        for name in sorted(results):
            for key, row in results[name].items():
                print(f"  {name}:{key:<58} {format_time(row['real_ns'])}")
        if subset:
            # What a filtered rebase kept, said out loud: the arms this run
            # never measured are the ones a merge exists to protect.
            for name in sorted(results):
                written = len(document["benches"][name])
                adopted = len(results[name])
                print(
                    f"  merged {adopted} of {written} arms in {name}; "
                    f"{written - adopted} kept at the value the baseline "
                    f"already held"
                )
        return 1 if errors else 0

    baseline.warn_host(standing)

    def line(status, name, measured, base, delta, band):
        if base is None:
            return f"  NEW        {name:<70} {format_time(measured)}"
        return (
            f"  {status:<10} {name:<70} {format_time(base)} -> "
            f"{format_time(measured)}  {delta:+6.1%} (band ±{band:.0%})"
        )

    pairs, missing = [], []
    print()
    for name in sorted(results):
        base_rows = standing.get("benches", {}).get(name, {})
        for key, row in results[name].items():
            base = base_rows.get(key)
            pairs.append(
                (f"{name}:{key}", row["real_ns"], base["real_ns"] if base else None)
            )
        for key in base_rows:
            if key not in results[name] and not args.filter:
                missing.append(f"{name}:{key}")
    identical, faster, slower, new = compare(pairs, BENCH_TOLERANCES, False, line)
    for name in missing:
        print(f"  MISSING    {name} (in baseline, not produced — rebase to drop)")

    return baseline.verdict(
        identical, faster, slower, new, missing, len(errors), "failed"
    )


# -------------------------------------------------------------- the fps lane


def parse_window_lines(text):
    """The WINDOW lines, as {stem: row} plus the stems stood down.

    A row is the key=value pairs after the name, numbers where they read
    as numbers. A line saying SKIPPED is a sketch this machine cannot run:
    not a measurement and not a failure.

    The name on a line is the sketch's STEM — the file it lives in, and
    what --sketch takes. The line is whitespace-separated, so a name
    carrying a space would end halfway through and the remainder would be
    read as fields nobody wrote."""
    rows, skipped = {}, {}
    for text_line in text.splitlines():
        found = WINDOW_LINE.match(text_line.strip())
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


def run_window(binary, args):
    command = [
        binary,
        "--window-bench",
        f"{args.seconds:g}",
        "--window-size",
        args.size,
        "--window-scale",
        f"{args.scale:g}",
    ]
    if args.sketch:
        command += ["--sketch", args.sketch]
    if args.kind:
        command += ["--kind", args.kind]
    try:
        result = subprocess.run(
            command, capture_output=True, text=True, timeout=args.timeout_seconds
        )
    except subprocess.TimeoutExpired:
        return "", f"still running after {args.timeout_seconds:g}s (killed)"
    if result.returncode != 0:
        return result.stdout, (result.stderr or result.stdout).strip()[-400:]
    return result.stdout, None


def fps_lane(args) -> int:
    binary = str(tree.sketchbook(args.config))
    baseline_path = str(tree.PROJECT_DIR / "bench" / f"app_fps_{args.config}.json")

    print(
        f"presenting in a {args.size} window at scale {args.scale:g}, "
        f"{args.seconds:g}s each, config {args.config}\n"
    )
    output, error = run_window(binary, args)
    rows, skipped = parse_window_lines(output)
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

    standing = baseline.load(baseline_path)
    if args.rebase or standing is None:
        if not args.rebase:
            print(
                f"no baseline at {baseline_path} — writing one (this sweep "
                f"becomes the baseline)"
            )
        subset = bool(args.sketch or args.kind)
        sketches = dict(standing.get("sketches", {})) if subset and standing else {}
        sketches.update(rows)
        document = baseline.write(
            baseline_path, args.config, "sketches", sketches, {"window": window}
        )
        print(
            f"baseline written: {baseline_path} "
            f"({len(document['sketches'])} sketches, {len(rows)} from this sweep)"
        )
        for name in sorted(rows):
            print(f"  {name:<28} {rows[name]['fps']:6.1f} fps")
        if subset:
            kept = len(document["sketches"]) - len(rows)
            print(
                f"  merged {len(rows)} of {len(document['sketches'])}; {kept} kept "
                f"at the value the baseline already held"
            )
        return 1 if error else 0

    baseline.warn_host(standing)
    if standing.get("window") and standing["window"] != window:
        print(
            f"WARNING: baseline was taken at {standing['window']}, this run "
            f"is {window} — a presented rate is not comparable across them\n"
        )

    base_sketches = standing.get("sketches", {})

    def line(status, name, measured, base, delta, band):
        row = rows[name]
        if base is None:
            return f"  NEW        {name:<28} {measured:6.1f} fps"
        return (
            f"  {status:<10} {name:<28} {base:6.1f} -> {measured:6.1f} fps"
            f"  {delta:+6.1%} (band ±{band:.0%})   work "
            f"{base_sketches[name].get('work', 0.0):5.2f} -> {row['work']:5.2f} ms"
        )

    pairs = [
        (name, rows[name]["fps"], (base_sketches.get(name) or {}).get("fps"))
        for name in sorted(rows)
    ]
    # No sketch has yet shown a run-to-run spread on a quiet machine that
    # the default band does not cover, so there is no table to consult.
    identical, faster, slower, new = compare(pairs, {}, True, line)

    missing = [
        name
        for name in base_sketches
        if name not in rows and name not in skipped and not (args.sketch or args.kind)
    ]
    for name in missing:
        print(f"  MISSING    {name} (in baseline, not presented — rebase to drop)")

    return baseline.verdict(
        identical, faster, slower, new, missing, 1 if error else 0, "failed"
    )


def main(argv: list) -> int:
    ap = argparse.ArgumentParser(
        prog="sigil.py bench",
        description="timing sweep judged against a committed baseline: the "
        "benchmark binaries, or the sketch registry in the real window",
    )
    ap.add_argument(
        "--lane",
        choices=("bench", "fps"),
        default="bench",
        help="bench (default): every *_bench binary, isolated. fps: every "
        "sketch presented in the real window, which needs a display and "
        "exclusive use of the machine",
    )
    ap.add_argument("--config", default="Release", choices=tree.CONFIGURATIONS)
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="write the baseline from this sweep. A narrowed sweep merges, so "
        "only an unnarrowed rebase rewrites the file wholesale",
    )
    ap.add_argument(
        "--timeout-seconds",
        type=float,
        metavar="S",
        help="ceiling per binary (bench lane, default 1800) or over the whole "
        "sweep (fps lane, default 3600)",
    )

    lane = ap.add_argument_group("the bench lane")
    lane.add_argument(
        "--benches",
        nargs="*",
        metavar="NAME",
        help="subset of *_bench binaries (default: every one the build produced)",
    )
    lane.add_argument(
        "--filter",
        metavar="REGEX",
        help="forwarded to each binary as --benchmark_filter; the comparison "
        "then covers only the benchmarks it selects",
    )
    lane.add_argument(
        "--repetitions",
        type=int,
        default=5,
        help="repetitions per benchmark; the first is discarded and the median "
        "of the rest is the number (default 5)",
    )
    lane.add_argument(
        "--min-time",
        type=float,
        default=0.1,
        metavar="S",
        help="minimum timed seconds per repetition (default 0.1)",
    )
    lane.add_argument(
        "--warmup",
        type=float,
        default=0.1,
        metavar="S",
        help="untimed warm-up seconds before each benchmark (default 0.1)",
    )

    window = ap.add_argument_group("the fps lane")
    window.add_argument(
        "--seconds",
        type=float,
        default=2.5,
        metavar="S",
        help="measured stretch per sketch, after the lane's own warm-up (default 2.5)",
    )
    window.add_argument(
        "--size",
        default="1440x900",
        metavar="WxH",
        help="the window's logical size (default 1440x900)",
    )
    window.add_argument(
        "--scale",
        type=float,
        default=2.0,
        metavar="N",
        help="Qt's scale factor for the window (default 2)",
    )
    window.add_argument("--sketch", metavar="NAME", help="present only this one")
    window.add_argument("--kind", choices=["canvas", "set"], help="present only these")

    args = ap.parse_args(argv)
    if args.timeout_seconds is None:
        args.timeout_seconds = 3600 if args.lane == "fps" else 1800
    return fps_lane(args) if args.lane == "fps" else bench_lane(args)
