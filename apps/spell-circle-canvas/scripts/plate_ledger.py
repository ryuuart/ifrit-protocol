#!/usr/bin/env python3
"""The plate ledger, as ONE command: parallel byte-identity sweeps.

Renders every sketch through `Sketchbook --headless --ledger` (the
benchmark-free exact-stepped capture), N at a time, hashes the plates,
and compares against a stored baseline manifest. One binary renders
every tier; what separates them is which runtime a sketch draws through
and what capture the flags ask for.

Usage (from apps/spell-circle-canvas):
  scripts/plate_ledger.py --rebase          # bake the baseline manifest
  scripts/plate_ledger.py                   # sweep + compare + verdict
  scripts/plate_ledger.py --tier quick      # the iteration loop (see below)
  scripts/plate_ledger.py --tier world      # the 3D studies, on the CPU
  scripts/plate_ledger.py --tier world-gpu  # the same studies, on a device
  scripts/plate_ledger.py --stability 3     # re-render movers 3x to
                                            # separate flappers from code
  scripts/plate_ledger.py --jobs 6 --config Release

FOUR VERIFICATION TIERS (--tier):

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
  the GPU raster path rather than the full tier's CPU path. A THIRD
  blind spot, measured: the device path is a function of the scene AND
  of the binary — two executables built from the same drawing code
  produce plates that differ in a few hundred colour channels out of
  nine million, stably, while the CPU tier is byte-identical. So quick
  answers "did I move any bytes I didn't mean to?" during iteration,
  and the full tier answers it authoritatively at the end.

  world — the 3D studies: the sketches that light a set rather than draw
  onto a canvas. Each is stepped from zero to its declared moment
  and drawn through SigilGeometry's CPU mesh executor, so a plate is a
  function of the declaration alone and the tier is judged on byte
  identity exactly as the full tier is. It is a separate HALF OF THE
  REGISTRY, not a separate question: the same sweep, the same hashes,
  the same verdict table, against its own baseline
  (build/plate_baseline_world_<config>.sha256). It needs no device — a
  machine with no GPU runs it green.

  world-gpu — the same studies, rendered through the Diligent runtime
  instead of the CPU one. It is the ONE TIER THAT IS NOT JUDGED ON BYTE
  IDENTITY, and it has no baseline of its own: each plate is compared
  against the world tier's plate of the same study, which must therefore
  have been rendered first (the sweep renders it, so nothing has to be
  kept between runs).

  WHY A DISTANCE AND NOT A HASH. The two tiers are two rasterisers. The
  host paints shaded vertices through a per-triangle sort with Skia's
  antialiasing; the device rasterises the same shading through a depth
  buffer with none. They agree about what the scene is and they differ
  along every edge, and a post pass's blur is a box approximation on one
  side and a separable Gaussian on the other. Asking for equal bytes
  would fail on the first pixel and tell no one anything.

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

  Each study names its own mean and p99 ceilings in GPU_TOLERANCE below,
  set from what the two tiers actually do rather than from a wish. A
  study with no entry there is judged by DEFAULT_GPU_TOLERANCE.

  It SKIPS cleanly with no device: `--gpu` reports that it found none,
  the tier says so and exits 0, because a machine with no Vulkan runtime
  has nothing to disagree about.

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

THE GPU 60 FPS GATE (`--fps-gate`) is a SEPARATE LANE, deliberately never
mixed into the byte-identity sweep, because timing and hashing want
opposite conditions: hashing wants parallel, benchmark-free (--ledger),
--no-promotion CPU renders — load-immune because a hash cannot flap under
contention; timing wants SERIAL single-scene renders on the Graphite/Metal
backend (--gpu) with the full benchmark phases, because parallel renders
contend for the machine and corrupt every number — the same load
sensitivity that makes hashing want the opposite. The gate runs
`Sketchbook --headless --sketch <s> --gpu` one sketch at a time and reads
the steady-state sample through --timing-json. It works over either
runtime: both write the same timing line.

IT JUDGES TWO NUMBERS, and a scene fails on either: the END-TO-END frame
time (CPU + drained GPU via submit(SyncToCpu) inside the timed window)
against --budget-ms, and the MODELED HEADROOM (1000 / mean work ms, that
drain taken back out) against --headroom-fps. The first is the
pessimistic bound — a real host pipelines the CPU and the GPU while this
lane serializes them — and the second is the optimistic one, the rate the
frame's own work would allow with the GPU assumed free.

Work ms is part of frame ms, so at the DEFAULT --headroom-fps
(1000/--budget-ms) the second check cannot fail on its own: it asserts
the relationship between the lanes rather than adding a test. Raise it to
make the optimistic bound bite — a scene whose CPU frame alone eats most
of the budget has nothing left for the GPU work a pipelined host runs
beside it, and passes here until the content grows.

Presented FPS is the interactive lane's number and stays there — a
headless sweep presents nothing. Exit 1 when any scene fails either. Run
it on a quiet machine; there is no known-flapper list here — a load spike
is YOUR machine, rerun the scene.
"""

