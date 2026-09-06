#!/usr/bin/env python3
"""The plate ledger, as ONE command: parallel plate sweeps.

Renders every sketch through `Sketchbook --headless --ledger` (the
benchmark-free exact-stepped capture), N at a time, hashes the plates,
and compares against a stored baseline manifest. One binary renders
both tiers; what separates them is which rasteriser a sketch draws
through.

Thumbnails are not this script's to write. Sketchbook owns the stills it
shows — it renders them on demand into its own cache and warms them with
`Sketchbook --thumbnails` — so the ledger renders plates for the verdict
and nothing else.

Usage (from apps/spell-circle-canvas):
  scripts/plate_ledger.py --rebase           # bake the baseline manifest
  scripts/plate_ledger.py                    # sweep + compare + verdict
  scripts/plate_ledger.py --kind set         # only the sketches that light a set
  scripts/plate_ledger.py --sketch astral_tome
  scripts/plate_ledger.py --scenes "aero desktop" black_watch
  scripts/plate_ledger.py --stability 3      # re-render movers 3x to
                                             # separate flappers from code
  scripts/plate_ledger.py --tier device      # the same sketches on the GPU
  scripts/plate_ledger.py --tier promotion   # …and with the promoter let go
  scripts/plate_ledger.py --jobs 6 --config Release

THREE TIERS (--tier):

  cpu (default) — every sketch stepped from t=0 to its DECLARED capture
  moment and rasterised on the CPU: a canvas sketch through Skia's CPU
  backend, a set through SigilGeometry's CPU mesh executor. A plate is
  then a function of the declaration alone, so BYTE IDENTITY IS THE BAR:
  a clean sweep is the byte-neutrality verdict, against ONE manifest,
  build/plate_baseline_<config>.sha256, keyed by registry name and
  covering both kinds. `--rebase` adopts. A sweep narrowed by --kind,
  --sketch or --scenes merges into the manifest rather than truncating
  it: adopting one deliberately changed plate must not discard the
  baseline for every scene the sweep did not render. Only an unnarrowed
  sweep writes the file wholesale, which is what drops a scene that no
  longer exists.

  device — the same sketches rendered through the device (--gpu) and
  judged against the CPU plate of the same run, per colour channel
  within stated ceilings. It has no baseline: both plates are made in
  this run, so nothing is kept between runs and --rebase is refused.

  WHY A DISTANCE AND NOT A HASH. A device plate is not a function of
  the drawing code alone. For a set the two tiers are two rasterisers:
  the host paints shaded vertices through a per-triangle sort with
  Skia's antialiasing; the device rasterises the same shading through a
  depth buffer with none. They agree about what the scene is and they
  differ along every edge, and a post pass's blur is a box approximation
  on one side and a separable Gaussian on the other. Asking for equal
  bytes would fail on the first pixel and tell no one anything.

  WHAT IS MEASURED, per colour channel in 0..255 over every pixel:

    mean   the average absolute difference. This is the number that says
           the two pictures ARE the same picture — a scene drawn wrong
           on one tier moves it immediately.
    p99    the value 99 channels in a hundred stay under, which says the
           disagreement is CONFINED rather than spread.
    max    the worst channel anywhere. It is an edge, or a body a
           centroid sort ranked wrongly on the host and a depth buffer
           ranked rightly on the device, and it is reported rather than
           judged.

  A sketch names its own mean and p99 ceilings in GPU_TOLERANCE below,
  set from what the two tiers actually do rather than from a wish. A
  sketch with no entry there is judged by DEFAULT_GPU_TOLERANCE.

  It SKIPS cleanly with no device: `--gpu` reports that it found none,
  the tier says so and exits 0, because a machine with no device runtime
  has nothing to disagree about.

  promotion — the same scenes rendered twice on the CPU, once with
  automatic texture promotion held off and once with it on, judged
  against each other. It has no baseline either: the held-off render of
  the same run is the reference.

  THE BAR IS ONE CODE VALUE, and it is not a tolerance anyone chose. A
  promoted node is baked under the live matrix post-translated by an
  integer; inverting that matrix to find a shader's local coordinates
  does not cancel the integer to the last bit at a scale whose
  reciprocal is inexact, so a shaded pixel can land one code value from
  the live paint and nothing may land further. A worst channel over 1
  is a picture that MOVED — a bake somewhere else, rasterised against
  another clip, or gone stale — and it is a defect to file against the
  promoter rather than a plate to adopt.

  IT EXISTS BECAUSE NOTHING ELSE EXERCISES THE PROMOTER. A headless
  session is opened deterministic and a deterministic session holds
  promotion off, so every other tier renders the one renderer feature
  with the feature switched out.

  IT PROMOTES EAGERLY, so the tier tests THE SAME NODE SET ON EVERY
  MACHINE. The runtime's own promotion rule is a stopwatch — a node is
  baked once its paint has measured over a millisecond for eight
  consecutive frames — so left to itself this tier reports whatever the
  machine's load happened to promote, which on an idle one is nothing at
  all. `--promotion` asks the runtime to bake every node its rules admit,
  from the first frame, whatever the node costs. Nothing about what a
  bake may do changes; the numbers below are therefore a measurement of
  the scene rather than a floor on it, and they reproduce.

The manifest lives in build/ (machine-local on purpose: plates are
AA-deterministic per machine, not across machines), so a fresh checkout
runs `--rebase` once before a sweep can judge anything.

THE PLATES THEMSELVES ARE KEPT, beside the manifest, under
build/plates_<config>/ — one directory per tier, one PNG per scene,
overwritten rather than accumulated, and never committed because build/
is ignored. `baseline/` holds what the manifest was baked from and is
overwritten on rebase; `cpu/` holds what the last judging sweep
rendered; the two comparing tiers keep both of their halves
(`device/cpu`, `device/gpu`, `promotion/off`, `promotion/on`). A hash
says a scene MOVED and stops there, so the two plates behind the two
hashes have to survive the run for anyone to see WHERE it moved:
`Sketchbook --compare build/plates_<config>/baseline
build/plates_<config>/cpu` differences them channel by channel, and the
verdict prints that line under the movers together with each mover's two
files. Every render a
hash judges carries --no-promotion: automatic texture promotion re-bakes
by a measured per-frame cost, which load can tip either way, so it is
the one renderer feature a byte-identity gate must hold off — with it
off, hashes are load-immune. The promotion tier is the one that turns it
back on, and it judges by distance for exactly that reason.

EVERY SCENE PRINTS ONE LINE AS IT FINISHES — its running count, how it
stands against the baseline, its name and what it took — in COMPLETION
order, so the scene the sweep is still waiting on is the one that has not
printed yet. The summary and the VERDICT below them are the report; the
per-scene lines are the sweep saying what it is doing while it does it.

Every scene render runs under a per-scene ceiling (--timeout-seconds,
default 300 s). A scene still running at the ceiling is killed and
reported FAILED-TIMEOUT by name while the rest of the sweep continues:
one runaway scene must not hang the verdict that protects everything
else. There is no per-scene override: a scene over the budget fails by
name, because an exception would assert that one scene's cost cannot be
reduced, which a declared cache and an earlier settled capture moment
almost always disprove.

A SKETCH THIS MACHINE CANNOT RENDER IS SKIPPED BY NAME. A sketch written
over an optional SDK is only compiled in where that SDK was found, and
the data the SDK needs at run time — a resource folder, the SDK's own
sample archives — can still be absent on the machine running the binary.
The registry answers for that rather than the sweep guessing: `--list`
marks such a sketch with what it is missing, both tiers print SKIPPED
and the reason, and no plate is rendered, hashed or judged. A skip is
not a failure and not a mover.

SO THE PLATES FOR THOSE SCENES EXIST ONLY WHERE THE SDK DOES. A baseline
holding one was rebased on a machine that had the SDK; a machine without
it skips the scene rather than reporting a plate it is missing. A rebase
that could not ask a scene anything keeps the baseline line already
there instead of discarding it, so running --rebase on the smaller
machine does not delete what the larger one recorded.

THERE IS NO LIST OF SCENES ALLOWED TO MOVE. Every mover is a finding
until it is shown to be one, and the showing is `--stability N`: a scene
that disagrees with ITSELF across N+1 renders is attributed to the scene
rather than to the change under test. A list would have to be believed;
this is measured on the machine in front of you, every time.

A sketch that draws a number it measured about its own execution — a
build time, a bake cost, a live node count — would be a scene like that
by construction, so the renderer pins those: a headless session is opened
with `ctx.deterministic` set, and `ctx.measured(value, pinned)` returns
the pinned number. A sketch that reads a clock and does not go through
`measured()` is the one thing `--stability` still has to catch.
"""

