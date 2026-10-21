"""Verb: plates — parallel plate sweeps over the sketch registry.

    sigil.py plates --rebase           # adopt the device manifest, and
                                       # the library plate cases
    sigil.py plates                    # the device reference: sweep +
                                       # hash + verdict, and the library
                                       # plate cases judged
    sigil.py plates --kind scene3d     # only the scene3d sketches
    sigil.py plates --sketch astral_tome   # by file stem or filed name
    sigil.py plates --scenes "aero desktop" black_watch
    sigil.py plates --stability 3      # re-render movers 3x to separate
                                       # flappers from code
    sigil.py plates --tier cpu         # the raster smoke: scene2d on the
                                       # CPU, read against the device plates
    sigil.py plates compare <a> <b>    # two plate directories, differenced

Renders every sketch through `Grimoire --headless --ledger` (the
benchmark-free exact-stepped capture), N at a time. One binary renders
both tiers; what separates them is which rasteriser a sketch draws
through. THE DEVICE IS
THE REFERENCE: the device tier (the default) renders every scene2d and
scene3d sketch on the device and holds each to one machine-local
manifest of device plates on byte identity. The CPU tier is a smoke run
of the raster path: it renders every scene2d sketch on the CPU, reads
each against its device plate per colour channel, and fails only for a
sketch that did not render, because the raster and the device need not
agree.

What each tier judges, what it refuses and why each ceiling stands where
it does is scripts/README.md; this refuses to keep a second copy of it.
The judgement is here because a ceiling is a tolerance about a machine
and not a fact about two files: the manifest, the tolerances and
--stability are what this owns. Decoding and
differencing plates is the sketch library's, read here as values through
the Python extension the build links.

A library's own plate cases are ctest cases labelled `plates`, each a
picture held against a baseline committed beside its test; an unnarrowed
device sweep runs them after the sketches, and `--rebase` adopts them the
way it adopts the manifest, by the one environment variable those cases
read.
"""

import argparse
import concurrent.futures
import contextlib
import dataclasses
import json
import os
import shutil
import subprocess
import sys
import time

from sigil import baseline, tree

# One prefix names every plate.
PLATE_PREFIX = "plate_"
KINDS = ("scene2d", "scene3d")

# The flag every render carries: the benchmark-free exact-stepped capture.
RENDER_ARGS = ("--ledger",)

# The ctest label every library plate case carries, and the variable that
# turns such a case from judging its render into adopting it.
LIBRARY_PLATES_LABEL = "plates"
LIBRARY_PLATES_REBASE = "SIGIL_PLATES_REBASE"

# How far a SCENE2D sketch's CPU plate reads from its device plate before
# the raster smoke calls the pair OVER: (mean, p99) per colour channel in
# 0..255. Set from what the two rasterisers actually do. It is a reading
# and never a verdict — the raster and the device need not agree — so a
# row past it is reported and fails nothing.
DEFAULT_RASTER_TOLERANCE = (12.0, 128)
RASTER_TOLERANCE = {}

# The kind that is drawn on the device and nowhere else.
SCENE3D_KIND = "scene3d"

# The device manifest, the one reference every sketch is judged against.
DEVICE_MANIFEST = "plate_baseline_{config}_device.sha256"


def registry(binary, kinds, device=False):
    """What the binary carries for the given kinds, as a Listing. @p device
    asks the binary to bring its device up first, without which every
    scene3d sketch is unavailable; None when that was asked and the machine
    has no device to bring up.

    Read as values — the catalog's rows, one JSON object each, which carry
    every entry's filed name, file stem, runtime and availability. The registry is
    compiled into Grimoire and into nothing a script can import, so its
    rows cross the process as data. A sketch this machine cannot run is a
    row like any other, with the reason: a sketch dropped from the rows
    and a sketch deleted from the tree would read exactly alike, and the
    difference is the whole point."""
    command = [binary, "--catalog"] + (["--gpu"] if device else [])
    result = tree.capture(command)
    if (
        device
        and result.returncode != 0
        and "no device runtime" in (result.stderr + result.stdout)
    ):
        return None
    result.check_returncode()
    return catalog_listing(result.stdout, kinds)