import argparse, concurrent.futures, hashlib, json, os, struct, subprocess, sys, tempfile, zlib

# WHAT EACH TIER RENDERS WITH. A tier names its own binary, the flags that
# define its capture, how it lists its registry, how it selects one entry
# and which runtime's sketches it walks. They ask one question — did any
# byte move that I did not mean to move — of different halves of one
# registry, which is why they share this sweep instead of forking a
# second script.
TIERS = {
    "full": {
        "base_args": ("--no-promotion", "--ledger"),
        "kind": "canvas",
        "honor_overrides": True,
    },
    "quick": {
        "base_args": ("--no-promotion", "--ledger"),
        "kind": "canvas",
        # The capture cap removes exactly the cost the per-scene overrides
        # budget for, so a quick render still running at the default
        # ceiling is a defect rather than an expensive scene.
        "honor_overrides": False,
    },
    "world": {
        "base_args": (),
        "kind": "set",
        "honor_overrides": False,
    },
    "world-gpu": {
        "base_args": ("--gpu",),
        "kind": "set",
        "honor_overrides": False,
    },
}

# One binary renders every tier, and one prefix names every plate: what
# separates the tiers is which runtime a sketch draws through and what
# capture the flags ask for, not which program was run.
BINARY = "Sketchbook.app/Contents/MacOS/Sketchbook"
PLATE_PREFIX = "plate_"

# HOW FAR A STUDY'S DEVICE PLATE MAY STAND FROM ITS CPU PLATE: (mean,
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
    # material_lab is the looser of the rest, and for one reason: five
    # upright cards over a broad textured floor is mostly ground plane,
    # and a ground plane is where the two tiers' vertex-versus-pixel
    # clamping drifts (see first_light above). Its map lands identically
    # on both, which is what the number says.
    "material_lab": (6.0, 64),
    # A card sampled onto a swept band and a still set under a ramping
    # key are both nearly all interior: the two tiers agree to a channel
    # or two everywhere but the silhouettes.
    "woven_card": (3.0, 32),
    "key_light": (3.0, 32),
}

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


def render_scene(profile, binary, scene, outdir, timeout, extra_args=()):
    # The per-scene timeout overrides budget the FULL tier's declared-moment
    # renders; the tiers that do not pay that cost say so in their profile.
    if profile["honor_overrides"]:
        timeout = timeout_for(scene, timeout)
    try:
        r = subprocess.run(
            [
                binary,
                "--headless",
                outdir,
                *profile["base_args"],
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
                f"FAILED-TIMEOUT: still rendering after "
                f"{timeout:g}s (killed; raise "
                f"--timeout-seconds if the scene is merely "
                f"slow)"
            ),
        )
    plate = os.path.join(outdir, f"{PLATE_PREFIX}{scene}.png")
    if r.returncode != 0 or not os.path.exists(plate):
        return scene, None, (r.stderr or r.stdout).strip()[-300:]
    return scene, sha256(plate), None


def read_png(path):
    """An RGBA8 image out of a PNG, with nothing but the standard library.

    The ledger's other tiers hash bytes and need no decoder; this one
    compares two pictures pixel by pixel, and adding an imaging package
    to the gate that protects the build is a worse trade than sixty lines
    here. Handles the 8-bit truecolour forms the plate writer emits."""
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


