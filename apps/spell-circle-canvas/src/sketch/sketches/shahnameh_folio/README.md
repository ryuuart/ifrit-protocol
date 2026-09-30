# The First Fire — illuminated manuscript study

An original 1100 × 1560 native folio in the **Study · Manuscripts** family.
Its miniature shows a royal gathering around a ceremonial fire, with
folded garments, embroidered carpets, long-necked vessels, fruit dishes,
deer, flowering trees and mineral crags. A lapis-and-gold ornamental frame,
eight-point rosettes, gilded arabesques, ruled text columns and a
speckled paper ground make this a substantial typography and drawing study.

## Visual source and authored interpretation

The primary reference is [The Metropolitan Museum of Art's *The Feast of
Sada*, folio 22v from the Shahnameh of Shah Tahmasp](https://www.metmuseum.org/art/collection/search/452111),
painted around 1525 in Tabriz, attributed to Sultan Muhammad. The museum
records opaque watercolor, ink, silver and gold on paper, and identifies
the object as public domain. Its
[primary museum image](https://collectionapi.metmuseum.org/api/collection/v1/iiif/452111/865998/main-image)
was visually inspected before drawing.

The source contributes the oval hierarchy of king, courtiers and animals;
the small red-and-gold fire; cool sky; colored mineral formations that
spill into paper margins; gold-speckled paper; and four ruled text columns.
The miniature, people, ornament, palettes and inscriptions here are newly
authored. The broad lapis border and title panel are original additions.
The figure treatment is deliberately stylized. This is an interpretation,
not a facsimile or a transcription of folio 22v.

## Text and script

The four bottom columns contain only the two opening couplets of Ferdowsi's
public-domain Shahnameh, in logical Unicode order, read across columns from
right to left. The short text was checked against
[Ganjoor's opening of the Shahnameh](https://ganjoor.net/ferdousi/shahname/aghaz/sh1/):

> به نام خداوندِ جان و خرد
>
> کز این برتر، اندیشه، بر نگذرد
>
> خداوندِ نام و خداوندِ جای
>
> خداوندِ روزی‌دِهِ رهنمای

These are opening verses, not the text surrounding the museum's Feast of
Sada miniature. The title reads `شاهنامه`; the small cartouche's `جشن آتش`
means “fire celebration” and is an original caption, not a canonical verse.
A mixed-run footer combines Persian, English and Persian digits in one
native RichText passage. No text is reversed or painted as a substitute
for native shaping.

The installed family **Noto Nastaliq Urdu** supplies the title, cartouche
and verses at 52, 28 and 25 pixels. Its system collection is
`/System/Library/Fonts/NotoNastaliq.ttc`. **Geeza Pro** supplies the mixed
footer, and **Baskerville** the English captions. A native font probe
confirmed the requested families visually; nonexistent alternate names
were rejected by the font context. The large title uses the existing
`textFirstBaseline(Fixed, 58)` control to seat its ink in the ornamental
panel, since the face's ascent leaves much more air above the ink than a
Latin display face.

## Native craft and retention

`setup` describes one retained Compose tree. Paper, border, headings,
miniature and ruling occupy separately keyed native Pen pictures. They
are painted once and retained. Text remains native Compose/Weave text.
The complete static printed page, including those pictures and its text,
is held in one explicitly declared Texture cache. The animated ember
picture stands outside that cache as a sibling. The page covers its own
opaque ground and all its drawing uses normal source-over compositing,
so baking it together preserves the compositional reading of its marks.
There is no sketch update loop repainting the folio and no raster art asset.

The miniature uses closed Bezier contours for mineral outcrops, coats,
folded knees, faces, fire tongues and vessels. A second group of wavy
mineral bands and short curling marks is clipped to each rock silhouette.
Carpet flowers, coat embroidery, leaf veins, fringe, facial marks,
antlers and gilded linework stress very small paths and strokes. Paper
fibers and mineral stipple use fixed seeds. Repeated rosettes combine a
lapis outline and a shape-fitted foil gradient. Native typography stays
above the painting and ruling pictures in the retained stack.

Only a small ember picture moves. Native opacity and translation
animations alternately lift and dim its sparks; the printed folio stays
still. The declared still is at 3 seconds.

## Run and verified captures

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Sketchbook.app/Contents/MacOS/Sketchbook \
  src/sketch/sketches/shahnameh_folio/shahnameh_folio.py \
  --frame /private/tmp/shahnameh-folio-cpu.png \
  --state /private/tmp/shahnameh-folio-state
```

Append `--at 0` to inspect the spark animation's starting state. Appending
`--gpu` to this file capture initializes the device executor but the
host's file `--frame` lane photographs the 2D canvas on raster. It is a
GPU-enabled capture and device smoke test, not evidence of Graphite
execution for the folio's material or text rendering.

The final CPU capture and elevated GPU-enabled capture both completed
successfully and were inspected. Their PNGs are byte-identical:

- `/private/tmp/shahnameh-folio-cpu.png`
- `/private/tmp/shahnameh-folio-gpu.png`

The zero-second control is `/private/tmp/shahnameh-folio-zero.png`.
Comparing it with the 3-second still changed 218 pixels in the small
spark region, x 501–591 / y 749–810. The rest of the paper, miniature,
ornament and text stayed identical. The font and baseline controls were
also rendered separately. These captures used the existing host's
compiled Python bindings; no library source was changed for the study.

The Texture-cache refinement also produced byte-identical stills at both
zero and three seconds against the preserved Picture-cache controls. Its
ember-only difference between those moments remains the same. The
sequential window check identified its renderer as Graphite GPU. Control
sources, images and logs, together with the measured cache comparison,
are kept in `build/media-study-evidence/shahnameh_cache_benchmark.md`.

## Authoring pressure

Native Persian joining, diacritics, contextual forms, multi-line narrow
columns and mixed runs all rendered. The existing baseline control
resolved the large-title placement; it is not an absent-API finding.

A retained Picture avoids re-authoring its paths but still replays those
paths on the GPU. Pen callbacks are opaque to automatic backdrop analysis,
and an explicitly declared Picture also refuses texture promotion. A
known opaque page is therefore an appropriate place to declare a Texture
cache explicitly. The static page and live sparks form the cache boundary;
this is an authoring and cache-choice observation, not a library defect.

The clipping binding invokes a zero-argument callback, while the generated
Python declaration says the callback receives a Pen. The study uses a
closure over the existing Pen to match runtime behavior. This extends the
same clip-callback inconsistency observed while authoring the scan visor.
The checked source is `src/common/python/draw/Pen.cpp` at the `shape()`
invocation, and `apps/python/sigil/typing/refinements/pen.py` supplies the
incompatible `DrawCallback` annotation. A regression should type-check the
callback form accepted by the binding and render its contained mineral
engraving, including inverted clipping and restoration of the outer clip.

The Unicode primitive already supports explicit bidi base direction, but
the Paragraph analysis path calls it with automatic direction. A numeric
folio label followed by an English title can therefore adopt a different
base direction from its Persian page. Passing the existing base-direction
value through a paragraph partial would avoid inserting directional
control characters into authored content. This is an API request, not a
claim that the rendered Persian passages failed.
`ParagraphAnalysis.cpp` invokes the Unicode primitive without its
base-direction argument; the checked Paragraph, ParagraphBlock,
ParagraphStyle and LayoutOptions declarations do not pass one. The lower
Unicode API's existing BaseDirection value is the natural field to carry
through. For `۱۵۲۵ · Folio 22v`, native Unicode analysis returns one
level-zero run under automatic direction, but explicit RTL yields the
expected mixed embedding levels. A regression should compare Paragraph
analysis with that explicit Unicode analysis and invalidate its cached
analysis when the authored base direction changes.
