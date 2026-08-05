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
  scripts/plate_ledger.py --tier quick      # the iteration loop (see below)
  scripts/plate_ledger.py --stability 3     # re-render movers 3x to
                                            # separate flappers from code
  scripts/plate_ledger.py --jobs 6 --config Release

TWO VERIFICATION TIERS (--tier):

  full (default) — CPU renders, every scene stepped from t=0 to its
  DECLARED capture moment. What it hashes is exactly what the registry
  declares, so a clean full sweep is the byte-neutrality verdict. This is
  the final confirmation gate before trusting or committing a change.

  quick — GPU renders (--gpu, still --no-promotion) with a uniform
  capture time (--capture-cap, default 2.0 s) passed to the renderer as
  --capture-at. Most of a full sweep's cost is stepping animations to
  late declared moments frame by frame on the CPU; the cap removes
  exactly that cost, which is what makes this the iteration loop. The
  cap is an OVERRIDE, not a minimum — a scene that declared an earlier
  moment renders at the cap time too — so quick hashes are not
  comparable to full ones and live in their own baseline,
  build/plate_baseline_quick_<config>.sha256, maintained by
  `--tier quick --rebase` (same subset-merge semantics as the full
  manifest). Stated blind spots, printed on every quick report: content
  that only appears after the cap is never hashed, and the backend is
  the GPU raster path rather than the full tier's CPU path — so quick
  answers "did I move any bytes I didn't mean to?" during iteration,
  and the full tier answers it authoritatively at the end.

  A rejected quick-tier design, and why: stepping every pre-capture
  frame without painting and painting only the capture frame. Several
  scenes advance their simulation inside the painted frame (cellular
  automata, particle and cloth sims), so skipping paints changes the
  final bytes for exactly the expensive scenes the tier exists to speed
  up. Any revisit of that idea needs a both-ways byte diff across the
  whole registry first.

The manifests live in build/ (machine-local on purpose: plates are
AA-deterministic per machine, not across machines). Both tiers render
with --no-promotion: automatic texture promotion re-bakes by a measured
per-frame cost, which load can tip either way, so it is the one renderer
feature a byte-identity gate must hold off — with it off, hashes are
load-immune.

Every scene render runs under a per-scene ceiling (--timeout-seconds,
default 300 s). A scene still running at the ceiling is killed and
reported FAILED-TIMEOUT by name while the rest of the sweep continues:
one runaway scene must not hang the gate that protects everything else.
SCENE_TIMEOUT_OVERRIDES raises the ceiling for scenes that are honestly
that expensive — a declared capture moment late in a costly animation is
content, not a defect, and gets its budget by name instead of inflating
everyone's.

Known self-nondeterministic scenes (the quiet-machine flap list):
genesis_fire, hitman_verlet, slitscan_2001. A mover on that list is
reported as ATTRIBUTED, not as a defect; anything else moving is a
finding.

