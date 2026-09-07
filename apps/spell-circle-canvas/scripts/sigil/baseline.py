"""What every ledger shares: a baseline read, merged, written and judged.

Three instruments keep a baseline — plate hashes, benchmark timings,
presented frame rates — and each takes its numbers its own way. What they
do with them is one rule: read what stands, merge a narrowed sweep over
it, write the file in one step, and judge each row against its band. That
rule is here, in both spellings a baseline comes in — the JSON documents
under bench/, and the sha256 manifest the plate tiers keep. It is a
module, not a verb: it has no main.

A BASELINE IS PER MACHINE, and each file records the host it was taken on
so a mismatch is visible rather than silently compared. A NARROWED SWEEP
MERGES: adopting one deliberately changed number must never discard a
number this run did not take, which would come back as `new`, judged
against nothing. Only an unnarrowed sweep writes a file wholesale, which
is what drops a row that no longer exists.
"""

import datetime
import fcntl
import hashlib
import json
import os
import platform
import re
from pathlib import Path

# ---------------------------------------------------------------- JSON

# The committed documents under bench/, one per build configuration, so a
# change that moves a number moves it in review.


def load(path) -> dict | None:
    if not os.path.exists(path):
        return None
    with open(path) as handle:
        return json.load(handle)


def write(path, config: str, section: str, entries: dict, extra=None) -> dict:
    """Writes the baseline document: a header naming the configuration,
    the host and the moment, then the entries under @p section, sorted by
    name. The caller has already merged a narrowed sweep's entries over
    what the file held."""
    document = {
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
    with open(path, "w") as handle:
        json.dump(document, handle, indent=1, sort_keys=False)
        handle.write("\n")
    return document


def warn_host(baseline: dict) -> None:
    if baseline.get("host") and baseline["host"] != platform.node():
        print(
            f"\nWARNING: baseline was taken on {baseline['host']}, this is "
            f"{platform.node()} — numbers are per machine"
        )


# ------------------------------------------------------------ manifest

# The plate tiers' baseline is a sha256 manifest in build/, machine-local
# on purpose: plates are deterministic per machine, not across machines.


def digest(path) -> str:
    """The sha256 of one file's contents."""
    running = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            running.update(chunk)
    return running.hexdigest()


def read_manifest(path) -> dict:
    """scene -> digest, empty when there is no manifest."""
    entries = {}
    if os.path.exists(path):
        with open(path) as handle:
            for line in handle:
                found, _, scene = line.strip().partition("  ")
                if scene:
                    entries[scene] = found
    return entries


def write_manifest(path, keep, results: dict) -> dict:
    """The manifest, replaced whole, with @p results merged over whichever
    of its entries @p keep selects from the file AS IT STANDS.

    A sweep takes minutes and the merge is decided at the end of them, so
    the manifest is re-read here rather than reused from the copy the run
    judged against: a rebase that landed in between wrote entries this one
    never saw, and merging into the older copy would drop them. The lock
    makes the read-modify-write one step against another writer holding
    the same lock, and the temp file plus rename makes it one step against
    everything else — a reader never sees half a manifest, and a run that
    dies mid-write leaves the previous one intact.

    @p keep answers which of the standing entries survive: None for a
    whole sweep, which is the one run entitled to drop what no longer
    exists; True for a narrowed sweep, which keeps every entry it did not
    render; or the set of scene names this sweep had nothing to say
    about."""
    lock = f"{path}.lock"
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(lock, "w") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        merged = {}
        if keep is not None:
            standing = read_manifest(path)
            merged = (
                standing
                if keep is True
                else {name: found for name, found in standing.items() if name in keep}
            )
        merged.update(results)
        temporary = f"{path}.{os.getpid()}.tmp"
        with open(temporary, "w") as out:
            out.writelines(f"{merged[scene]}  {scene}\n" for scene in sorted(merged))
        os.replace(temporary, path)
    return merged


# ------------------------------------------------------------- verdict


def band_for(name: str, table: dict, default: float) -> float:
    """The band for one row: the first pattern in the table that matches
    its name, else the default. A widened band is a statement about the
    row's own run-to-run spread on a quiet machine, never a way past a
    finding."""
    for pattern, band in table.items():
        if re.search(pattern, name):
            return band
    return default


def judge(measured: float, base: float, band: float, higher_is_better: bool):
    """One row against its baseline value: (status, delta). Within the
    band a row is IDENTICAL; past it in the good direction FASTER, in the
    bad direction SLOWER."""
    ratio = measured / base if base else 1.0
    delta = ratio - 1.0
    worse = delta < -band if higher_is_better else delta > band
    better = delta > band if higher_is_better else delta < -band
    return ("SLOWER" if worse else "FASTER" if better else "IDENTICAL"), delta


def verdict(identical, faster, slower, new, missing, failed: int, failed_word: str):
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