@dataclasses.dataclass
class Listing:
    """The registry as one tier sees it. `scenes` is filed name -> kind for
    what this machine can render, `unavailable` filed name -> (kind,
    reason) for what it cannot, and `stems` file stem -> filed name for
    both, because a sketch answers to either spelling."""

    scenes: dict
    unavailable: dict
    stems: dict


def catalog_listing(printed: str, kinds) -> Listing:
    """The rows `Grimoire --catalog` printed, kept to @p kinds."""
    listing = Listing({}, {}, {})
    for line in printed.splitlines():
        # A row is one JSON object per line; a device brought up for the
        # listing logs its own lines beside them, which are not rows.
        if not line.startswith("{"):
            continue
        row = json.loads(line)
        if row["kind"] not in kinds:
            continue
        name = row["filedName"]
        if row["available"]:
            listing.scenes[name] = row["kind"]
        else:
            listing.unavailable[name] = (row["kind"], row["reason"])
        if row.get("key"):
            listing.stems[row["key"]] = name
    return listing


def resolve(queries, listing: Listing):
    """The filed names @p queries select, and the queries that select
    nothing. A query is a filed name or a file stem, matched exactly — the
    two spellings Grimoire's own `--sketch` answers to — and a filed name
    wins where one sketch's stem is another's filed name."""
    chosen, unknown = [], []
    for query in queries:
        if query in listing.scenes or query in listing.unavailable:
            chosen.append(query)
        elif query in listing.stems:
            chosen.append(listing.stems[query])
        else:
            unknown.append(query)
    return chosen, unknown


def render_scene(binary, scene, outdir, timeout, extra_args=()):
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
        return (
            scene,
            None,
            (result.stderr or result.stdout).strip()[-300:],
            elapsed,
        )
    return scene, baseline.digest(plate), None, elapsed


def compared(first, second):
    """Every plate in both directories, differenced: name -> its row, for
    the plates that were compared, name -> the row's line for the ones
    that were missing, unreadable or resized, and the refusal of a
    comparison that could not start at all, empty when it started.

    A row carries the mean, p99 and worst channel distance, the worst split
    by what the pixel stands on — transparent black in `first`
    (`worstOverClear`), an edge both plates draw (`worstOverGraze`, over
    `grazingPixels`) or anything else (`worstOverContent`). Decoding and differencing
    is the library's; what stays here is the judgement — which distance is
    close enough on this machine — because that is a tolerance and not a
    fact about two files."""
    sketch = tree.extension("sketch.compare", "sketch.PlateOutcome").sketch
    comparison = sketch.compare(first, second)
    distances, unusable = {}, {}
    for plate in comparison.plates:
        if plate.outcome == sketch.PlateOutcome.Compared:
            distances[plate.name] = plate
        else:
            unusable[plate.name] = _unusable(plate)
    return distances, unusable, comparison.refusal


def _unusable(plate):
    """What a plate that could not be measured says about itself: its
    line, without the name, which can carry spaces."""
    line = str(plate)
    verb = line.split(" ", 1)[0]
    return verb + line.removeprefix(f"{verb} {plate.name}")


def compare_main(argv: list) -> int:
    """`plates compare A B`: every plate of two directories as the line its
    row spells, and the status the comparison answers — 0 when every plate
    was compared, 1 when any was missing, unreadable or resized, 2 when
    there was nothing to compare."""
    ap = argparse.ArgumentParser(
        prog="sigil.py plates compare",
        description="difference two directories of plates, one line per plate; "
        "the first directory is the reference",
    )
    ap.add_argument("first", help="the reference plate directory")
    ap.add_argument("second", help="the plate directory compared against it")
    args = ap.parse_args(argv)
    sketch = tree.extension("sketch.compare").sketch
    comparison = sketch.compare(args.first, args.second)
    if comparison.refusal:
        print(comparison.refusal, file=sys.stderr)
    for plate in comparison.plates:
        print(plate)
    return comparison.status()


