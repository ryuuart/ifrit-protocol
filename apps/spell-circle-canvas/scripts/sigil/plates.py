"""Verb: plates — parallel plate sweeps over the sketch registry.

    sigil.py plates --rebase           # bake the baseline manifest
    sigil.py plates                    # sweep + compare + verdict
    sigil.py plates --kind set         # only the sketches that light a set
    sigil.py plates --sketch astral_tome
    sigil.py plates --scenes "aero desktop" black_watch
    sigil.py plates --stability 3      # re-render movers 3x to separate
                                       # flappers from code
    sigil.py plates --tier device      # the same sketches on the GPU
    sigil.py plates --tier promotion   # …and with the promoter let go

Renders every sketch through `Sketchbook --headless --ledger` (the
benchmark-free exact-stepped capture), N at a time, hashes the plates,
and compares against a stored baseline manifest. One binary renders all
three tiers; what separates them is which rasteriser a sketch draws
through and what the runtime's promoter is allowed to do.

What each tier judges, what it refuses and why each ceiling stands where
it does is scripts/README.md; this refuses to keep a second copy of it.
The judgement is here because a ceiling is a tolerance about a machine
and not a fact about two files: the manifest, the tolerances, the
promotion ceiling and --stability are what this owns, and decoding,
differencing and thumbnailing plates is Sketchbook's.
"""

import argparse
import concurrent.futures
import os
import shutil
import subprocess
import sys
import time

from sigil import baseline, tree

# One prefix names every plate.
PLATE_PREFIX = "plate_"
KINDS = ("canvas", "set", "draw")

# The flag every render carries: the benchmark-free exact-stepped capture.
RENDER_ARGS = ("--ledger",)
# …and what each tier says about the promoter. Held off wherever a hash is
# the verdict, because cost-based re-baking decides by a measured per-frame
# cost that load tips either way; turned on for the tier whose whole subject
# it is.
PROMOTION_OFF = ("--no-promotion",)
PROMOTION_ON = ("--promotion",)

# How far a sketch's device plate may stand from its CPU plate: (mean,
# p99) per colour channel in 0..255. Set from what the two tiers actually
# do, and tightened when one of them gets closer to the other rather than
# loosened when a change moves them apart. What each entry is answering
# is the table in scripts/README.md.
DEFAULT_GPU_TOLERANCE = (12.0, 128)
GPU_TOLERANCE = {
    "first_light": (10.0, 96),
    "glow_trail": (4.0, 32),
    "material_lab": (10.0, 192),
    "key_light": (3.0, 32),
    "dart_flight": (2.0, 24),
    "deformed_cloud": (2.0, 24),
    "scattered_model": (4.0, 128),
    "lantern_room": (4.0, 64),
}

# Scenes whose SUBJECT is the difference between the two backends, so the
# tier measures them and does not judge them. A scene belongs here only
# when what it draws IS the divergence, and the entry says which draw.
DEVICE_DIVERGENT = {
    "nine slice": "the trap cell calls Skia's own drawImageLattice, which "
    "a device implements with an empty body — the empty cell is the lesson",
}

# How far a promoted plate may stand from the same scene rendered with the
# promoter held off: one code value on any channel of any pixel. It is not
# a tolerance anyone chose. A promoted node is baked under the live matrix
# post-translated by an integer, and inverting that matrix to find a
# shader's local coordinates does not cancel the integer to the last bit at
# a scale whose reciprocal is inexact — so a shaded pixel can land one code
# value from the live paint and nothing may land further. Anything past it
# is a picture that moved rather than a picture that rounded.
PROMOTION_DRIFT_CEILING = 1


def registry(binary, kinds):
    """What the binary carries for the given kinds: scene -> kind for the
    ones this machine can render, and scene -> reason for the ones it
    cannot.

    ONE LISTING LINE PER SKETCH, and the ones this machine cannot run
    carry a tab and the reason. They stay in the listing on purpose: a
    sketch dropped from it and a sketch deleted from the tree read
    exactly alike, and the difference is the whole point."""
    scenes, unavailable = {}, {}
    for kind in kinds:
        listed = tree.capture([binary, "--list", "--kind", kind], check=True).stdout
        for line in listed.splitlines():
            if not line.strip():
                continue
            name, tab, note = line.partition("\t")
            if tab:
                unavailable[name] = note.removeprefix("unavailable: ")
            else:
                scenes[name] = kind
    return scenes, unavailable


