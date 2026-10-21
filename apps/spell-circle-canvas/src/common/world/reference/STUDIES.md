# SigilWorld — the studies and their plates

The chapter on the twelve 3D frames this library is looked at
through, and on the ledger that judges them: what each study draws, the
presets they are built out of, and how the plate ledger's device tier
holds each to a picture of its own. `README.md` beside the library is
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
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
    --headless <outdir> --kind scene3d [--sketch <name>] --gpu
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire --list --kind scene3d
```

`--sketch` takes a case-insensitive substring, which is the loop for
visual iteration. A study joins the registry by being a file in
`apps/grimoire/sketches/`.

A study is drawn on the device, so it needs `--gpu`. A study that
declared no passes is wrapped in one geometry pass clearing to its
background, because an executor is only reached through passes. On a
machine with no Vulkan runtime `--gpu` reports that and fails, and a run
without it lists every study unavailable, saying a scene3d sketch is drawn on a
device. The device is brought up by the BINARY and installed once for the
process, so no feature here but `diligent/` links one.

Fifteen sketches draw through the scene3d runtime, and between them they
exercise every feature this library has:

- **`first_light`** — the scene: a tube swept along a closed loop, a
  comet of stamps riding a moving window of that same loop, a plate under
  both, a sun and a lamp, and a camera on a rail of its own.
- **`glow_trail`** — the passes. Its scene is drawn once, and what is
  tagged "glow" is then reached three ways, one per realisation: a
  narrowed post pass lifts the beads in place through the coverage the
  geometry pass before it was made to write; a narrowed geometry pass
  draws the same beads alone into a target of their own, which is
  softened and dimmed; and that is laid over its own output from the
  frame before, so the comet drags a tail no single frame contains. Six
  passes, no stated order, and three of its surfaces are taken in turns.
- **`surface_lab`** — World's one surface model, under a rig and under a
  sky. A back row of five curved cards, each told something different —
  a colour, a STACK of two through a noise mask, a normal differentiated
  from a height field, roughness and metallic from one packed image read
  at two channels, and an emission map — and a front row that is legible
  only because of the environment: a mirror, a rough metal, a dielectric,
  glass, and a flat chrome plate whose bevel is a relief its page paints
  from the outline's coverage. Every body is the same metallic-roughness
  surface; what differs is what it is told and what light reaches it. The
  frame's environment, a studio crossfaded toward a sunset that the kit's
  environment maker bakes, turns by its `rotation`, turning every
  reflection while the lights and bodies stand still. The turntable is
  PARKED: a lab is read rather than watched. Every map is generated, so
  the plate is a function of the declaration.
- **`scene_surfaces`** — a compose scene as an ordinary texture, and
  every sampling dial applied to it. Three flat cards on an arc, one
  curved band under them and one swept ribbon whose card repeats along
  the band's length each wear a compose tree rendered by a composer of
  its own; the screens are unlit, so what they show is what the trees
  painted, and the ribbon is a lit surface, so the same texture is read
  through shading beside them.
- **`set_stagger`** — the entrances of a scene's children, staggered, and
  the collection that addresses a subtree afterwards. Two rows differ
  only in their stagger's origin, so at one moment they hold different
  shapes of the same run; a `Collection` naming one row's subtree by its
  key narrows a pass to it.
- **`dart_flight`** — `along()`, and nothing else. One winding closed
  loop swept into a rail, a dart flying it at a distance that is a
  function of the scene time, and gates standing on the same loop at
  constant distances and rolled about it — one verb serving a moving
  body and a still one, and composing with the rotation lanes rather
  than replacing them.
- **`deformed_cloud`** — the point operators, in a room. A band across a
  scattered body is selected once and addressed twice: the points inside
  it are pushed out along their own normals, and inverting the same
  region turns everything outside it about the up axis. The chain is a
  value the node carries and the frame's runtime cooks it.
- **`lantern_room`** — the three lights together. Four unlit lantern
  shells each with a coloured point light, a spot opening downward onto
  the cluster between them, and a directional light faint enough to be an
  outline. A light stands where its node stands and carries no geometry,
  so a lantern here is two siblings sharing a placement. The amber lamp's
  strength and colour are bound to live values: nothing about its
  description changes from frame to frame, and what moves is what the
  light's values are bound to.
- **`compute_variant`**, **`import_native`**, **`vagrant_story_target`**
  and **`world_hud`** — the pass verbs that draw nothing by themselves
  (cooking points, re-drawing a selection, asking for a resource back),
  the zero-copy import door, and two studies that hang a compose overlay
  on an unlit quad filling the frustum over a lit scene.

Most of them are built out of `kit/`: `kit::threePoint` puts three point
lights round a subject in its own extents, with the key's strength and
colour bindable, `kit::turntable`
rides a closed rail looking inward, and `kit::litSet` is both over a
ground plane. Every one returns an ordinary node, which is why
`lantern_room`, which lights its own room, takes the turntable alone and
leaves the rest of the preset behind rather than describing a second room
over the top of its own.

A study returns a `Frame`, and any node is one with no passes, so a study
about the scene says nothing about passes at all. The host writes the
plate's size and its camera into whichever it was handed.

## The plate ledger over the studies

A study is a sketch of the `scene3d` kind, so it is judged by the one plate
ledger over the one registry, narrowed to that kind. It is drawn on the
device and nowhere else, so its tier is the DEVICE tier: every study is
rendered on the device to its declared moment and its bytes hashed
against a manifest of the device plates this machine keeps,
`build/plate_baseline_<config>_device.sha256`, the same manifest every
scene2d sketch is judged by: did any byte move that I did not mean to
move, asked of the one rasteriser a study has, on the machine in front of
you:

```sh
python3 scripts/sigil.py plates --kind scene3d --tier device --rebase  # adopt
python3 scripts/sigil.py plates --kind scene3d --tier device           # judge
python3 scripts/sigil.py plates --kind scene3d --tier device --stability 2
```

A sweep narrowed to one kind or one study merges into that manifest
rather than truncating it. The CPU smoke passes every study over and
says how many it passed over: a study has no raster plate. With no device the device tier reports that and exits green, because a
machine with no Vulkan runtime has nothing to judge a study with.