def plate_dir(config, *parts, fresh=True, replacing=None):
    """A KEPT plate directory under build/, beside the manifest.

    A verdict of MOVED is a hash disagreeing with a hash, which says
    nothing about WHERE the picture moved. The two plates behind the two
    hashes answer that, so they are written where they can still be
    opened and differenced after the run rather than into a directory
    removed at exit. build/ is ignored, so nothing here is ever committed.

    @p fresh empties the directory first, which is what a whole sweep's
    own output wants: every plate in it is then one this run rendered.
    @p replacing, given instead, names the scenes a narrowed sweep is
    about to render and removes only their plates, so a run narrowed to
    two scenes keeps the plates another narrowed run left beside them,
    and a scene that fails to render leaves no plate of an earlier run
    standing under its name. The baseline directory is neither: it is
    overwritten scene by scene as a rebase adopts them, and pruned to the
    manifest afterwards."""
    directory = tree.build_dir() / f"plates_{config}"
    for part in parts:
        directory = directory / part
    if replacing is not None:
        for scene in replacing:
            with contextlib.suppress(FileNotFoundError):
                os.remove(plate_path(directory, scene))
    elif fresh:
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


def raster_smoke(binary, scenes, timeout, jobs, raster_dir, device_dir):
    """The CPU tier: every scene2d sketch rendered on the CPU and read
    against its device plate, the one the device tier last adopted.

    It is a SMOKE RUN of the raster path and keeps no baseline. The device
    is the reference and the raster and the device need not agree, so a
    row past the tolerance is reported and fails nothing; the run fails
    only for a sketch that did not render. The CPU plates are kept beside
    the device plates so a row can be opened rather than only measured."""
    print("[scene2d on the cpu]")
    _, errors = sweep(
        binary,
        scenes,
        raster_dir,
        timeout,
        jobs,
        (),
        lambda s, d: "rendered",
    )
    print()
    distances, unusable, refusal = compared(device_dir, raster_dir)
    status = raster_smoke_report(scenes, errors, distances, unusable, refusal)
    print(f"plates kept: {raster_dir}\n  read against {device_dir}")
    return status


def raster_smoke_report(scenes, errors, distances, unusable, refusal):
    """The raster smoke's rows and its exit status: one row per scene that
    rendered — WITHIN or OVER the tolerance of its device plate, or what
    kept the pair from being read — and 1 only when a scene failed to
    render. @p distances and @p unusable are the comparison's answer with
    the device plates as the reference, and @p refusal is its refusal of
    the whole pair of directories, which says there is no device plate to
    read against at all."""
    within = over = unread = 0
    if refusal:
        print(f"  {refusal}: no device plate to read against")
    for scene in scenes:
        if scene in errors:
            continue
        if scene in unusable:
            print(f"  {unusable[scene].upper()} {scene}")
            unread += 1
            continue
        if scene not in distances:
            unread += 1
            continue
        row = distances[scene]
        mean_cap, p99_cap = RASTER_TOLERANCE.get(scene, DEFAULT_RASTER_TOLERANCE)
        apart = row.mean > mean_cap or row.p99 > p99_cap
        over += apart
        within += not apart
        print(
            f"  {'OVER ' if apart else 'WITHIN'} {scene:<24} "
            f"mean {row.mean:6.2f} (<= {mean_cap:g})  "
            f"p99 {row.p99:4d} (<= {p99_cap})  max {row.worst:3d}"
        )
    print(
        f"\n{within} within tolerance of the device plate, {over} over, "
        f"{unread} not read against one, {len(errors)} failed to render"
    )
    if errors:
        return 1
    print("VERDICT: every scene2d sketch rendered on the CPU")
    return 0