THE GPU 60 FPS GATE (`--fps-gate`) is a SEPARATE LANE, deliberately never
mixed into the byte-identity sweep, because timing and hashing want
opposite conditions: hashing wants parallel, benchmark-free (--ledger),
--no-promotion CPU renders — load-immune because a hash cannot flap under
contention; timing wants SERIAL single-scene renders on the Graphite/Metal
backend (--gpu) with the full benchmark phases, because parallel renders
contend for the machine and corrupt every number — the same load
sensitivity that makes hashing want the opposite. The gate runs
`ComposeGallery
--headless --scene <s> --gpu`, whose work ms is CPU + drained GPU via
submit(SyncToCpu) per frame — one scene at a time, reads the steady-state
sample through --timing-json, and reports every scene whose steady frame
exceeds the 60 FPS budget (16.7 ms). Exit 1 when any scene fails the
budget. Run it on a quiet machine; there is no known-flapper list here —
a load spike is YOUR machine, rerun the scene.
"""
import argparse, concurrent.futures, hashlib, json, os, subprocess, sys, tempfile

FLAPPERS = {"genesis_fire", "hitman_verlet", "slitscan_2001"}

# Scenes whose honest render exceeds the default ceiling. chaucer_astrolabe
# declares its capture moment at 23 s into an animation whose every frame
# rasterizes a full-canvas noise shader on the CPU (the ledger's own
# --no-promotion forbids the bake that would amortize it), so its ledger
# render is ~30 minutes of legitimate work. The value is the measured cost
# plus headroom; a scene here still times out, just at its own ceiling.
SCENE_TIMEOUT_OVERRIDES = {"chaucer_astrolabe": 2400.0}

def timeout_for(scene, default):
    return SCENE_TIMEOUT_OVERRIDES.get(scene, default)

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()

def render_scene(binary, scene, outdir, timeout, extra_args=(),
                 honor_overrides=True):
    # The per-scene timeout overrides budget the FULL tier's declared-moment
    # renders. The quick tier's capture cap removes exactly that cost, so a
    # quick render still running at the default ceiling is a defect, not an
    # expensive scene — quick passes honor_overrides=False.
    if honor_overrides:
        timeout = timeout_for(scene, timeout)
    try:
        r = subprocess.run(
            [binary, "--headless", outdir, "--no-promotion", "--ledger",
             *extra_args, "--scene", scene],
            capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        # One scene consuming unbounded CPU must not hang the whole sweep:
        # the render is killed, the scene is reported by name, and every
        # other scene still gets its verdict.
        return scene, None, (f"FAILED-TIMEOUT: still rendering after "
                             f"{timeout:g}s (killed; raise "
                             f"--timeout-seconds if the scene is merely "
                             f"slow)")
    plate = os.path.join(outdir, f"gallery_{scene}.png")
    if r.returncode != 0 or not os.path.exists(plate):
        return scene, None, (r.stderr or r.stdout).strip()[-300:]
    return scene, sha256(plate), None

def fps_gate(binary, scenes, budget_ms, timeout):
    """SERIAL GPU timing sweep; see the module docstring for why serial and
    why this never shares a run with the hash sweep. Each scene gets the
    timing instrument: single-scene mode (the full unbudgeted 240-warm/120-sample
    window), --gpu (work ms = CPU + drained GPU, submit(SyncToCpu) per
    frame). --capture-at 0.02 collapses the post-sample capture pass to one
    frame — the timing sample closes before the capture pass begins (the
    JSON line snapshots at sample-window close), so this only trims wall
    clock, never the measurement; the throwaway plates land in a temp dir
    and are NOT ledger material (wrong backend, wrong conditions)."""
    outdir = tempfile.mkdtemp(prefix="fps_gate_")
    print(f"GPU 60 FPS gate: {len(scenes)} scenes, serial, budget "
          f"{budget_ms} ms\n")
    rows, failures, errors = [], [], []
    for scene in scenes:
        tj = os.path.join(outdir, f"timing_{scene}.json")
        try:
            r = subprocess.run(
                [binary, "--headless", outdir, "--gpu", "--scene", scene,
                 "--timing-json", tj, "--capture-at", "0.02"],
                capture_output=True, text=True, timeout=timeout)
        except subprocess.TimeoutExpired:
            errors.append(scene)
            print(f"  FAILED-TIMEOUT {scene}: still rendering after "
                  f"{timeout:g}s, killed")
            continue
        if r.returncode != 0 or not os.path.exists(tj):
            errors.append(scene)
            print(f"  RENDER FAILED  {scene}: "
                  f"{(r.stderr or r.stdout).strip()[-200:]}")
            continue
        with open(tj) as f:
            row = json.loads(f.readline())
        rows.append(row)
        ok = row["work_ms"] <= budget_ms
        if not ok:
            failures.append(row)
        print(f"  {'PASS' if ok else 'FAIL'}  {scene:<24} "
              f"{row['work_ms']:8.2f} ms  (p99 {row['p99_ms']:7.2f}, "
              f"{row['fps']:5.0f} fps)")
    rows.sort(key=lambda r: -r["work_ms"])
    print(f"\n{len(rows) - len(failures)} under budget, {len(failures)} "
          f"over, {len(errors)} failed to render")
    if failures:
        print(f"OVER {budget_ms} ms (steady frame, GPU):")
        for row in sorted(failures, key=lambda r: -r["work_ms"]):
            print(f"  {row['scene']:<24} {row['work_ms']:8.2f} ms "
                  f"({row['fps']:.0f} fps)")
    return 1 if failures or errors else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="Release")
    ap.add_argument("--jobs", type=int, default=max(2, (os.cpu_count() or 8) // 2))
    ap.add_argument("--tier", choices=("quick", "full"), default="full",
                    help="verification tier. full (default): CPU renders to "
                         "each scene's declared capture moment — the final "
                         "confirmation gate. quick: GPU renders with a "
                         "uniform capture-time cap, compared against the "
                         "separate quick baseline — the iteration loop. See "
                         "the module docstring for the tiers' blind spots")
    ap.add_argument("--capture-cap", type=float, default=2.0, metavar="S",
                    help="quick tier only: the uniform capture time in "
                         "seconds, passed to the renderer as --capture-at "
                         "(default 2.0). Content after this time is invisible "
                         "to the quick tier")
    ap.add_argument("--rebase", action="store_true",
                    help="write the manifest from this sweep (tier-specific: "
                         "--tier quick --rebase writes the quick baseline)")
    ap.add_argument("--stability", type=int, default=0, metavar="N",
                    help="re-render each mover N more times; a scene that "
                         "disagrees with ITSELF is attributed to the scene")
    ap.add_argument("--scenes", nargs="*", help="subset (registry names)")
    ap.add_argument("--fps-gate", action="store_true",
                    help="the GPU 60 FPS gate — a separate SERIAL lane, not "
                         "part of the byte-identity sweep (timing and "
                         "hashing want opposite conditions; see the module "
                         "docstring). Renders each scene alone with --gpu "
                         "through the full benchmark phases and fails any "
                         "whose steady work ms exceeds --budget-ms")
    ap.add_argument("--budget-ms", type=float, default=16.7,
                    help="fps-gate frame budget in ms (default 16.7 = 60 FPS)")
    ap.add_argument("--timeout-seconds", type=float, default=300, metavar="S",
                    help="per-scene render ceiling, in seconds (default 300). "
                         "A scene still running at the ceiling is killed and "
                         "reported FAILED-TIMEOUT by name while the rest of "
                         "the sweep continues — one runaway scene must not "
                         "hang the gate that protects everything else")
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    binary = os.path.join(
        root, "build/bin", args.config,
        "ComposeGallery.app/Contents/MacOS/ComposeGallery")
    # Each tier compares against its own baseline: quick plates are rendered
    # on a different backend at a different scene time, so their hashes can
    # never match the full manifest and must never be written into it.
    tier_tag = "" if args.tier == "full" else f"_{args.tier}"
    manifest = os.path.join(
        root, "build", f"plate_baseline{tier_tag}_{args.config}.sha256")
    if not os.path.exists(binary):
        sys.exit(f"no gallery binary at {binary} — build ComposeGallery first")

    # The render arguments that define the tier. GPU stability was validated
    # empirically on this machine: representative scenes hash identically
    # across processes under this exact invocation, with only the documented
    # FLAPPERS moving — the same attribution the full tier already makes.
    quick = args.tier == "quick"
    tier_args = ("--gpu", "--capture-at", f"{args.capture_cap:g}") if quick \
        else ()

    scenes = args.scenes or [
        s for s in subprocess.run(
            [binary, "--headless", "/tmp", "--list-scenes"],
            capture_output=True, text=True, check=True).stdout.splitlines()
        if s.strip()]

    if args.fps_gate:
        return fps_gate(binary, scenes, args.budget_ms, args.timeout_seconds)

    print(f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, "
          f"tier {args.tier}")
    if quick:
        print(f"QUICK TIER: GPU renders (--gpu --no-promotion), capture "
              f"capped at {args.capture_cap:g}s. Blind spots: content that "
              f"only appears after the cap is never hashed, and the backend "
              f"is the GPU raster path, not the full tier's CPU path. Run "
              f"the full tier as the final gate before trusting a "
              f"byte-neutral verdict.")

    outdir = tempfile.mkdtemp(prefix="plate_ledger_")
    results, errors = {}, {}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for scene, digest, err in pool.map(
                lambda s: render_scene(binary, s, outdir,
                                       args.timeout_seconds, tier_args,
                                       honor_overrides=not quick), scenes):
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
        # A subset rebase (--scenes ... --rebase) merges into the existing
        # manifest rather than truncating it to the subset: adopting one
        # deliberately changed plate must not silently discard the baseline
        # for every scene the sweep did not render.
        merged = {}
        if args.scenes and os.path.exists(manifest):
            with open(manifest) as f:
                for line in f:
                    digest, _, scene = line.rstrip("\n").partition("  ")
                    if scene:
                        merged[scene] = digest
        merged.update(results)
        with open(manifest, "w") as f:
            for scene in sorted(merged):
                f.write(f"{merged[scene]}  {scene}\n")
        print(f"baseline written: {manifest} ({len(merged)} scenes, "
              f"{len(results)} from this sweep)")
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
                    binary, scene, tempfile.mkdtemp(prefix="plate_stab_"),
                    args.timeout_seconds, tier_args,
                    honor_overrides=not quick)
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
