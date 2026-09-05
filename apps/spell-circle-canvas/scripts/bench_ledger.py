#!/usr/bin/env python3
"""The bench ledger, as ONE command: timing sweeps over the benchmark
binaries, compared against a committed baseline.

Runs every `*_bench` binary the build produced — one per library, under
bin/<config>/benches — (or the subset named with
--benches), each with Google Benchmark's JSON reporter and a fixed number
of repetitions, takes the MEDIAN real time of every benchmark, and
compares it against the baseline stored for the build configuration —
printing IDENTICAL / FASTER / SLOWER rows and one verdict. The plate
ledger does this for bytes; this does it for time.

Usage (from apps/spell-circle-canvas):
  scripts/bench_ledger.py --rebase              # bake the baseline
  scripts/bench_ledger.py                       # sweep + compare + verdict
  scripts/bench_ledger.py --benches weave_bench geometry_bench
  scripts/bench_ledger.py --config Release --repetitions 7 --min-time 0.2

HOW A NUMBER IS TAKEN. Each benchmark runs --repetitions times (default
5), every repetition long enough to satisfy --min-time (default 0.1 s)
after a warm-up period (--warmup, default 0.1 s) that is never timed.
The first repetition is discarded as well — it is the one that pays for
page faults, lazily built caches and frequency ramp — and the median of
the remaining repetitions is the number. Real (wall-clock) time is what
is compared, since it is what a frame budget is spent in; CPU time is
kept in the baseline for reference but never judged.

HOW A NUMBER IS JUDGED. Each benchmark has a tolerance band, ±10 % by
default; a benchmark whose noise is honestly wider is given its own band
by name in TOLERANCES below rather than widening everyone's. Within the
band a row is IDENTICAL, below it FASTER, above it SLOWER, and any SLOWER
row fails the run. A benchmark the baseline has never seen is NEW and a
baseline entry no run produced is MISSING; neither fails the run, since
the fix for both is --rebase.

THE MACHINE MUST BE QUIET. Timing wants the opposite conditions from
hashing: the binaries run one at a time (--jobs 1, the default, and the
only setting whose numbers mean anything), and a build, a browser or a
sync client running beside them corrupts every number. A SLOWER row on a
busy machine is the machine, not the code — rerun the bench alone before
reading it as a finding.

THE BASELINE IS COMMITTED under bench/baseline_<config>.json. --rebase
writes the file from this sweep, and A NARROWED SWEEP MERGES: with
--benches it keeps the binaries it did not run, with --filter it keeps
the arms of a binary it did not select, and with both it keeps both.
Adopting one deliberately changed number never discards a number this
run did not take — those would come back as `new`, judged against
nothing, which is worse than the change being visible. The rebase says
per binary how many arms it adopted and how many it kept. Only an
unnarrowed sweep writes the file wholesale, which is what drops a
benchmark that no longer exists.
"""

import argparse
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile

import ledger

# Per-benchmark tolerance bands, keyed by a regular expression matched
# against the full benchmark name ("binary:BM_Name/args"). Only for
# benchmarks whose run-to-run spread on a quiet machine honestly exceeds
# the default; a widened band is a statement about the benchmark, not
# a way past a finding. The default applies to everything unmatched.
DEFAULT_TOLERANCE = 0.10
TOLERANCES = {
    # Sub-microsecond bodies: a hash, a lookup, a resolve. The timer's own
    # resolution and the loop overhead are a visible fraction of each.
    r":BM_Noise": 0.15,
    # The cold arms purge and refill a cache inside each repetition and
    # pay HarfBuzz for every word; allocator state moves them more than
    # the warm arms.
    r"_Cold": 0.15,
    r"ReplaceWholeParagraph_Cold": 0.15,
    # Raster painting through Skia's CPU backend has thread-pool warm-up
    # inside it that the warm-up period does not fully settle.
    r"^weave_bench:BM_Draw": 0.15,
    # A latency, not a cost: the arm waits for the web thread's own
    # repaint, which is paced at a fixed cadence, so the figure moves by a
    # whole frame interval from one run to the next.
    r"^scry_bench:BM_Page_ChangeLatency": 0.75,
    # The smallest device cook is mostly the fixed cost of putting work on
    # the device and taking the answer back, and the driver pays that in
    # one of two states — the arm's median lands in one or the other, a
    # quarter apart, on a machine doing nothing else. Its own band, not
    # the benchmark's: the arms with ten and fifty times the points hold
    # to a percent, and they are where a change to the cook shows.
    r"^world_bench:BM_ChainOnDevice/20000/": 0.35,
}