def hashed_sweep(
    binary,
    scenes,
    args,
    *,
    manifest,
    kept_baseline,
    judged,
    stability,
    extra_args,
    narrowed,
    skipped,
):
    """Renders @p scenes, hashes each plate and judges it against
    @p manifest — or, adopting, writes the manifest from them. The device
    tier asks this of every sketch, scene2d and scene3d alike: on one
    machine and one driver the same declaration renders the same bytes,
    which is what makes a hash the honest verdict. @p judged and @p stability make the directories a judging
    sweep and a stability re-render write into, made only when used.
    Answers the verdict: 0 when nothing moved and nothing failed."""
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
    # directories `plates compare` differences standing when it is over.
    outdir = kept_baseline if adopting else judged()
    results, errors = sweep(
        binary,
        scenes,
        outdir,
        args.timeout_seconds,
        args.jobs,
        extra_args,
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
        return 1 if errors else 0

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
                    stability(),
                    args.timeout_seconds,
                    extra_args,
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
        print(f"  python3 scripts/sigil.py plates compare {kept_baseline} {outdir}")
    if verdict == 0 and not errors:
        print("VERDICT: byte-neutral")
    return verdict or (1 if errors else 0)


def library_plates(config: str, rebase: bool) -> int:
    """The libraries' own plate cases, judged — or, rebasing, adopted into
    the baselines committed beside their tests, which the caller then
    commits with the cause. Answers ctest's exit status."""
    environment = dict(os.environ)
    environment.pop(LIBRARY_PLATES_REBASE, None)
    if rebase:
        environment[LIBRARY_PLATES_REBASE] = "1"
    command = [
        "ctest",
        "--test-dir",
        str(tree.build_dir()),
        "-C",
        config,
        "-L",
        LIBRARY_PLATES_LABEL,
        "--output-on-failure",
    ]
    verb = "adopting" if rebase else "judging"
    print(f"\nlibrary plates, {verb}: {' '.join(command)}", flush=True)
    status = subprocess.run(command, env=environment).returncode
    if rebase and status == 0:
        print(
            "library plates adopted: commit the baselines under each "
            "library's test/plates/ with the cause"
        )
    return status


def main(argv: list) -> int:
    if argv[:1] == ["compare"]:
        return compare_main(argv[1:])
    ap = argparse.ArgumentParser(
        prog="sigil.py plates",
        description="plate sweep over the sketch registry, judged against a "
        "machine-local manifest of device plates",
    )
    ap.add_argument("--config", default="Release", choices=tree.CONFIGURATIONS)
    ap.add_argument("--jobs", type=int, default=max(2, (os.cpu_count() or 8) // 2))
    ap.add_argument(
        "--tier",
        choices=("device", "cpu"),
        default="device",
        help="device (default), the reference: every scene2d and scene3d "
        "sketch rendered on the device to its declared capture moment and "
        "judged on byte identity against the device manifest, which "
        "--rebase adopts. "
        "cpu, the raster smoke: every scene2d sketch rendered on the CPU and "
        "read per colour channel against its device plate; the rows are "
        "reported and only a sketch that fails to render fails the run; no "
        "baseline",
    )
    ap.add_argument(
        "--kind",
        choices=KINDS,
        help="only the sketches drawn through this runtime (default: both)",
    )
    ap.add_argument(
        "--scenes", nargs="*", help="subset, each a filed name or a file stem"
    )
    ap.add_argument(
        "--sketch", metavar="NAME", help="one scene, by filed name or file stem"
    )
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="write the device manifest from this sweep. A sweep narrowed by "
        "--kind, --sketch or --scenes merges, so only an unnarrowed rebase "
        "rewrites the file wholesale, and only an unnarrowed one also adopts "
        "the library plate cases (ctest label plates) into the baselines "
        "committed beside their tests",
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
    if args.rebase and args.tier != "device":
        sys.exit(
            f"--tier {args.tier} has no baseline to rebase: the device tier "
            f"is the reference, and this tier reads its plates against "
            f"plates rendered in the same run or against the device's"
        )

    binary = str(tree.grimoire(args.config))
    kinds = (args.kind,) if args.kind else KINDS
    queries = list(args.scenes or []) + ([args.sketch] if args.sketch else [])
    narrowed = bool(queries or args.kind)
    on_device = args.tier == "device"

    # A SCENE3D SKETCH IS AVAILABLE ONLY WHERE A DEVICE IS, so the device
    # tier asks the binary to bring its device up before it lists anything;
    # the CPU tier lists without one and passes every scene3d sketch over.
    listing = registry(binary, kinds, device=on_device)
    if listing is None:
        print("SKIPPED: this machine has no device to render on")
        # The libraries' own plates are rendered by their harnesses and
        # need no device, so they are still judged.
        return library_plates(args.config, args.rebase) if not narrowed else 0
    chosen, unknown = resolve(queries, listing)
    if unknown:
        sys.exit(f"not in the registry: {', '.join(unknown)}")
    listed, unavailable = listing.scenes, listing.unavailable

    def selected(scene):
        return not chosen or scene in chosen

    # Off the device every scene3d sketch is passed over, counted rather
    # than listed one by one: it has no CPU plate, and its picture is the
    # device tier's to judge.
    passed_over = []
    if not on_device:
        passed_over = sorted(
            [s for s, k in listed.items() if k == SCENE3D_KIND and selected(s)]
            + [
                s
                for s, (k, _) in unavailable.items()
                if k == SCENE3D_KIND and selected(s)
            ]
        )
        listed = {s: k for s, k in listed.items() if k != SCENE3D_KIND}
        unavailable = {s: v for s, v in unavailable.items() if v[0] != SCENE3D_KIND}
    # A scene this machine cannot render is reported and stood down —
    # including one named explicitly on the command line, because asking
    # for it by name does not install anything.
    skipped = {scene: why for scene, (_, why) in unavailable.items() if selected(scene)}
    scenes = [s for s in listed if selected(s)]
    for scene, why in sorted(skipped.items()):
        print(f"SKIPPED {scene}: {why}")
    if passed_over:
        print(
            f"PASSED OVER {len(passed_over)} scene3d sketches: a scene3d sketch "
            f"is drawn on the device and nowhere else"
        )

    # The device plates the manifest was adopted from: what the device tier
    # judges beside and what the raster smoke reads against.
    device_baseline = plate_dir(args.config, "device", "baseline", fresh=False)

    if args.tier == "cpu":
        print(
            f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, "
            f"tier cpu: the raster smoke, each scene2d sketch rendered on the "
            f"CPU and read against its device plate"
        )
        return raster_smoke(
            binary,
            scenes,
            args.timeout_seconds,
            args.jobs,
            plate_dir(args.config, "cpu", replacing=scenes if narrowed else None),
            device_baseline,
        )

    print(
        f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, "
        f"tier device: every sketch rendered on the device and hashed against "
        f"the device manifest"
    )
    verdict = hashed_sweep(
        binary,
        scenes,
        args,
        manifest=str(tree.build_dir() / DEVICE_MANIFEST.format(config=args.config)),
        kept_baseline=device_baseline,
        judged=lambda: plate_dir(
            args.config, "device", "gpu", replacing=scenes if narrowed else None
        ),
        stability=lambda: plate_dir(args.config, "device", "stability", fresh=False),
        extra_args=("--gpu",),
        narrowed=narrowed,
        skipped=skipped,
    )

    # The libraries' own plates answer for the whole tree or not at all, so
    # a sweep narrowed to some sketches leaves them standing.
    if not narrowed and library_plates(args.config, args.rebase) != 0:
        verdict = verdict or 1
    return verdict