import argparse
import concurrent.futures
import fcntl
import hashlib
import os
import shutil
import subprocess
import sys
import time

# One binary renders both tiers, and one prefix names every plate.
BINARY = "Sketchbook.app/Contents/MacOS/Sketchbook"
PLATE_PREFIX = "plate_"
KINDS = ("canvas", "set")

# The flag every render carries: the benchmark-free exact-stepped capture.
RENDER_ARGS = ("--ledger",)
# …and what each tier says about the promoter. Held off wherever a hash is
# the verdict, because cost-based re-baking decides by a measured per-frame
# cost that load tips either way; turned on for the tier whose whole subject
# it is.
PROMOTION_OFF = ("--no-promotion",)
PROMOTION_ON = ("--promotion",)

# HOW FAR A SKETCH'S DEVICE PLATE MAY STAND FROM ITS CPU PLATE: (mean,
# p99) per colour channel in 0..255. Set from what the two tiers actually
# do, and tightened when one of them gets closer to the other rather than
# loosened when a change moves them apart.
#
# first_light is the looser of the two, for two reasons its picture makes
# unusually large. A comet of twelve hundred stamped beads is nothing but
# silhouettes, and the host antialiases those edges where the device does
# not. And a broad ground plate is FOUR vertices wide: the host clamps
# each shaded vertex to a byte and interpolates the bytes, the device
# interpolates the shading and clamps per pixel, and across a quad that
# large the two readings drift mildly apart everywhere at once.
# glow_trail's picture has neither, and the two tiers stand a per-channel
# unit or two apart over almost all of it — its worst channel is where a
# centroid sort puts a far post behind the plate on the host and the
# depth buffer puts it in front on the device, which is the host being
# wrong rather than the device.
DEFAULT_GPU_TOLERANCE = (12.0, 128)
GPU_TOLERANCE = {
    "first_light": (10.0, 96),
    "glow_trail": (4.0, 32),
    # material_lab is the loosest entry here, and it is the one study
    # whose two tiers are MEANT to disagree. Its five cards are chosen
    # because the device shades them — a stack composed through a mask, a
    # normal map, a packed roughness-and-metallic map, an emission — and
    # the CPU tier can read a base colour and a base-colour map and
    # nothing else, so on four of the five the two pictures are simply
    # different pictures. That is what puts the p99 where it is: at the
    # 99th channel the disagreement is the study's whole subject. The
    # mean is still the number that says a card landed where it belongs,
    # and it is held near what the two tiers actually produce. On top of
    # that the study carries the drift every 3D scene here has: a broad
    # ground plane, where the two tiers' vertex-versus-pixel clamping
    # parts company (see first_light above), wearing a check repeated
    # five times across itself and seen nearly edge on, which the two
    # tiers minify differently everywhere at once.
    "material_lab": (10.0, 192),
    # A still set under a ramping key is nearly all interior: the two
    # tiers agree to a channel or two everywhere but the silhouettes.
    "key_light": (3.0, 32),
    # A swept rail, a few gates and a dart on it are almost entirely
    # smooth interior over an empty background, which is where the two
    # tiers agree most closely of anything in this registry.
    "dart_flight": (2.0, 24),
    # …and a densely packed cloud of flakes reads the same way for the
    # opposite reason: every flake stands against its neighbour rather
    # than against the background, so there is hardly a silhouette in the
    # picture to disagree about.
    "deformed_cloud": (2.0, 24),
    # A scatter thin enough to see through is the other extreme: nearly
    # every lit pixel of it IS a silhouette edge, one rasteriser
    # antialiases those and the other does not, and the p99 says so
    # while the mean says the two are the same picture.
    "scattered_model": (4.0, 128),
    # Four coloured lamps read as directions on the host and as
    # attenuated emitters on the device, so the bodies between them are
    # shaded from slightly different strengths — a low mean over a
    # picture that is mostly dark, and a p99 at the lit edges.
    "lantern_room": (4.0, 64),
}


