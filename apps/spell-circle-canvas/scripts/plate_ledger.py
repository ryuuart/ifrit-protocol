#!/usr/bin/env python3
"""The plate ledger, as ONE command: parallel byte-identity sweeps.

Renders every gallery scene through `ComposeGallery --headless --ledger`
(the benchmark-free exact-stepped capture), N scenes at a time, hashes
the plates, and compares against a stored baseline manifest — printing
movers with the known self-nondeterministic scenes attributed instead of
alarmed about. What used to be a ~40-minute serial sweep plus ad-hoc
hashing is a few minutes of parallel renders and one verdict table.

Usage (from apps/spell-circle-canvas):
  scripts/plate_ledger.py --rebase          # bake the baseline manifest
  scripts/plate_ledger.py                   # sweep + compare + verdict
  scripts/plate_ledger.py --stability 3     # re-render movers 3x to
                                            # separate flappers from code
  scripts/plate_ledger.py --jobs 6 --config Release

The manifest lives in build/ (machine-local on purpose: plates are
AA-deterministic per machine, not across machines). Always renders with
--no-promotion — the documented nondeterminism control (ROADMAP §33 R4
methodology) — so hashes are load-immune.

Known self-nondeterministic scenes (quiet-machine flap list, ROADMAP §33):
genesis_fire, hitman_verlet, slitscan_2001. A mover on that list is
reported as ATTRIBUTED, not as a defect; anything else moving is a
finding.
"""
import argparse, concurrent.futures, hashlib, os, subprocess, sys, tempfile

FLAPPERS = {"genesis_fire", "hitman_verlet", "slitscan_2001"}

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def render_scene(binary, scene, outdir):
    r = subprocess.run(
        [binary, "--headless", outdir, "--no-promotion", "--ledger",
         "--scene", scene],
        capture_output=True, text=True)
    plate = os.path.join(outdir, f"gallery_{scene}.png")
    if r.returncode != 0 or not os.path.exists(plate):
        return scene, None, (r.stderr or r.stdout).strip()[-300:]
    return scene, sha256(plate), None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="Release")
    ap.add_argument("--jobs", type=int, default=max(2, (os.cpu_count() or 8) // 2))
    ap.add_argument("--rebase", action="store_true",
                    help="write the manifest from this sweep")
    ap.add_argument("--stability", type=int, default=0, metavar="N",
                    help="re-render each mover N more times; a scene that "
                         "disagrees with ITSELF is attributed to the scene")
    ap.add_argument("--scenes", nargs="*", help="subset (registry names)")
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    binary = os.path.join(
        root, "build/bin", args.config,
        "ComposeGallery.app/Contents/MacOS/ComposeGallery")
    manifest = os.path.join(root, "build", f"plate_baseline_{args.config}.sha256")
    if not os.path.exists(binary):
        sys.exit(f"no gallery binary at {binary} — build ComposeGallery first")

    scenes = args.scenes or [
        s for s in subprocess.run(
            [binary, "--headless", "/tmp", "--list-scenes"],
            capture_output=True, text=True, check=True).stdout.splitlines()
        if s.strip()]
    print(f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}")

    outdir = tempfile.mkdtemp(prefix="plate_ledger_")
    results, errors = {}, {}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for scene, digest, err in pool.map(
                lambda s: render_scene(binary, s, outdir), scenes):
            if digest is None:
                errors[scene] = err
            else:
                results[scene] = digest
    for scene, err in sorted(errors.items()):
        print(f"RENDER FAILED  {scene}: {err}")

    if args.rebase or not os.path.exists(manifest):
        if not args.rebase:
            print(f"no manifest at {manifest} — writing one (this sweep "
                  f"becomes the baseline)")
        with open(manifest, "w") as f:
            for scene in sorted(results):
                f.write(f"{results[scene]}  {scene}\n")
        print(f"baseline written: {manifest} ({len(results)} scenes)")
        return 0 if not errors else 1

    baseline = {}
    for line in open(manifest):
        digest, _, scene = line.strip().partition("  ")
        baseline[scene] = digest

    movers, missing = [], []
    for scene, digest in sorted(results.items()):
        if scene not in baseline:
            missing.append(scene)
        elif baseline[scene] != digest:
            movers.append(scene)
    identical = len(results) - len(movers) - len(missing)
    print(f"\n{identical} byte-identical, {len(movers)} moved, "
          f"{len(missing)} not in baseline, {len(errors)} failed")

    verdict = 0
    for scene in movers:
        if scene in FLAPPERS:
            print(f"  MOVED (attributed) {scene} — on the documented "
                  f"self-nondeterministic list")
            continue
        if args.stability > 0:
            rerenders = {results[scene]}
            for _ in range(args.stability):
                _, digest, err = render_scene(
                    binary, scene, tempfile.mkdtemp(prefix="plate_stab_"))
                if digest:
                    rerenders.add(digest)
            if len(rerenders) > 1:
                print(f"  MOVED (self-unstable) {scene} — disagrees with "
                      f"itself across {args.stability + 1} renders; "
                      f"attribute to the scene, consider adding to FLAPPERS")
                continue
        print(f"  MOVED  {scene}  {baseline[scene][:12]} -> "
              f"{results[scene][:12]}   <-- FINDING")
        verdict = 1
    for scene in missing:
        print(f"  NEW    {scene} (not in baseline — rebase to adopt)")
    if verdict == 0 and not errors:
        print("VERDICT: byte-neutral (modulo attributed scenes)")
    return verdict or (1 if errors else 0)

if __name__ == "__main__":
    sys.exit(main())
