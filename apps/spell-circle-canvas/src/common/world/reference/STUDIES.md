# SigilWorld — the studies and their plates

The chapter on the fifteen 3D frames this library is looked at
through, and on the ledger that judges them: what each study draws, the
presets they are built out of, and what the byte-identity, device and
promotion tiers each ask of a plate. `README.md` beside the library is
the front page; `src/sketch/README.md` is the canon for the registry,
the live host and the plates themselves.

## Studies

A study is one 3D frame, stepped from zero at a fixed 1/60 to its
declared moment and photographed — so a plate is a function of the
declaration alone and never of how fast the machine ran.

A study is a **sketch**, and the harness that runs one belongs to
SigilSketch rather than here: this library draws a frame and says nothing
about how a frame is photographed. `src/sketch/README.md` is the canon
for the registry, the live host and the plates.

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
    --headless <outdir> --kind set [--sketch <name>] [--gpu]
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook --list --kind set
```

`--sketch` takes a case-insensitive substring, which is the loop for
visual iteration. A study joins the registry by being a file in
`src/sketch/sketches/`.

`--gpu` renders every study through the device runtime instead. A study
that declared no passes is wrapped in one geometry pass clearing to its
background, because an executor is only reached through passes and a
study about the scene must be able to say what it looks like on a device
too. The flag answers with the device or with nothing: on a machine with
no Vulkan runtime it reports that and fails, rather than quietly putting
the CPU's plate under a name that asked for the device's. The device is
brought up by the BINARY and installed once for the process, so no
feature here but `diligent/` links one, and a machine with no GPU still
renders the CPU tier.

Fifteen sketches draw through the Set runtime, and between them they
exercise every feature this library has:

- **`first_light`** — the scene: a tube swept along a closed loop, a
  comet of stamps riding a moving window of that same loop, a plate under
  both, a sun and a lamp, and a camera on a rail of its own.
- **`glow_trail`** — the passes. Its set is drawn once, and what is
  tagged "glow" is then reached three ways, one per realisation: a
  narrowed post pass lifts the beads in place through the coverage the
  geometry pass before it was made to write; a narrowed geometry pass
  draws the same beads alone into a target of their own, which is
  softened and dimmed; and that is laid over its own output from the
  frame before, so the comet drags a tail no single frame contains. Six
  passes, no stated order, and three of its surfaces are taken in turns.
- **`material_lab`** — what a surface is made of, and the difference
  between the tiers. Five curved cards over a floor wearing a texture
  set: one plain, one a STACK of two through a mask, one wearing a normal
  map, one wearing a packed roughness-and-metallic map read at two
  channels, and one that emits in a pattern. Every card is chosen because
  the device SHADES it, so the device plate is what the parameters and the
  maps say; the CPU plate is five flat colours and the floor's weave,
  because that tier reads a base colour and a base-colour map and nothing
  else. The cards are curved rather than flat, because a Blinn highlight
  on a flat card is one value over the whole face and a card meant to
  show a highlight narrowing has to present a range of normals to the
  key. The turntable is PARKED: a lab is read rather than watched, so the
  live picture and the plate are the same picture. The texture set is
  GENERATED in the study rather than read off
  the disk, through the same `texture::` door a scanned folder arrives
  by: a plate is a function of the declaration, and what a machine
  happens to have under `build/assets` is not.
- **`scene_surfaces`** — a compose scene as an ordinary texture, and
  every sampling dial applied to it. Three flat cards on an arc, one
  curved band under them and one swept ribbon whose card repeats along
  the band's length each wear a compose tree rendered by a composer of
  its own; the screens are unlit, so what they show is what the trees
  painted, and the ribbon is a lit surface, so the same texture is read
  through shading beside them.
- **`reflection_lab`** — what a body sees when it looks past the lights.
  Four spheres in a row under a sky — chrome, a rough metal, a
  dielectric and glass — each legible only because of the environment
  map; the sky is a node whose `rotateY` turns the reflections while the
  lights and bodies stand still, and the row stands under a held
  crossfade of two panoramas.
- **`set_stagger`** — the entrances of a set's children, cascaded, and
  the two selectors that address a subtree afterwards. Two rows differ
  only in their spread's origin, so at one moment they hold different
  shapes of the same cascade; `selectors::under` and `selectors::material` narrow a
  pass to one of them.
- **`key_light`** — the emitter's dials. One still set under the kit's
  three-point rig, with the key light's strength and colour bound to live
  values: nothing about the description changes from frame to frame, and
  what moves is what the lanes are bound to.
- **`dart_flight`** — `along()`, and nothing else. One winding closed
  loop swept into a rail, a dart flying it at a distance that is a
  function of the scene time, and gates standing on the same loop at
  constant distances and rolled about it — one verb serving a moving
  body and a still one, and composing with the rotation lanes rather
  than replacing them.
- **`scattered_model`** — the import door. A model decoded through the
  mesh codec and fitted to the stage, with a cloud scattered over its
  surface and a flake stamped at every point; the codec's output is the
  same `Mesh` a generated body is, so the tree past the import cannot
  tell which it holds. It reads `res://models/`, which nothing in this
  repository mounts, so what a plate is taken from is the generated
  subject.