# HOW FAR A PROMOTED PLATE MAY STAND FROM THE SAME SCENE RENDERED WITH THE
# PROMOTER HELD OFF: one code value on any channel of any pixel. It is not a
# tolerance anyone chose — it is the whole of what an integer translation
# under an inexact scale can cost a shaded pixel, so anything past it is a
# picture that moved rather than a picture that rounded.
PROMOTION_DRIFT_CEILING = 1


def read_manifest(path):
    """scene -> digest for a baseline manifest, empty when there is none."""
    baseline = {}
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                digest, _, scene = line.strip().partition("  ")
                if scene:
                    baseline[scene] = digest
    return baseline


def write_manifest(path, keep, results):
    """The baseline manifest, replaced whole, with @p results merged over
    whichever of its entries @p keep selects from the file AS IT STANDS.

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
    lock = path + ".lock"
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(lock, "w") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        merged = {}
        if keep is not None:
            standing = read_manifest(path)
            merged = (
                standing
                if keep is True
                else {s: d for s, d in standing.items() if s in keep}
            )
        merged.update(results)
        temporary = f"{path}.{os.getpid()}.tmp"
        with open(temporary, "w") as f:
            f.writelines(f"{merged[scene]}  {scene}\n" for scene in sorted(merged))
        os.replace(temporary, path)
    return merged


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


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
        listed = subprocess.run(
            [binary, "--list", "--kind", kind],
            capture_output=True,
            text=True,
            check=True,
        ).stdout
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
        r = subprocess.run(
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
    if r.returncode != 0 or not os.path.exists(plate):
        return scene, None, (r.stderr or r.stdout).strip()[-300:], elapsed
    return scene, sha256(plate), None, elapsed


def compared(binary, first, second):
    """Every plate in both directories, differenced by the renderer that
    wrote them: name -> (mean, p99, max), plus the names it could not
    compare.

    Decoding a PNG and differencing two pictures is what the binary
    already does; what stays here is the judgement — which distance is
    close enough on this machine — because that is a tolerance and not a
    fact about two files."""
    r = subprocess.run(
        [binary, "--compare", first, second], capture_output=True, text=True
    )
    distances, unusable = {}, {}
    for line in r.stdout.splitlines():
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
        unusable["--compare"] = (r.stderr or r.stdout).strip()[-300:]
    return distances, unusable


def plate_dir(root, config, *parts, fresh=True):
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
    directory = os.path.join(root, "build", f"plates_{config}", *parts)
    if fresh:
        shutil.rmtree(directory, ignore_errors=True)
    os.makedirs(directory, exist_ok=True)
    return directory


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
        binary,
        scenes,
        host_dir,
        timeout,
        jobs,
        PROMOTION_OFF,
        lambda s, d: "rendered",
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

    THE PROMOTED SET IS THE SCENE'S, NOT THE MACHINE'S. The `on` half
    renders with the runtime's promotion policy set EAGER: every node the
    rules admit is baked from its first frame, whatever it costs, instead
    of whatever a stopwatch happened to find expensive under this run's
    load. So the tier tests the same nodes on every machine, and all of
    the promotable ones rather than the few slow ones.

    THE BAR IS ONE CODE VALUE ANYWHERE. A promoted node is baked under the
    live matrix post-translated by an integer, and inverting that matrix to
    find a shader's local coordinates does not cancel the integer to the
    last bit at a scale whose reciprocal is inexact — so a shaded pixel can
    land one code value from the live paint and nothing may land further.
    A worst channel over 1 is a picture that MOVED: the bake landed
    somewhere else, or was rasterised against a different clip, or went
    stale. That is a defect to file against the promoter, never a plate to
    rebase — there is no baseline here to rebase into. Both halves are
    kept, so a scene reported MOVED can be opened beside the plate it was
    meant to match."""

    print("[promotion off]")
    _, off_errors = sweep(
        binary,
        scenes,
        off_dir,
        timeout,
        jobs,
        PROMOTION_OFF,
        lambda s, d: "rendered",
    )
    print("[promotion on]")
    _, on_errors = sweep(
        binary,
        scenes,
        on_dir,
        timeout,
        jobs,
        PROMOTION_ON,
        lambda s, d: "rendered",
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


def main():
    ap = argparse.ArgumentParser(
        description="plate sweep over the sketch registry, judged against a "
        "machine-local baseline manifest"
    )
    ap.add_argument("--config", default="Release")
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
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    binary = os.path.join(root, "build/bin", args.config, BINARY)
    manifest = os.path.join(root, "build", f"plate_baseline_{args.config}.sha256")
    if not os.path.exists(binary):
        sys.exit(f"no binary at {binary} — build the Sketchbook target first")

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
            plate_dir(root, args.config, "device", "cpu"),
            plate_dir(root, args.config, "device", "gpu"),
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
            plate_dir(root, args.config, "promotion", "off"),
            plate_dir(root, args.config, "promotion", "on"),
        )

    print(f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, tier cpu")

    # Read BEFORE the sweep so a scene can be judged the moment it lands.
    baseline = read_manifest(manifest)
    adopting = args.rebase or not os.path.exists(manifest)

    def standing(scene, digest):
        if args.rebase or scene not in baseline:
            return "rendered"
        return "identical" if baseline[scene] == digest else "hash miss"

    # An adopting sweep IS the baseline, so it renders straight into the
    # kept baseline directory and the manifest is written from the same
    # plates. A judging sweep renders beside it, which leaves the two
    # directories `--compare` differences standing when it is over.
    kept_baseline = plate_dir(root, args.config, "baseline", fresh=False)
    outdir = kept_baseline if adopting else plate_dir(root, args.config, "cpu")
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
        merged = write_manifest(manifest, keep, results)
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
            if scene not in baseline:
                missing.append(scene)
            elif baseline[scene] != digest:
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
                        plate_dir(root, args.config, "stability", fresh=False),
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
                f"  MOVED  {scene}  {baseline[scene][:12]} -> "
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


if __name__ == "__main__":
    sys.exit(main())
