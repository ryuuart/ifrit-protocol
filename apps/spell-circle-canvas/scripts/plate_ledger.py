#!/usr/bin/env python3
"""The plate ledger, as ONE command: parallel plate sweeps.

Renders every sketch through `Sketchbook --headless --ledger` (the
benchmark-free exact-stepped capture), N at a time, hashes the plates,
and compares against a stored baseline manifest. One binary renders
both tiers; what separates them is which rasteriser a sketch draws
through.

Usage (from apps/spell-circle-canvas):
  scripts/plate_ledger.py --rebase           # bake the baseline manifest
  scripts/plate_ledger.py                    # sweep + compare + verdict
  scripts/plate_ledger.py --kind set         # only the sketches that light a set
  scripts/plate_ledger.py --sketch astral_tome
  scripts/plate_ledger.py --scenes "aero desktop" black_watch
  scripts/plate_ledger.py --stability 3      # re-render movers 3x to
                                             # separate flappers from code
  scripts/plate_ledger.py --tier device      # the same sketches on the GPU
  scripts/plate_ledger.py --jobs 6 --config Release

TWO TIERS (--tier):

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

The manifest lives in build/ (machine-local on purpose: plates are
AA-deterministic per machine, not across machines), so a fresh checkout
runs `--rebase` once before a sweep can judge anything. Both tiers
render with --no-promotion: automatic texture promotion re-bakes by a
measured per-frame cost, which load can tip either way, so it is the
one renderer feature a byte-identity gate must hold off — with it off,
hashes are load-immune.

EVERY SUCCESSFUL CPU SWEEP REFRESHES SKETCHBOOK'S THUMBNAILS: the plates
it rendered are copied into the directories Sketchbook shows its stills
from, one per kind. The thumbnails are not the baseline — retaining the
current pictures changes no verdict, which is still the manifest's byte
identity — so a new or deliberately changed sketch has a thumbnail
before it has been adopted.

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
import atexit
import concurrent.futures
import fcntl
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time
import zlib

# One binary renders both tiers, and one prefix names every plate.
BINARY = "Sketchbook.app/Contents/MacOS/Sketchbook"
PLATE_PREFIX = "plate_"
KINDS = ("canvas", "set")

# The flags every render carries: the benchmark-free exact-stepped
# capture, with cost-based texture promotion held off.
RENDER_ARGS = ("--no-promotion", "--ledger")

# WHERE SKETCHBOOK SHOWS ITS STILLS FROM, per kind. The directory names
# are compiled into Sketchbook, and every configuration of it reads the
# Release stores, so the ledger writes them for whichever configuration
# it rendered under those names.
THUMBNAIL_STORES = {
    "canvas": "plate_thumbnails_quick_{config}",
    "set": "plate_thumbnails_world_{config}",
}

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


def render_scene(binary, scene, outdir, timeout, extra_args=()):
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
    plate = os.path.join(outdir, f"{PLATE_PREFIX}{scene}.png")
    if r.returncode != 0 or not os.path.exists(plate):
        return scene, None, (r.stderr or r.stdout).strip()[-300:], elapsed
    return scene, sha256(plate), None, elapsed


def read_png(path):
    """An RGBA8 image out of a PNG, with nothing but the standard library.

    The cpu tier hashes bytes and needs no decoder; the device tier
    compares two pictures pixel by pixel, and the repository's Python
    declares no imaging package, so the 8-bit truecolour forms the plate
    writer emits are decoded here."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    at, width, height, depth, colour, idat = 8, 0, 0, 0, 0, bytearray()
    while at + 8 <= len(data):
        length = struct.unpack(">I", data[at : at + 4])[0]
        kind = data[at + 4 : at + 8]
        body = data[at + 8 : at + 8 + length]
        if kind == b"IHDR":
            width, height, depth, colour = struct.unpack(">IIBB", body[:10])
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
        at += 12 + length
    if depth != 8 or colour not in (2, 6):
        raise ValueError(f"{path}: unsupported PNG ({depth}-bit, colour {colour})")
    channels = 4 if colour == 6 else 3
    raw = zlib.decompress(bytes(idat))
    stride = width * channels
    out = bytearray(height * stride)
    previous = bytearray(stride)
    at = 0
    for y in range(height):
        filter_type = raw[at]
        at += 1
        row = bytearray(raw[at : at + stride])
        at += stride
        if filter_type == 1:
            for i in range(channels, stride):
                row[i] = (row[i] + row[i - channels]) & 0xFF
        elif filter_type == 2:
            for i in range(stride):
                row[i] = (row[i] + previous[i]) & 0xFF
        elif filter_type == 3:
            for i in range(stride):
                left = row[i - channels] if i >= channels else 0
                row[i] = (row[i] + ((left + previous[i]) >> 1)) & 0xFF
        elif filter_type == 4:
            for i in range(stride):
                left = row[i - channels] if i >= channels else 0
                up = previous[i]
                upleft = previous[i - channels] if i >= channels else 0
                p = left + up - upleft
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - upleft)
                best = left if (pa <= pb and pa <= pc) else (up if pb <= pc else upleft)
                row[i] = (row[i] + best) & 0xFF
        out[y * stride : (y + 1) * stride] = row
        previous = row
    return width, height, channels, out