- **`deformed_cloud`** — the point operators, in a room. A band across a
  scattered body is selected once and addressed twice: the points inside
  it are pushed out along their own normals, and inverting the same
  region turns everything outside it about the up axis. The chain is a
  value the node carries and the frame's runtime cooks it.
- **`lantern_room`** — the three emitters together. Four unlit lantern
  shells each carrying a coloured point light, a spot opening downward
  onto the cluster between them, and a sun faint enough to be an
  outline. An emitter stands where its node stands and carries no
  geometry, so a lantern here is two siblings sharing a placement.
- **`compute_variant`**, **`import_native`**, **`vagrant_story_target`**
  and **`world_hud`** — the pass verbs that draw nothing by themselves
  (cooking points, re-drawing a selection, asking for a resource back),
  the zero-copy import door, and two studies that hang a compose overlay
  on an unlit quad filling the frustum over a lit set.

Most of them are built out of `kit/`: `kit::threePoint` puts three
emitters round a subject in its own extents, `kit::turntable` rides a
closed rail looking inward, and `kit::litSet` is both over a ground
plane. Every one returns an ordinary `Element`, which is why `key_light`
can take the rig the preset returned and put lanes on its key light
without the preset offering a hook for it — and why `lantern_room`,
which lights its own room, takes the turntable alone and leaves the rest
of the preset behind rather than describing a second room over the top
of its own.

A study returns a `Frame`, and an `Element` is one with no passes, so a
study about the scene says nothing about passes at all. The host writes
the plate's size and its viewpoint into whichever it was handed.

## The plate ledger over the studies

A study is a sketch of the `set` kind, so it is judged by the one plate
ledger over the one registry, narrowed to that kind. The CPU tier
renders every study to its declared moment and hashes the bytes against
the manifest, `build/plate_baseline_<config>.sha256`. It is the same
question asked of a canvas sketch — did any byte move that I did not
mean to move — and it needs no device:

```sh
python3 scripts/sigil.py plates --kind set --rebase   # adopt a baseline
python3 scripts/sigil.py plates --kind set            # sweep and judge
python3 scripts/sigil.py plates --kind set --stability 2
python3 scripts/sigil.py plates --kind set --tier device
python3 scripts/sigil.py plates --kind set --tier promotion
```

A sweep narrowed to one kind merges into the manifest rather than
truncating it, so adopting a study's changed plate keeps every canvas
sketch's baseline.

`--tier device` renders the same studies through the device runtime
and is the ONE TIER NOT JUDGED ON BYTE IDENTITY. It has no baseline: each
plate is compared against the CPU tier's plate of the same study, and the
same sweep renders both. Two rasterisers are not asked to agree bit for
bit — the host paints shaded vertices through a per-triangle sort with
Skia's antialiasing, the device rasterises the same shading through a
depth buffer with none, and a blur is a box approximation on one side and
a Gaussian on the other. What is measured instead, per colour channel in
0..255 over every pixel, is the MEAN absolute difference (which says the
two are the same picture), the 99th PERCENTILE (which says the
disagreement is confined) and the WORST channel — which is an edge, or a
body a centroid sort ranked wrongly on the host and a depth buffer ranked
rightly on the device, and is reported rather than judged. Each study names its own mean and p99
ceilings in the script, set from what the two tiers do rather than from a
wish. With no device the tier reports that and exits green, because a
machine with no Vulkan runtime has nothing to disagree about.

`--tier promotion` is the third. Every render a hash judges is made with
automatic texture promotion held off, because a re-bake decided by a
measured per-frame cost is a thing load can tip either way and a
byte-identity gate has to be load-immune — so no other tier exercises
the promoter at all. This one renders each sketch twice on the CPU, once
with it off and once with it on, and differences the two: the held-off
plate is the reference, there is no baseline, and the bar is ONE CODE
VALUE anywhere. That bar is a consequence rather than a tolerance
somebody picked — a promoted node is baked under the live matrix
post-translated by an integer, and inverting that matrix to find a
shader's local coordinates does not cancel the integer to the last bit at
a scale whose reciprocal is inexact. A worst channel over one is a
picture that MOVED, and a defect to file rather than a plate to adopt.
**Its subject is not the studies.** Promotion is a 2D runtime's
re-baking, and the runtime a study draws through promotes nothing — so
each study renders the same bytes with the promoter on and off, and
`--tier promotion --kind set` asks a question this library does not
answer.