def render_scene(binary, scene, outdir, timeout, extra_args=PROMOTION_OFF):
    """Render one scene; returns (scene, digest, error, elapsed seconds).

    The elapsed time is reported for every outcome, so a sweep can name
    what it is still waiting on rather than going quiet behind its
    slowest scene."""
    started = time.monotonic()
    try:
        result = subprocess.run(
            [
                binary,
                "--headless",
                outdir,
                *RENDER_ARGS,
                *extra_args,
                "--sketch",
                scene,
            ],
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired:
        # One scene consuming unbounded CPU must not hang the whole sweep:
        # the render is killed, the scene is reported by name, and every
        # other scene still gets its verdict.
        return (
            scene,
            None,
            (
                f"FAILED-TIMEOUT: still rendering after {timeout:g}s (killed; "
                f"raise --timeout-seconds if the scene is merely slow)"
            ),
            time.monotonic() - started,
        )
    elapsed = time.monotonic() - started
    plate = plate_path(outdir, scene)
    if result.returncode != 0 or not os.path.exists(plate):
        return scene, None, (result.stderr or result.stdout).strip()[-300:], elapsed
    return scene, baseline.digest(plate), None, elapsed


def compared(binary, first, second):
    """Every plate in both directories, differenced by the renderer that
    wrote them: name -> (mean, p99, max), plus the names it could not
    compare.

    Decoding a PNG and differencing two pictures is what the binary
    already does; what stays here is the judgement — which distance is
    close enough on this machine — because that is a tolerance and not a
    fact about two files."""
    result = tree.capture([binary, "--compare", first, second])
    distances, unusable = {}, {}
    for line in result.stdout.splitlines():
        words = line.split()
        # A registry name CAN CARRY SPACES, so every row is read from its
        # ends inward: the verb is the first word, the fixed-width tail is
        # the last, and whatever lies between them is the name.
        if len(words) >= 8 and words[0] == "compared" and words[-6] == "mean":
            name = " ".join(words[1:-6])
            distances[name] = (float(words[-5]), int(words[-3]), int(words[-1]))
        elif words and words[0] == "size" and len(words) >= 4:
            name = " ".join(words[1:-2])
            unusable[name] = "size " + " ".join(words[-2:])
        elif words and words[0] in ("missing", "unreadable") and len(words) >= 3:
            name = " ".join(words[1:-1])
            unusable[name] = f"{words[0]} {words[-1]}"
    if not distances and not unusable:
        unusable["--compare"] = (result.stderr or result.stdout).strip()[-300:]
    return distances, unusable


def plate_dir(config, *parts, fresh=True):
    """A KEPT plate directory under build/, beside the manifest.

    A verdict of MOVED is a hash disagreeing with a hash, which says
    nothing about WHERE the picture moved. The two plates behind the two
    hashes answer that, so they are written where they can still be
    opened and differenced after the run rather than into a directory
    removed at exit. build/ is ignored, so nothing here is ever committed.

    @p fresh empties the directory first, which is what a run's own output
    wants: a sweep narrowed to two scenes must not leave the other
    hundred's plates standing beside them, or `--compare` would report
    scenes this run never rendered. The baseline directory is the one that
    is NOT fresh — it is overwritten scene by scene as a rebase adopts
    them, and pruned to the manifest afterwards."""
    directory = tree.build_dir() / f"plates_{config}"
    for part in parts:
        directory = directory / part
    if fresh:
        shutil.rmtree(directory, ignore_errors=True)
    directory.mkdir(parents=True, exist_ok=True)
    return str(directory)


def plate_path(directory, scene):
    """Where one scene's plate lands. One spelling, because the sweep
    writes it, the hash reads it and the verdict prints it."""
    return os.path.join(directory, f"{PLATE_PREFIX}{scene}.png")


def prune_plates(directory, scenes):
    """Drop the plates of scenes the manifest no longer carries, so the
    kept baseline and the manifest beside it name the same set."""
    for name in os.listdir(directory):
        scene = name.removeprefix(PLATE_PREFIX).removesuffix(".png")
        if name.startswith(PLATE_PREFIX) and scene not in scenes:
            os.remove(os.path.join(directory, name))


def sweep(binary, scenes, outdir, timeout, jobs, extra_args, standing):
    """Renders every scene, N at a time, printing one line per scene as
    it finishes. Returns (scene -> digest, scene -> error). @p standing
    names how a rendered scene stands, given its digest."""
    results, errors = {}, {}
    # Submitted rather than mapped, because map yields in submission
    # order and would hold every finished scene's line behind an
    # unfinished earlier one.
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        pending = [
            pool.submit(render_scene, binary, scene, outdir, timeout, extra_args)
            for scene in scenes
        ]
        for done, future in enumerate(concurrent.futures.as_completed(pending), 1):
            scene, digest, err, elapsed = future.result()
            if digest is None:
                errors[scene] = err
            else:
                results[scene] = digest
            state = "FAILED" if digest is None else standing(scene, digest)
            print(
                f"  [{done:>3}/{len(scenes)}] {state:<9} {scene:<24} {elapsed:6.1f}s",
                flush=True,
            )
    for scene, err in sorted(errors.items()):
        print(f"RENDER FAILED  {scene}: {err}")
    return results, errors


def device_sweep(binary, scenes, timeout, jobs, host_dir, device_dir):
    """The device tier: every sketch rendered BOTH ways and the two plates
    compared. It has no baseline — the CPU plate of the same sketch IS
    the reference, and both are made in this run, and both are kept so a
    scene reported OVER can be looked at rather than only measured."""

    # One sketch first, to tell "no device on this machine" from a defect.
    probe = subprocess.run(
        [binary, "--headless", device_dir, "--gpu", "--sketch", scenes[0]],
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    if probe.returncode != 0 and "no device runtime" in (probe.stderr + probe.stdout):
        print("SKIPPED: this machine has no device to render on")
        return 0

    print("[cpu]")
    _, cpu_errors = sweep(
        binary, scenes, host_dir, timeout, jobs, PROMOTION_OFF, lambda s, d: "rendered"
    )
    print("[gpu]")
    _, gpu_errors = sweep(
        binary,
        scenes,
        device_dir,
        timeout,
        jobs,
        PROMOTION_OFF + ("--gpu",),
        lambda s, d: "rendered",
    )
    errors = len(cpu_errors) + len(gpu_errors)

    verdict = 0
    print()
    distances, unusable = compared(binary, host_dir, device_dir)
    for scene in scenes:
        if scene in unusable:
            print(f"  {unusable[scene].upper()} {scene}   <-- FINDING")
            verdict = 1
            continue
        if scene not in distances:
            verdict = 1
            continue
        mean, p99, worst = distances[scene]
        if scene in DEVICE_DIVERGENT:
            print(
                f"  DRAWS IT {scene:<24} "
                f"mean {mean:6.2f}  p99 {p99:4d}  max {worst:3d}"
                f"\n           {DEVICE_DIVERGENT[scene]}"
            )
            continue
        mean_cap, p99_cap = GPU_TOLERANCE.get(scene, DEFAULT_GPU_TOLERANCE)
        over = mean > mean_cap or p99 > p99_cap
        print(
            f"  {'OVER ' if over else 'WITHIN'} {scene:<24} "
            f"mean {mean:6.2f} (<= {mean_cap:g})  "
            f"p99 {p99:4d} (<= {p99_cap})  max {worst:3d}"
        )
        if over:
            verdict = 1
    print(f"\nplates kept: {host_dir}\n             {device_dir}")
    if verdict == 0 and not errors:
        print("VERDICT: the device tier stands within tolerance of the CPU tier")
    return verdict or (1 if errors else 0)


def promotion_sweep(binary, scenes, timeout, jobs, off_dir, on_dir):
    """The promotion tier: every sketch rendered with the promoter held off
    and again with it on, and the two plates differenced.

    It has no baseline. Both plates are made in this run and the held-off
    one IS the reference, because the question is not what a sketch draws
    but whether the runtime's own re-baking changes it.

    THE ON HALF IS EAGER: the runtime bakes every node its rules admit,
    from that node's first frame, instead of the handful a stopwatch found
    expensive under this run's load. So the tier tests the same node set on
    every machine, and all of the promotable set rather than the few slow
    nodes — an idle machine promotes nothing by cost and would report a
    clean sweep it never earned.

    THE BAR IS ONE CODE VALUE ANYWHERE, and a scene past it is a defect to
    file against the promoter, never a plate to rebase — there is no
    baseline here to rebase into. Both halves are kept, so a scene reported
    MOVED can be opened beside the plate it was meant to match."""

    print("[promotion off]")
    _, off_errors = sweep(
        binary, scenes, off_dir, timeout, jobs, PROMOTION_OFF, lambda s, d: "rendered"
    )
    print("[promotion on]")
    _, on_errors = sweep(
        binary, scenes, on_dir, timeout, jobs, PROMOTION_ON, lambda s, d: "rendered"
    )
    errors = len(off_errors) + len(on_errors)

    verdict = 0
    within = 0
    print()
    distances, unusable = compared(binary, off_dir, on_dir)
    for scene in scenes:
        if scene in unusable:
            print(f"  {unusable[scene].upper()} {scene}   <-- FINDING")
            verdict = 1
            continue
        if scene not in distances:
            verdict = 1
            continue
        mean, p99, worst = distances[scene]
        if worst <= PROMOTION_DRIFT_CEILING:
            within += 1
            # A scene the promoter never fired on differs in nothing at
            # all, and one it did fire on differs by a code value on the
            # shaded pixels. Both are within the rule; the count of
            # differing pixels is what tells them apart, so max is
            # printed for every scene rather than only for the movers.
            print(f"  WITHIN {scene:<24} max {worst:3d}  mean {mean:6.2f}")
            continue
        print(
            f"  MOVED  {scene:<24} max {worst:3d} (> "
            f"{PROMOTION_DRIFT_CEILING})  mean {mean:6.2f}  p99 {p99:4d}"
            f"   <-- FINDING"
        )
        verdict = 1
    print(f"\n{within} of {len(scenes)} within one code value, {errors} failed")
    print(f"plates kept: {off_dir}\n             {on_dir}")
    if verdict == 0 and not errors:
        print("VERDICT: the promoter moves no picture by more than one code value")
    return verdict or (1 if errors else 0)


def main(argv: list) -> int:
    ap = argparse.ArgumentParser(
        prog="sigil.py plates",
        description="plate sweep over the sketch registry, judged against a "
        "machine-local baseline manifest",
    )
    ap.add_argument("--config", default="Release", choices=tree.CONFIGURATIONS)
    ap.add_argument("--jobs", type=int, default=max(2, (os.cpu_count() or 8) // 2))
    ap.add_argument(
        "--tier",
        choices=("cpu", "device", "promotion"),
        default="cpu",
        help="cpu (default): CPU renders to each scene's declared capture "
        "moment, judged on byte identity against the baseline manifest. "
        "device: the same scenes on the GPU, judged per colour channel "
        "against the CPU plate of the same run; no baseline. promotion: "
        "the same scenes rendered with automatic texture promotion held "
        "off and again with every promotable node eagerly baked, judged "
        "within one code value; no baseline",
    )
    ap.add_argument(
        "--kind",
        choices=KINDS,
        help="only the sketches drawn through this runtime (default: both)",
    )
    ap.add_argument("--scenes", nargs="*", help="subset (registry names)")
    ap.add_argument("--sketch", metavar="NAME", help="one scene (registry name)")
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="write the manifest from this sweep. A sweep narrowed by --kind, "
        "--sketch or --scenes merges, so only an unnarrowed rebase rewrites "
        "the file wholesale",
    )
    ap.add_argument(
        "--stability",
        type=int,
        default=0,
        metavar="N",
        help="re-render each mover N more times; a scene that "
        "disagrees with ITSELF is attributed to the scene. This is "
        "the ONLY way a mover is excused — there is no list",
    )
    ap.add_argument(
        "--timeout-seconds",
        type=float,
        default=300,
        metavar="S",
        help="per-scene render ceiling, in seconds (default 300). "
        "A scene still running at the ceiling is killed and "
        "reported FAILED-TIMEOUT by name while the rest of "
        "the sweep continues — one runaway scene must not "
        "hang the verdict that protects everything else",
    )
    args = ap.parse_args(argv)

    binary = str(tree.sketchbook(args.config))
    manifest = str(tree.build_dir() / f"plate_baseline_{args.config}.sha256")

    kinds = (args.kind,) if args.kind else KINDS
    listed, unavailable = registry(binary, kinds)
    chosen = list(args.scenes or [])
    if args.sketch:
        chosen.append(args.sketch)
    unknown = [
        scene for scene in chosen if scene not in listed and scene not in unavailable
    ]
    if unknown:
        sys.exit(f"not in the registry: {', '.join(unknown)}")
    # A scene this machine cannot render is reported and stood down —
    # including one named explicitly on the command line, because asking
    # for it by name does not install anything.
    skipped = {
        scene: why
        for scene, why in unavailable.items()
        if not chosen or scene in chosen
    }
    scenes = {s: k for s, k in listed.items() if not chosen or s in chosen}
    for scene, why in sorted(skipped.items()):
        print(f"SKIPPED {scene}: {why}")
    narrowed = bool(chosen or args.kind)

    if args.tier == "device":
        if args.rebase:
            sys.exit(
                "--tier device has no baseline to rebase: it is judged against "
                "the CPU plates the same sweep renders. Change GPU_TOLERANCE to "
                "move what it accepts."
            )
        print(
            f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, "
            f"tier device: each rendered on the CPU and on the device and "
            f"compared per colour channel"
        )
        return device_sweep(
            binary,
            list(scenes),
            args.timeout_seconds,
            args.jobs,
            plate_dir(args.config, "device", "cpu"),
            plate_dir(args.config, "device", "gpu"),
        )

    if args.tier == "promotion":
        if args.rebase:
            sys.exit(
                "--tier promotion has no baseline to rebase: it is judged "
                "against the same scenes rendered with the promoter held off, "
                "in the same run. A scene past the ceiling is a defect in the "
                "promoter, not a plate to adopt."
            )
        print(
            f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, "
            f"tier promotion: each rendered with automatic texture promotion "
            f"held off and again with every promotable node eagerly baked"
        )
        return promotion_sweep(
            binary,
            list(scenes),
            args.timeout_seconds,
            args.jobs,
            plate_dir(args.config, "promotion", "off"),
            plate_dir(args.config, "promotion", "on"),
        )

    print(f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, tier cpu")

    # Read BEFORE the sweep so a scene can be judged the moment it lands.
    standing_manifest = baseline.read_manifest(manifest)
    adopting = args.rebase or not os.path.exists(manifest)

    def standing(scene, digest):
        if args.rebase or scene not in standing_manifest:
            return "rendered"
        return "identical" if standing_manifest[scene] == digest else "hash miss"

    # An adopting sweep IS the baseline, so it renders straight into the
    # kept baseline directory and the manifest is written from the same
    # plates. A judging sweep renders beside it, which leaves the two
    # directories `--compare` differences standing when it is over.
    kept_baseline = plate_dir(args.config, "baseline", fresh=False)
    outdir = kept_baseline if adopting else plate_dir(args.config, "cpu")
    results, errors = sweep(
        binary,
        list(scenes),
        outdir,
        args.timeout_seconds,
        args.jobs,
        PROMOTION_OFF,
        standing,
    )

    if adopting:
        if not args.rebase:
            print(
                f"no manifest at {manifest} — writing one (this sweep "
                f"becomes the baseline)"
            )
        # A narrowed rebase merges into the existing manifest rather than
        # truncating it to the subset. A rebase that skipped scenes merges
        # for the same reason narrowed to those: this machine could not
        # ask them anything, so it has nothing to say about their
        # baselines either.
        keep = True if narrowed else (set(skipped) if skipped else None)
        merged = baseline.write_manifest(manifest, keep, results)
        prune_plates(kept_baseline, merged)
        print(
            f"baseline written: {manifest} ({len(merged)} scenes, "
            f"{len(results)} from this sweep)"
        )
        print(f"baseline plates: {kept_baseline}")
        verdict = 0
    else:
        movers, missing = [], []
        for scene, digest in sorted(results.items()):
            if scene not in standing_manifest:
                missing.append(scene)
            elif standing_manifest[scene] != digest:
                movers.append(scene)
        identical = len(results) - len(movers) - len(missing)
        print(
            f"\n{identical} byte-identical, {len(movers)} with a moved hash, "
            f"{len(missing)} not in baseline, {len(errors)} failed"
        )

        verdict = 0
        for scene in movers:
            if args.stability > 0:
                rerenders = {results[scene]}
                for _ in range(args.stability):
                    _, digest, _, _ = render_scene(
                        binary,
                        scene,
                        plate_dir(args.config, "stability", fresh=False),
                        args.timeout_seconds,
                    )
                    if digest:
                        rerenders.add(digest)
                if len(rerenders) > 1:
                    print(
                        f"  MOVED (self-unstable) {scene} — disagrees with "
                        f"itself across {args.stability + 1} renders; "
                        f"attribute to the scene, not to the change"
                    )
                    continue
            print(
                f"  MOVED  {scene}  {standing_manifest[scene][:12]} -> "
                f"{results[scene][:12]}   <-- FINDING\n"
                f"           was {plate_path(kept_baseline, scene)}\n"
                f"           now {plate_path(outdir, scene)}"
            )
            verdict = 1
        for scene in missing:
            print(f"  NEW    {scene} (not in baseline — rebase to adopt)")
        # A hash says a scene moved and nothing about where. The two
        # directories behind the two hashes are both still on disk, so the
        # next question has a command rather than a re-render.
        print(f"\nplates kept: {outdir}")
        if verdict:
            print(f"  {binary} --compare {kept_baseline} {outdir}")
        if verdict == 0 and not errors:
            print("VERDICT: byte-neutral")

    return verdict or (1 if errors else 0)
