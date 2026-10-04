# The optical record

An original 38-second film study of document overprinting, crystalline
microscopy and uncertain observation, inspired by *The Andromeda Strain*.
It occupies a 2.35:1 frame. The specimen, scientific instrument section,
record text, response curves and sequence are authored fiction; they do
not reproduce an experimental procedure or claim measured data.

The opening places a readable white title over repeated red type,
oversized classification words, a blue optical section and a magenta
report heading. A full-width transmitted field replaces the documents.
A three-image comparison then preserves the specimen origin while
changing magnification and illumination. The final report holds an image
beside an unresolved observation.

| Scene time | Composition and motion |
| --- | --- |
| 0–7 seconds | Document overprints move beneath a held title. |
| 7–18 seconds | The field appears, the focal plane traverses depth, and magnification increases. |
| 18–31 seconds | Violet multiple illumination, green context and monochrome relief share specimen coordinates. |
| 31–38 seconds | An observation report holds the record, then fades through black before reset. |

## Observed reference and authored interpretation

The inspected title contact sheet contains repeated red titles, very large
cropped classification words, blue technical linework and magenta report
headings. The foreground type is white and typewriter-like. Courier is
the study's chosen approximation, not an identification of the original
face. The contact sheet establishes appearance, not timing or transition
direction. [Art of the Title](https://www.artofthetitle.com/title/the-andromeda-strain/)

The inspected crystal frame contains overlapping pale hexagonal outlines
in a violet-red field, with a soft diagonal band of blue light and varied
focus. The specimen frame combines a craggy clump, small green flecks and
copper mesh. The study develops the crystal optics; it does not recreate
the laboratory set or claim an exact frame reconstruction.
[Crystalline frame](https://theasc.com/wp-content/uploads/2026/04/Andromeda-Strain-Growth.jpg),
[specimen frame](https://theasc.com/wp-content/uploads/2026/04/Andromeda-Strain-Spore.jpg)

Douglas Trumbull describes an articulated plexiglass model photographed
through a microscope with strobe illumination and multiple exposures.
The study interprets that account through faceted procedural structure
and stepped exposure variation. Its motion is original, rather than a
measured reconstruction of the film's moving sequence.
[First-person production interview](https://vfxvoice.com/douglas-trumbull-ves-advancing-new-technologies-for-the-future-of-film/)

The contemporary production account describes rear-projected laboratory
images and a subdued photographic finish. That motivates restrained
halation, imperfect focus and a common film transfer, while the native
type and rulers remain outside the specimen's optical blur.
[American Cinematographer](https://theasc.com/article/the-andromeda-strain-may-1971/)

## Native construction

- `andromeda_optical_lab.py` retains the composed document, typography,
  optical fields and report. One host elapsed clock drives all phase
  envelopes and uniforms. No image generator or rendered film frame is
  used as an asset.
- `micrograph.sksl` defines a common specimen coordinate system. Its
  visible modes and depth map evaluate the same geometry at the same
  time, magnification and center. Two projected sheets, varied facet
  radii, doubled walls, correlated voids and a diagonal light field give
  the mass depth. Articulation changes at a stepped exposure cadence;
  the rear sheet retains the preceding pose.
- The specimen's map feeds the native spatially varying blur with a
  finite six-pixel maximum. This blends three Gaussian levels; it is an
  image-space approximation, without physical aperture bokeh or
  occlusion-aware depth compositing.
- A native bloom filter contributes near halation. An enclosing clipped
  viewport contains the image; its rulers sit outside the optical filter.
- `film.sksl` modulates the completed exposure with luminance-dependent
  emulsion grain at a stepped 24-frame cadence. It is a transfer effect,
  not an accumulation buffer or motion blur.

The ruler bar tracks magnification in invented specimen units. The
response curves in the report are illustrative and have no physical
calibration. Depth, exposure and film grain are artistic approximations.
The geometry is a projected analytic construction, without ray-traced
refraction. The specimen clock holds after 26 seconds; the film transfer
continues moving over the held exposure.

## Render

From the application directory:

```sh
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/andromeda_optical_lab/andromeda_optical_lab.py \
  --frame /tmp/optical-record.png --at 24
```

Use scene times 4, 12, 24 and 34 for the four principal compositions.
The default capture is the simultaneous comparison at 24 seconds.

A complete movie can be assembled from the native frame lane:

```sh
mkdir -p /tmp/optical-record
build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/andromeda_optical_lab/andromeda_optical_lab.py \
  --frame /tmp/optical-record/frame.png --at 0 --frames 456 --fps 12 --scale 0.75
ffmpeg -framerate 12 -i /tmp/optical-record/frame_%04d.png \
  -c:v libx264 -crf 18 -pix_fmt yuv420p /tmp/optical-record.mp4
```

That file lane draws 2D canvases on Raster. A native Graphite window is
separate backend evidence. File photographs observe the requested time
plus the runtime's still step; capture comparisons must account for it.