TIME_UNITS = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}


def to_ns(value, unit):
    return value * TIME_UNITS[unit]


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
    out = os.path.join(tempfile.mkdtemp(prefix="bench_ledger_"), f"{name}.json")
    cmd = [
        binary,
        f"--benchmark_out={out}",
        "--benchmark_out_format=json",
        f"--benchmark_repetitions={repetitions}",
        f"--benchmark_min_time={min_time:g}s",
        f"--benchmark_min_warmup_time={warmup:g}",
        # Aggregates are computed here, over the repetitions that
        # survive the warm-up discard; the library's own would include
        # the first.
        "--benchmark_report_aggregates_only=false",
        "--benchmark_display_aggregates_only=true",
    ]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return {}, f"still running after {timeout:g}s (killed)"
    if r.returncode != 0 or not os.path.exists(out):
        return {}, (r.stderr or r.stdout).strip()[-300:]
    with open(out) as f:
        report = json.load(f)

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
                to_ns(entry["real_time"], entry["time_unit"]),
                to_ns(entry["cpu_time"], entry["time_unit"]),
                entry["time_unit"],
            )
        )

    rows = {}
    for key, reps in samples.items():
        reps.sort()
        kept = reps[1:] if len(reps) > 1 else reps
        rows[key] = {
            "real_ns": statistics.median(rep[1] for rep in kept),
            "cpu_ns": statistics.median(rep[2] for rep in kept),
            "unit": kept[0][3],
            "repetitions": len(kept),
        }
    return rows, None


def fmt_time(ns):
    for unit, scale in (("s", 1e9), ("ms", 1e6), ("us", 1e3)):
        if ns >= scale:
            return f"{ns / scale:8.3f} {unit}"
    return f"{ns:8.1f} ns"


def merged_benches(results, baseline):
    """This sweep's arms over what the baseline already held.

    A SUBSET IS A SUBSET AT EVERY LEVEL. --benches names some binaries
    and --filter names some arms inside them, and either way what was
    not measured has to survive: adopting one deliberately changed arm
    must not drop the arms this run never ran, which would leave them
    unjudged until someone noticed them reported as new. So the merge
    is per arm, not per binary. Pass baseline=None for a whole sweep,
    which is the one run entitled to write the file wholesale and thereby
    to drop what no longer exists."""
    benches = {
        name: dict(rows) for name, rows in (baseline or {}).get("benches", {}).items()
    }
    for name, rows in results.items():
        benches.setdefault(name, {}).update(rows)
    return benches