def gpu_sweep(profile, binary, scenes, timeout, jobs):
    """The device tier: every study rendered BOTH ways and the two plates
    compared. It has no baseline — the CPU tier's plate of the same study
    IS the reference, and both are made in this run."""
    host_dir = tempfile.mkdtemp(prefix="plate_world_cpu_")
    device_dir = tempfile.mkdtemp(prefix="plate_world_gpu_")
    host_profile = dict(profile, base_args=())

    # One study first, to tell "no device on this machine" from a defect.
    probe = subprocess.run(
        [binary, "--headless", device_dir, "--gpu", "--study", scenes[0]],
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    if probe.returncode != 0 and "no device runtime" in (probe.stderr + probe.stdout):
        print("SKIPPED: this machine has no device to render the studies on")
        return 0

    errors = {}
    with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
        for which, outdir, prof in (
            ("cpu", host_dir, host_profile),
            ("gpu", device_dir, profile),
        ):
            for scene, digest, err in pool.map(
                lambda s, o=outdir, p=prof: render_scene(p, binary, s, o, timeout),
                scenes,
            ):
                if digest is None:
                    errors[f"{scene} ({which})"] = err
    for scene, err in sorted(errors.items()):
        print(f"RENDER FAILED  {scene}: {err}")

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
            f"  {'OVER ' if over else 'WITHIN'} {scene:<20} "
            f"mean {mean:6.2f} (<= {mean_cap:g})  "
            f"p99 {p99:4d} (<= {p99_cap})  max {worst:3d}"
        )
        if over:
            verdict = 1
    if verdict == 0 and not errors:
        print("VERDICT: the device tier stands within tolerance of the CPU tier")
    return verdict or (1 if errors else 0)


def gate_verdict(row, budget_ms, floor_fps):
    """Which of the two numbers a scene failed, as a list of reasons.

    The gate judges BOTH, because a headless lane presents nothing and so
    has no single honest frame rate to judge instead:

      frame_ms      end to end, the backend flush included — on --gpu a
                    submit(SyncToCpu) per frame, so this is the SERIALIZED
                    CPU+GPU cost of one frame. A real host pipelines the
                    two, so this is the pessimistic bound: a scene passing
                    it cannot miss for want of either.
      headroom_fps  1000 / mean(work ms), the flush taken back out — the
                    rate the frame's own work would allow, with the GPU
                    assumed free. The optimistic bound, and the one that
                    answers "is the CPU side alone already too slow".

    THE TWO FLOORS ARE INDEPENDENT KNOBS, and they need to be. Work ms is
    part of frame ms, so at a shared floor (--headroom-fps defaulting to
    1000/--budget-ms) the headroom check can never fail on its own: any
    scene inside the end-to-end budget is inside the modeled one too. That
    default asserts the relationship rather than adding a test. Raise
    --headroom-fps to make the optimistic bound bite — a scene whose CPU
    frame alone eats most of the budget has nothing left for the GPU work
    a pipelined host would be running beside it, and reads as passing
    until the content grows."""
    reasons = []
    if row["frame_ms"] > budget_ms:
        reasons.append(f"frame {row['frame_ms']:.2f} ms > {budget_ms:g} ms")
    if row["headroom_fps"] < floor_fps:
        reasons.append(f"headroom {row['headroom_fps']:.0f} fps < {floor_fps:.0f} fps")
    return reasons


