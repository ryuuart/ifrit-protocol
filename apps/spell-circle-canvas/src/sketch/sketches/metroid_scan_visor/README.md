# Metroid scan visor study

A 1440 × 960 biological research scene inside a helmet. The study uses
Metroid Prime's scan-visor visual grammar: icy etched corner brackets,
visible helmet hardware, an amber scan target, projected anatomical
panels and a lower acquired-data window. The room, anatomical drawings,
labels and specimen placement are an original composition. The prose
and numerical readings are authored for this study.

## Visual references

- [Jon Wofford: Metroid Prime Remastered — HUD & Visors](https://waffledoodle.artstation.com/projects/g0Y4ve).
  The UI artist's scan and completed-scan images were inspected directly.
  The study carries the cool optics, rectangular field, separate anatomy
  panels and framed research readout. The source's completed scan shows
  an unknown mutated creature; this study examines a larval Metroid.
- [Nintendo: Metroid Prime Trilogy manual](https://csassets.nintendo.com/noaext/image/private/t_KA_PDF/Wii_Metroid_Prime_Trilogy?_a=DATC1RAAZAA0).
  The Scan Visor section establishes orange versus red target semantics,
  an aiming cursor and scan data stored in the logbook.

No reference bitmap is used in the scene. The downloaded research images
remain outside the sketch directory. The organism, room and visor are
native closed paths, stroke paths and shaped gradients.

## Craft

The specimen's semitransparent bell combines a shape-fitted radial ramp
with curved latitude and meridian traces. Fine branching veins, cellular
ellipses and warm energy nuclei sit under a bell-shaped clip. Four
closed Bezier teeth use individual surface ramps and engraved contours.
A separate wireframe rendering reuses the same anatomical construction
on the left research panel.

The laboratory establishes depth with symmetrical wall ribs, converging
floor seams, suspended cable curves, tank rails and repeat terminal
drawings. Cold cyan lights and weak caustic interference make the tank
read as glass without flooding the labels. The visor's hardware and
optical brackets have different line weights and transparency so their
physical and projected layers remain distinguishable.

Static pen drawings are retained pictures; the chamfered research panel
grounds are texture caches and use SigilGeometry's stock chamfered
silhouette through the pen's shape verb. A single engine timer publishes seconds.
Bindings derive restrained parallax, biological buoyancy, target-marker
pulsing and the repeating scan sweep from that value. A scan gauge fills
over 3.3 seconds, then the status changes to complete and the acquired-data
window appears. Both passages are retained with complementary opacity
bindings. The only immediate
program is the narrow live spectral trace. `fluid.sksl` is a small live
caustic material within the containment tank. The setup tree is declared
once and does not repaint static art through `update()`.

The intended still is at 4.2 seconds. The scene also remains active under
the live host so drift, spectrometry and the scan sweep can be inspected.
The interface requests Eurostile, then DIN Alternate and Helvetica Neue;
the compact instrument labels use Menlo or Monaco. These are accepted
stand-ins for the game's own typography, not a claim of font identity.

## Authoring pressure

- Shape-fitted pen fills accept `material::Paint`, while the convenient
  gradient factories return `material::Material`. This study uses
  `material::Paint::radialGradient` and `linearGradient` for the specimen
  ramps. A material assembled from layers requires `Paint::recipe` before
  it can be passed with `SHAPE`; the Material fill overload alone does not
  accept that fit parameter.
- Pen programs accept their pen as a callback argument, while `Pen::clip`
  invokes its mask callable with no argument. The clip here captures the
  existing pen and invokes the shared anatomical outline through it.
- A world-anchored material has no Compose root transform or root extent
  in the pen's `draw::Frame`. The pen's material frame consequently uses
  its local box and an identity transform; its fitted shader adds only
  the local shape-box translation. A root-continuous scan grid would need
  that information to cross from Compose into the pen. This study keeps
  the optic sweep in a Compose layer and fits the anatomical ramps locally.
- A scanned creature naturally wants its bright contour, optical sweep,
  local glows, labels and side-view anatomy to share semantic identity.
  The current composition expresses those as separate retained Elements
  and a shared native drawing function. That is workable, but requires
  authors to keep the anatomical coordinates and leader coordinates in
  sync manually.

The first two points are source-checked API ergonomics requests. The
world-space point was reproduced through the installed host's loaded
Python binding: one root-anchored Material stays continuous across
translated Compose fills but restarts its entire ramp inside each pen
leaf. The source omission is still present in the current pen seam.
The rebuilt host also reproduced the same result through a fresh native
C++ entry using a world-anchored Paint wrapped as a Material. A separate
probe found that anchoring the convenient gradient Material itself drops
that flag when Skia folds its base: the material-level flag restarts the
ramp per Compose box, while its Paint-level counterpart stays continuous.
Render verification should inspect gradient
fit on each curved tooth, clip containment at the bell edge, label
readability, the moving sweep's clipping and retained motion after the
initial still.

## Run

From `apps/spell-circle-canvas` with a current Sketchbook host:

```sh
build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/metroid_scan_visor/metroid_scan_visor.cpp \
  --frame /private/tmp/metroid_scan_visor-4.2.png --at 4.2 \
  --state /private/tmp/metroid_scan_visor-state
```

Open that same entry without `--frame` for live editing. The scene
requires no external assets or GPU backend for its still.

## Evidence

The Release syntax-only check passed using the exact consumer compile
flags after correcting fitted Material calls and the mask callback to
the current native APIs. The matching native host compiled the live
entry and rendered successful stills at 0.8, 4.2 and 7 seconds. All three
were inspected against the primary scan-visor reference. The biological
signature label was moved below the energy number after the first render
exposed a collision. The curved-tooth gradient fit, bell-mask containment,
scan progress, acquired-data reveal, moving trace and later buoyancy are
visible in these native captures:

- `/private/tmp/metroid-scan-visor-scanning.png`
- `/private/tmp/metroid-scan-visor-complete.png`
- `/private/tmp/metroid-scan-visor-late.png`

These file `--frame` stills use the raster 2D capture lane. A targeted
registered GPU sweep could not select the new entry because the host's
compiled registry did not include it. Graphite execution of this study
is therefore not claimed by these captures. A separate bounded file-window
run successfully selected this exact source, reported `renderer: Graphite
GPU`, and completed without shader errors. Its app-window image was inspected
for the translucent specimen, caustics, optics and typography. This route
does not require the source to be in the compiled registry.

Separate root-space probes rendered successfully through the loaded
native Python runtime, at `/private/tmp/metroid-worldspace-probe.png` and
`/private/tmp/metroid-worldspace-width-probe.png`. The second probe uses
180-pixel and 420-pixel local panels on an 800-pixel root. Both pen panels
still stretch the full root ramp to their own width. These probes are
evidence for the authoring limitation, not captures of the visor study.

Fresh native C++ probes compiled and rendered through the matching host:

- `/private/tmp/metroid-worldspace-current-cpp.png` compares the same
  root-anchored Paint wrapped as a Material in Compose fills and Pen fills.
  The Compose row is continuous; the Pen row restarts in each local panel.
- `/private/tmp/metroid-material-worldspace-current-cpp.png` compares
  `Material::worldSpace` with an anchored Paint wrapped as a Material in
  ordinary Compose fills. Skia lowering loses the Material-level flag.
  The copied base's Paint-level flag survives. The lowering and volatility
  classification were source-checked; the findings queue records the
  reproduction and regression expectation.