def main():
    ap = argparse.ArgumentParser(
        description="timing sweep over the benchmark binaries, judged "
        "against a committed baseline"
    )
    ap.add_argument("--config", default="Release")
    ap.add_argument(
        "--benches",
        nargs="*",
        metavar="NAME",
        help="subset of *_bench binaries (default: every one the build produced)",
    )
    ap.add_argument(
        "--filter",
        metavar="REGEX",
        help="forwarded to each binary as --benchmark_filter; the comparison "
        "then covers only the benchmarks it selects",
    )
    ap.add_argument(
        "--repetitions",
        type=int,
        default=5,
        help="repetitions per benchmark; the first is discarded and the "
        "median of the rest is the number (default 5)",
    )
    ap.add_argument(
        "--min-time",
        type=float,
        default=0.1,
        metavar="S",
        help="minimum timed seconds per repetition (default 0.1)",
    )
    ap.add_argument(
        "--warmup",
        type=float,
        default=0.1,
        metavar="S",
        help="untimed warm-up seconds before each benchmark (default 0.1)",
    )
    ap.add_argument(
        "--jobs",
        type=int,
        default=1,
        help="binaries run at once. 1 (the default) is the only value whose "
        "numbers mean anything — timing wants a quiet machine — and is "
        "what every baseline is taken at; higher values exist for a smoke "
        "run where the verdict will be ignored",
    )
    ap.add_argument(
        "--timeout-seconds",
        type=float,
        default=1800,
        metavar="S",
        help="per-binary ceiling (default 1800); a binary still running at "
        "the ceiling is killed and reported FAILED-TIMEOUT by name",
    )
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="write the baseline from this sweep. A narrowed sweep merges: "
        "--benches keeps the binaries it did not run and --filter keeps "
        "the arms it did not select, so only an unnarrowed rebase "
        "rewrites the file wholesale",
    )
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    bin_dir = os.path.join(root, "build", "bin", args.config, "benches")
    baseline_path = os.path.join(root, "bench", f"baseline_{args.config}.json")

    available = discover(bin_dir)
    if args.benches:
        missing = [b for b in args.benches if b not in available]
        if missing:
            sys.exit(
                f"no such bench binary in {bin_dir}: {' '.join(missing)} — "
                f"build the `benches` target first"
            )
        selected = {b: available[b] for b in args.benches}
    else:
        selected = available
    if not selected:
        sys.exit(f"no *_bench binaries in {bin_dir} — build the `benches` target first")

    if args.jobs != 1:
        print(
            f"WARNING: --jobs {args.jobs}: binaries contend for the machine; "
            f"the numbers below are not comparable to a baseline"
        )
    print(
        f"{len(selected)} benches, config {args.config}, "
        f"{args.repetitions} repetitions (first discarded), "
        f"min {args.min_time:g}s each, {args.jobs} job(s)\n"
    )

    def run_one(item):
        name, binary = item
        rows, err = run_bench(
            name,
            binary,
            args.repetitions,
            args.min_time,
            args.warmup,
            args.timeout_seconds,
        )
        if args.filter and rows:
            rows = {k: v for k, v in rows.items() if re.search(args.filter, k)}
        return name, rows, err

    results, errors = {}, {}
    if args.jobs == 1:
        outcomes = map(run_one, selected.items())
    else:
        import concurrent.futures

        pool = concurrent.futures.ThreadPoolExecutor(args.jobs)
        outcomes = pool.map(run_one, selected.items())
    for name, rows, err in outcomes:
        if err is not None:
            errors[name] = err
            print(f"  FAILED  {name}: {err}")
        else:
            results[name] = rows
            print(f"  ran     {name:<26} {len(rows)} benchmarks")

    if not results:
        print("\nnothing ran; the baseline is untouched")
        return 1

    baseline = ledger.load_baseline(baseline_path)
    if args.rebase or baseline is None:
        if not args.rebase:
            print(
                f"\nno baseline at {baseline_path} — writing one (this sweep "
                f"becomes the baseline)"
            )
        # A sweep narrowed by binary OR by arm merges into what is already
        # there; only an unnarrowed sweep writes the file wholesale.
        subset = bool(args.benches or args.filter)
        doc = ledger.write_baseline(
            baseline_path,
            args.config,
            "benches",
            merged_benches(results, baseline if subset else None),
        )
        print(
            f"\nbaseline written: {baseline_path} "
            f"({len(doc['benches'])} benches, {len(results)} from this sweep)"
        )
        for name in sorted(results):
            for key, row in results[name].items():
                print(f"  {name}:{key:<58} {fmt_time(row['real_ns'])}")
        if subset:
            # What a filtered rebase kept, said out loud: the arms this
            # run never measured are the ones a merge exists to protect,
            # and the count is where a reader sees that it did.
            for name in sorted(results):
                written = len(doc["benches"][name])
                adopted = len(results[name])
                print(
                    f"  merged {adopted} of {written} arms in {name}; "
                    f"{written - adopted} kept at the value the baseline "
                    f"already held"
                )
        return 1 if errors else 0

    ledger.warn_host(baseline)

    identical, faster, slower, new, missing = [], [], [], [], []
    print()
    for name in sorted(results):
        base_rows = baseline.get("benches", {}).get(name, {})
        for key, row in results[name].items():
            full = f"{name}:{key}"
            base = base_rows.get(key)
            if base is None:
                new.append(full)
                print(f"  NEW        {full:<70} {fmt_time(row['real_ns'])}")
                continue
            band = ledger.tolerance_for(full, TOLERANCES, DEFAULT_TOLERANCE)
            status, delta = ledger.judge(
                row["real_ns"], base["real_ns"], band, higher_is_better=False
            )
            {"SLOWER": slower, "FASTER": faster, "IDENTICAL": identical}[status].append(
                full
            )
            print(
                f"  {status:<10} {full:<70} {fmt_time(base['real_ns'])} -> "
                f"{fmt_time(row['real_ns'])}  {delta:+6.1%} (band ±{band:.0%})"
            )
        for key in base_rows:
            if key not in results[name] and not args.filter:
                missing.append(f"{name}:{key}")
    for full in missing:
        print(f"  MISSING    {full} (in baseline, not produced — rebase to drop)")

    return ledger.verdict(
        identical, faster, slower, new, missing, len(errors), "failed"
    )


if __name__ == "__main__":
    sys.exit(main())
