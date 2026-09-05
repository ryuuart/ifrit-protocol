"""What the two timing ledgers share: the committed baseline file, the
tolerance table, and the verdict over measured-against-baseline rows.

app_fps_ledger.py measures presented frame rates and bench_ledger.py
measures benchmark timings; each takes its numbers its own way and
keeps its own baseline under bench/, but a baseline is read, merged
and written the same way in both, a tolerance band is found the same
way, and a row is judged IDENTICAL / FASTER / SLOWER by the same
arithmetic. This is a module, not a command — it has no main.

THE BASELINE IS COMMITTED, one file per build configuration, so a
change that moves a number moves it in review. Numbers are per
machine: a baseline taken on one host says nothing about another, and
the file records the host it was taken on so a mismatch is visible
rather than silently compared.
"""

import datetime
import json
import os
import platform
import re


def tolerance_for(name, table, default):
    """The band for one row: the first pattern in the table that matches
    its name, else the default. A widened band is a statement about the
    row's own run-to-run spread on a quiet machine, never a way past a
    finding."""
    for pattern, band in table.items():
        if re.search(pattern, name):
            return band
    return default


def load_baseline(path):
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return json.load(f)


def write_baseline(path, config, section, entries, extra=None):
    """Writes the baseline document: a header naming the configuration,
    the host and the moment, then the entries under @p section, sorted
    by name. The caller has already merged a narrowed sweep's entries
    over what the file held — only an unnarrowed sweep is entitled to
    write the file wholesale and thereby drop what no longer exists."""
    doc = {
        "config": config,
        "host": platform.node(),
        "machine": platform.machine(),
        **(extra or {}),
        "taken": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="seconds"
        ),
        section: {name: entries[name] for name in sorted(entries)},
    }
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(doc, f, indent=1, sort_keys=False)
        f.write("\n")
    return doc


def warn_host(baseline):
    if baseline.get("host") and baseline["host"] != platform.node():
        print(
            f"\nWARNING: baseline was taken on {baseline['host']}, this is "
            f"{platform.node()} — numbers are per machine"
        )


def judge(measured, base, band, higher_is_better):
    """One row against its baseline value: (status, delta). Within the
    band a row is IDENTICAL; past it in the good direction FASTER, in
    the bad direction SLOWER."""
    ratio = measured / base if base else 1.0
    delta = ratio - 1.0
    worse = delta < -band if higher_is_better else delta > band
    better = delta > band if higher_is_better else delta < -band
    return ("SLOWER" if worse else "FASTER" if better else "IDENTICAL"), delta


def verdict(identical, faster, slower, new, missing, failed, failed_word):
    """The summary lines and the exit status. SLOWER rows and failures
    fail the run; NEW and MISSING rows do not, since the fix for both is
    --rebase."""
    print(
        f"\n{len(identical)} identical, {len(faster)} faster, {len(slower)} slower, "
        f"{len(new)} new, {len(missing)} missing, {failed} {failed_word}"
    )
    if slower:
        print("SLOWER beyond band:")
        for name in slower:
            print(f"  {name}   <-- FINDING")
    if not slower and not failed:
        print("VERDICT: within band")
    return 1 if slower or failed else 0