def fps_gate(binary, scenes, budget_ms, floor_fps, timeout):
    """SERIAL GPU timing sweep; see the module docstring for why serial and
    why this never shares a run with the hash sweep. Each scene gets the
    timing instrument: single-scene mode (the full unbudgeted 240-warm/120-sample
    window), --gpu (frame ms = CPU + drained GPU, submit(SyncToCpu) per
    frame; work ms is the same frame with that drain taken out).
    --capture-at 0.02 collapses the post-sample capture pass to one
    frame — the timing sample closes before the capture pass begins (the
    JSON line snapshots at sample-window close), so this only trims wall
    clock, never the measurement; the throwaway plates land in a temp dir
    and are NOT ledger material (wrong backend, wrong conditions)."""
    outdir = tempfile.mkdtemp(prefix="fps_gate_")
    print(
        f"GPU 60 FPS gate: {len(scenes)} scenes, serial, budget "
        f"{budget_ms} ms end-to-end AND {floor_fps:.0f} fps modeled "
        f"headroom\n"
    )
    rows, failures, errors = [], [], []
    for scene in scenes:
        tj = os.path.join(outdir, f"timing_{scene}.json")
        try:
            r = subprocess.run(
                [
                    binary,
                    "--headless",
                    outdir,
                    "--gpu",
                    "--sketch",
                    scene,
                    "--timing-json",
                    tj,
                    "--capture-at",
                    "0.02",
                ],
                capture_output=True,
                text=True,
                timeout=timeout,
            )
        except subprocess.TimeoutExpired:
            errors.append(scene)
            print(
                f"  FAILED-TIMEOUT {scene}: still rendering after {timeout:g}s, killed"
            )
            continue
        if r.returncode != 0 or not os.path.exists(tj):
            errors.append(scene)
            print(f"  RENDER FAILED  {scene}: {(r.stderr or r.stdout).strip()[-200:]}")
            continue
        with open(tj) as f:
            row = json.loads(f.readline())
        rows.append(row)
        reasons = gate_verdict(row, budget_ms, floor_fps)
        if reasons:
            row["reasons"] = reasons
            failures.append(row)
        print(
            f"  {'FAIL' if reasons else 'PASS'}  {scene:<24} "
            f"frame {row['frame_ms']:8.2f} ms  (p99 {row['p99_ms']:7.2f}, "
            f"work {row['work_ms']:7.2f}, "
            f"{row['headroom_fps']:5.0f} fps headroom)"
        )
    rows.sort(key=lambda r: -r["frame_ms"])
    print(
        f"\n{len(rows) - len(failures)} under budget, {len(failures)} "
        f"over, {len(errors)} failed to render"
    )
    if failures:
        print("OVER BUDGET (steady frame, GPU):")
        for row in sorted(failures, key=lambda r: -r["frame_ms"]):
            print(f"  {row['scene']:<24} {'; '.join(row['reasons'])}")
    return 1 if failures or errors else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--config", default="Release")
    ap.add_argument("--jobs", type=int, default=max(2, (os.cpu_count() or 8) // 2))
    ap.add_argument(
        "--tier",
        choices=tuple(TIERS),
        default="full",
        help="verification tier. full (default): CPU renders to "
        "each scene's declared capture moment — the final "
        "confirmation gate. quick: GPU renders with a "
        "uniform capture-time cap, compared against the "
        "separate quick baseline — the iteration loop. world: "
        "the 3D studies, stepped to their declared moments on "
        "the CPU mesh executor, against their own baseline. "
        "Each tier has its own baseline and they are never "
        "comparable. See the module docstring",
    )
    ap.add_argument(
        "--capture-cap",
        type=float,
        default=2.0,
        metavar="S",
        help="quick tier only: the uniform capture time in "
        "seconds, passed to the renderer as --capture-at "
        "(default 2.0). Content after this time is invisible "
        "to the quick tier",
    )
    ap.add_argument(
        "--rebase",
        action="store_true",
        help="write the manifest from this sweep (tier-specific: "
        "--tier quick --rebase writes the quick baseline)",
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
    ap.add_argument("--scenes", nargs="*", help="subset (registry names)")
    ap.add_argument(
        "--fps-gate",
        action="store_true",
        help="the GPU 60 FPS gate — a separate SERIAL lane, not "
        "part of the byte-identity sweep (timing and "
        "hashing want opposite conditions; see the module "
        "docstring). Renders each scene alone with --gpu "
        "through the full benchmark phases and fails any "
        "whose steady end-to-end frame exceeds --budget-ms "
        "OR whose modeled headroom falls under --headroom-fps",
    )
    ap.add_argument(
        "--budget-ms",
        type=float,
        default=16.7,
        help="fps-gate END-TO-END frame budget in ms (default 16.7 = "
        "60 FPS): CPU plus the synchronously drained GPU",
    )
    ap.add_argument(
        "--headroom-fps",
        type=float,
        default=None,
        metavar="FPS",
        help="fps-gate floor on the MODELED headroom (1000 / mean work "
        "ms, the GPU drain taken back out). Defaults to the rate "
        "--budget-ms implies, which asserts the relationship between "
        "the two lanes rather than testing anything: work ms is part "
        "of frame ms, so at that default a scene inside the end-to-end "
        "budget is always inside this one. Raise it to make the "
        "modeled bound bite on its own",
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
        "hang the gate that protects everything else",
    )
    args = ap.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    profile = TIERS[args.tier]
    binary = os.path.join(root, "build/bin", args.config, BINARY)
    # Each tier compares against its own baseline: quick plates are rendered
    # on a different backend at a different scene time, so their hashes can
    # never match the full manifest and must never be written into it.
    tier_tag = "" if args.tier == "full" else f"_{args.tier}"
    manifest = os.path.join(
        root, "build", f"plate_baseline{tier_tag}_{args.config}.sha256"
    )
    if not os.path.exists(binary):
        sys.exit(f"no binary at {binary} — build the {args.tier} tier's renderer first")

    # The render arguments that define the tier. GPU stability was validated
    # empirically on this machine: representative scenes hash identically
    # across processes under this exact invocation.
    quick = args.tier == "quick"
    tier_args = ("--gpu", "--capture-at", f"{args.capture_cap:g}") if quick else ()

    # The registry filtered to the runtime this tier renders: the 2D tiers
    # ask for the sketches drawn onto a canvas, the world tiers for the ones
    # that light a set. One registry, two questions.
    scenes = args.scenes or [
        s
        for s in subprocess.run(
            [binary, "--list", "--kind", profile["kind"]],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.splitlines()
        if s.strip()
    ]

    if args.tier == "world-gpu":
        if args.rebase:
            sys.exit(
                "--tier world-gpu has no baseline to rebase: it is judged "
                "against the world tier's plates, which the same sweep "
                "renders. Change GPU_TOLERANCE to move what it accepts."
            )
        print(
            f"{len(scenes)} studies, config {args.config}, tier world-gpu: "
            f"each rendered on the CPU and on the device and compared per "
            f"colour channel"
        )
        return gpu_sweep(profile, binary, scenes, args.timeout_seconds, args.jobs)
    if args.fps_gate:
        floor_fps = (
            args.headroom_fps
            if args.headroom_fps is not None
            else 1000.0 / args.budget_ms
        )
        return fps_gate(binary, scenes, args.budget_ms, floor_fps, args.timeout_seconds)

    print(
        f"{len(scenes)} scenes, {args.jobs} jobs, config {args.config}, "
        f"tier {args.tier}"
    )
    if quick:
        print(
            f"QUICK TIER: GPU renders (--gpu --no-promotion), capture "
            f"capped at {args.capture_cap:g}s. Blind spots: content that "
            f"only appears after the cap is never hashed, and the backend "
            f"is the GPU raster path, not the full tier's CPU path. Run "
            f"the full tier as the final gate before trusting a "
            f"byte-neutral verdict."
        )

    outdir = tempfile.mkdtemp(prefix="plate_ledger_")
    results, errors = {}, {}
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for scene, digest, err in pool.map(
            lambda s: render_scene(
                profile, binary, s, outdir, args.timeout_seconds, tier_args
            ),
            scenes,
        ):
            if digest is None:
                errors[scene] = err
            else:
                results[scene] = digest
    for scene, err in sorted(errors.items()):
        print(f"RENDER FAILED  {scene}: {err}")

    if args.rebase or not os.path.exists(manifest):
        if not args.rebase:
            print(
                f"no manifest at {manifest} — writing one (this sweep "
                f"becomes the baseline)"
            )
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
        print(
            f"baseline written: {manifest} ({len(merged)} scenes, "
            f"{len(results)} from this sweep)"
        )
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
    print(
        f"\n{identical} byte-identical, {len(movers)} moved, "
        f"{len(missing)} not in baseline, {len(errors)} failed"
    )

    verdict = 0
    for scene in movers:
        if args.stability > 0:
            rerenders = {results[scene]}
            for _ in range(args.stability):
                _, digest, err = render_scene(
                    profile,
                    binary,
                    scene,
                    tempfile.mkdtemp(prefix="plate_stab_"),
                    args.timeout_seconds,
                    tier_args,
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
    return verdict or (1 if errors else 0)


if __name__ == "__main__":
    sys.exit(main())
