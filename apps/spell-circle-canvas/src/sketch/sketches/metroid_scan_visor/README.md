# Metroid scan visor

The current entry is `metroid_scan_visor.py`: a 1440 × 960 native biological
research scene. The visor presents an original larval Metroid interpretation
inside a laboratory, with anatomical comparison panels and an acquired-data
window. The biological prose, readings and arrangement are authored. The
preceding C++ composition is preserved as a control;
its source and stills are stored under
`build/media-study-redo/metroid_scan_visor/`.

## Reference and comparison

[Jon Wofford's Metroid Prime Remastered HUD & Visors](https://waffledoodle.artstation.com/projects/g0Y4ve)
is the primary visual reference. Both his scanning and completed-scan images
were inspected directly. They establish the curved physical rim with venting
and luminous inserts; four floating, etched corner pieces; the world visible
through the optics; the energy readout suspended below the rim; the tilted
anatomical image panels; and a separate lower research window. The source's
completed screen examines a mutated creature. This study's translucent larval
organism is an original subject interpretation.

[Nintendo's Metroid Prime Trilogy manual](https://csassets.nintendo.com/noaext/image/private/t_KA_PDF/Wii_Metroid_Prime_Trilogy?_a=DATC1RAAZAA0)
provides the scan target, cursor and logbook semantics.

The three benchmark sketches were read and their native controls inspected.
The relevant craft is the seal's material relief, the rain's coherent lighting
and depth, and the bench's illuminated physical setting. The visor develops
those concerns through optical volume integration, normal-based solid shading,
retained diagnostic views and a perspective laboratory. No reference bitmap
or imported illustration is used in the rendered scene.

## Native craft and dimensionality

`specimen.sksl` integrates an optical ray through three-dimensional implicit
anatomy. Its semitransparent ellipsoidal bell has an undulating skirt, fine
surface perturbation, Fresnel edge scattering and a sharp key-light glint.
Front-to-back attenuation composites four displaced and folded red energy
nuclei, internal branching fibers and thin collagen lamellae. Four tapered,
curving mandibular teeth are separate solid distance fields: their finite-
difference normals provide diffuse and specular shading, with faint chitin
striation. Diagnostic mode exposes the membrane's lamellae and warms the
nuclei. The same anatomy is rendered at different bearings for the large
morphology panel and three small comparison views.

The shader models absorption, scattering and surface sheen. It does not
trace refracted background rays, implement physical multiple scattering or
render through SigilWorld's mesh/depth-buffer executor. The organism uses an
orthographic optical ray. `laboratory.sksl` uses perspective rays intersecting
planes and equipment boxes, shaded metal seams, vents, indicator strips,
floor illumination and haze. These are native Material programs drawn by
the 2D renderer. The curved tank, conduits, blower assemblies, consoles and
visor are retained Pen paths. The UI panels are retained 2D Compose elements
projected through the Compose perspective transform.

Shape-fitted gradients give the helmet and tank rims local metal ramps.
Vent slits, screws, small scratches and precise nested outlines distinguish
physical hardware from the etched optics. Fans use individually oriented
blade silhouettes, radial ramps and fasteners. The consoles contain small
native diagrams at several depths. Pen callbacks set their angle mode to
degrees explicitly, matching the authored console inclinations. Cool environmental lighting leaves amber
reserved for acquisition and anatomical identification.

The anatomy, room and dense vector grounds use explicit texture caches.
The optical material itself has no clock input, so its ray integration is
baked once. The retained specimen moves by bounded rotation and buoyancy;
the background has restrained parallax. `fluid.sksl` is the small live
caustic layer. A clipped retained scan band crosses the selected field during
acquisition. At 3.3 seconds the gauge completes, the status changes and the
research window fades in. The sweep then disappears. Static art is never
re-authored in an `update` or `draw` callback.

The Python entry uses the existing native `shader(hub, ctx.local(...),
parameters)` resource API. The hub owns the shader resources and diagnoses
invalid text; edits use the same live program mechanism as C++ authoring.
The parameter mapping declares `bearing` and `diagnostic` for each anatomical
instance. DIN Alternate and Menlo are verified local interface stand-ins;
they do not claim to be the game's original font.

## Authoring observations

The native Python shader, retained cache, perspective, clipped-overflow and
animation APIs express this scene without changing a library. The volume
look belongs to this study's material, rather than a reusable named library
look. Perspective room rays and orthographic anatomy rays are deliberately
separate projections; matching their authored light vectors is an explicit
responsibility. Native mesh cameras and world lights exist, but this study's
optical integration does not execute in that scene seam.

Pen shape fitting still requires the `Paint` plus `SHAPE` form. A gradient
in the pen's frame otherwise uses the entire leaf extent; a short helmet
rim then samples only a small part of its vertical ramp. Fitting each rim
was essential to its shaded physical reading. The earlier source-verified
Material-fit, callback typing, and root-space findings remain in the shared
backlog; this revision adds rendering pressure rather than assuming a new
absence from an unfamiliar API.

The SkSL runtime reports an integer modulo expression as unsupported. The
room's deterministic equipment index uses floating `mod` instead. This is a
shader dialect constraint, not a library defect.

## Run and evidence

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/metroid_scan_visor/metroid_scan_visor.py \
  --frame /private/tmp/metroid-redo-complete.png --at 4.2 \
  --state /private/tmp/metroid-redo-python-state
```

Use `--at 0.8` for acquisition and `--at 7` for later retained motion. Open
the same entry without `--frame` for live authoring. Raster native captures
are recorded under `build/media-study-redo/metroid_scan_visor/`; the original
still controls remain preserved there. A file `--frame` capture is the
raster 2D lane even when `--gpu` initializes the device. Actual Graphite
verification requires the serialized file-window lane.

The final completed and later native raster stills were inspected against the
Wofford reference and the preserved prior control. The tooth attachment now
fades under the membrane instead of presenting a hard cap. Warped collagen
threads replace the regular grid. The serialized native file-window check
reported Graphite GPU and completed successfully without shader errors; its
app screenshot was also inspected. The exact logs, app image and benchmark
ledger are in `build/media-study-redo/metroid_scan_visor/`.