def channel_distance(reference, candidate):
    """Mean, 99th percentile and worst absolute channel difference in
    0..255, or None when the two plates are not the same size."""
    rw, rh, rc, rd = read_png(reference)
    cw, ch, cc, cd = read_png(candidate)
    if (rw, rh, rc) != (cw, ch, cc):
        return None
    histogram = [0] * 256
    for value in map(abs, map(int.__sub__, rd, cd)):
        histogram[value] += 1
    count = len(rd)
    total = sum(value * n for value, n in enumerate(histogram))
    seen, p99, worst = 0, None, 0
    cut = count * 0.99
    for value, n in enumerate(histogram):
        if n:
            worst = value
        seen += n
        if p99 is None and seen >= cut:
            p99 = value
    return total / count, p99 or 0, worst


def store_thumbnails(root, config, rendered_dir, scenes, kinds, whole_registry):
    """Copies the rendered plates into Sketchbook's per-kind stores.

    A complete sweep re-makes a store so a scene that left the registry
    cannot linger; a subset or partially failed sweep replaces only the
    scenes it rendered."""
    for kind in kinds:
        store = os.path.join(
            root, "build", THUMBNAIL_STORES[kind].format(config=config)
        )
        chosen = [scene for scene in scenes if scenes[scene] == kind]
        if whole_registry and os.path.isdir(store):
            shutil.rmtree(store)
        os.makedirs(store, exist_ok=True)
        for scene in chosen:
            plate = f"{PLATE_PREFIX}{scene}.png"
            shutil.copyfile(
                os.path.join(rendered_dir, plate), os.path.join(store, plate)
            )
        print(f"thumbnails written: {store} ({len(chosen)} plates)")


def discard_later(directory):
    """A sweep's plates, marked for removal when the process ends.

    Every tier writes about one full-frame plate per scene, and no path to
    them is ever printed: what a sweep reports is digests and distances,
    never a file. Left behind they accumulate a sweep's worth of frames per
    run in the system temporary directory, so removal is registered where
    the directory is made and holds on every exit path.
    """
    atexit.register(shutil.rmtree, directory, True)
    return directory


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


def device_sweep(binary, scenes, timeout, jobs):
    """The device tier: every sketch rendered BOTH ways and the two plates
    compared. It has no baseline — the CPU plate of the same sketch IS
    the reference, and both are made in this run."""
    host_dir = discard_later(tempfile.mkdtemp(prefix="plate_cpu_"))
    device_dir = discard_later(tempfile.mkdtemp(prefix="plate_gpu_"))

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
        binary, scenes, host_dir, timeout, jobs, (), lambda s, d: "rendered"
    )
    print("[gpu]")
    _, gpu_errors = sweep(
        binary, scenes, device_dir, timeout, jobs, ("--gpu",), lambda s, d: "rendered"
    )
    errors = len(cpu_errors) + len(gpu_errors)

    verdict = 0
    print()
    for scene in scenes:
        reference = os.path.join(host_dir, f"{PLATE_PREFIX}{scene}.png")
        candidate = os.path.join(device_dir, f"{PLATE_PREFIX}{scene}.png")
        if not (os.path.exists(reference) and os.path.exists(candidate)):
            verdict = 1
            continue
        measured = channel_distance(reference, candidate)
        if measured is None:
            print(f"  MISMATCHED SIZE {scene}   <-- FINDING")
            verdict = 1
            continue
        mean, p99, worst = measured
        mean_cap, p99_cap = GPU_TOLERANCE.get(scene, DEFAULT_GPU_TOLERANCE)
        over = mean > mean_cap or p99 > p99_cap
        print(
            f"  {'OVER ' if over else 'WITHIN'} {scene:<24} "
            f"mean {mean:6.2f} (<= {mean_cap:g})  "
            f"p99 {p99:4d} (<= {p99_cap})  max {worst:3d}"
        )
        if over:
            verdict = 1
    if verdict == 0 and not errors:
        print("VERDICT: the device tier stands within tolerance of the CPU tier")
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
        choices=("cpu", "device"),
        default="cpu",
        help="cpu (default): CPU renders to each scene's declared capture "
        "moment, judged on byte identity against the baseline manifest. "
        "device: the same scenes on the GPU, judged per colour channel "
        "against the CPU plate of the same run; no baseline",
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
        return device_sweep(binary, list(scenes), args.timeout_seconds, args.jobs)

    print(f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, tier cpu")

    # Read BEFORE the sweep so a scene can be judged the moment it lands.
    baseline = read_manifest(manifest)

    def standing(scene, digest):
        if args.rebase or scene not in baseline:
            return "rendered"
        return "identical" if baseline[scene] == digest else "hash miss"

    outdir = discard_later(tempfile.mkdtemp(prefix="plate_ledger_"))
    results, errors = sweep(
        binary, list(scenes), outdir, args.timeout_seconds, args.jobs, (), standing
    )
    rendered = {scene: scenes[scene] for scene in results}

    if args.rebase or not os.path.exists(manifest):
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
        print(
            f"baseline written: {manifest} ({len(merged)} scenes, "
            f"{len(results)} from this sweep)"
        )
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
                        discard_later(tempfile.mkdtemp(prefix="plate_stab_")),
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
                f"{results[scene][:12]}   <-- FINDING"
            )
            verdict = 1
        for scene in missing:
            print(f"  NEW    {scene} (not in baseline — rebase to adopt)")
        if verdict == 0 and not errors:
            print("VERDICT: byte-neutral")

    if rendered:
        store_thumbnails(
            root, args.config, outdir, rendered, kinds, not narrowed and not errors
        )
    return verdict or (1 if errors else 0)


if __name__ == "__main__":
    sys.exit(main())
