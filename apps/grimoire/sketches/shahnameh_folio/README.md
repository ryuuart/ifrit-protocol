# The First Fire — illuminated manuscript study

An original **1650 × 2340** native folio in **Study · Manuscripts**. Its
miniature shows a royal gathering around a ceremonial fire, with standing
attendants, a musician, a reader, seated courtiers, three deer, embroidered
carpets, vessels, fruit dishes, flowering trees and mineral crags. Lapis and
gold borders, varied floral motifs, fine ink, painted cloth, four Persian
verse columns and a fibrous paper ground make the folio a comprehensive
brush, typography and retained-composition study.

## Reference and authored interpretation

The primary reference is [The Metropolitan Museum of Art's *The Feast of
Sada*, folio 22v from the Shahnameh of Shah Tahmasp](https://www.metmuseum.org/art/collection/search/452111),
painted around 1525 in Tabriz and attributed to Sultan Muhammad. The museum
records opaque watercolor, ink, silver and gold on paper and identifies the
object as public domain. Its [primary museum image](https://collectionapi.metmuseum.org/api/collection/v1/iiif/452111/865998/main-image)
was visually inspected alongside native renders.

The source contributes the oval hierarchy of king, courtiers and animals,
the red and gold fire, cool sky, colored mineral formations extending into
the paper margins, delicate foliage, gold-speckled paper and four ruled text
columns. The figures, miniature geometry, ornament, palettes and caption
here are newly authored. The broad lapis border and title panel are original
additions. This is an interpretation, not a facsimile or a transcription of
the museum folio's inscriptions.

## Text and script

The lower columns contain the first **eight couplets**, sixteen hemistiches,
of Ferdowsi's public-domain Shahnameh, checked against [Ganjoor's published
opening](https://ganjoor.net/ferdousi/shahname/aghaz/sh1/). They read across
four columns from right to left, then down. These opening verses differ
from the passage surrounding the museum's Feast of Sada miniature. The
publisher's punctuation and diacritics are retained; this does not claim to
establish a critical edition.

The title is `شاهنامه`. The cartouche's `جشن آتش` means “fire celebration”
and is an original caption. A native RichText footer combines Persian,
English and Persian digits in logical Unicode order. Text is shaped by the
native paragraph engine, without reversal or painted substitute glyphs.

The installed **Noto Nastaliq Urdu** family supplies the title, cartouche and
verses; its collection is `/System/Library/Fonts/NotoNastaliq.ttc`. **Geeza
Pro** supplies the mixed footer and **Baskerville** the English captions.
The title uses the existing fixed first-baseline control. The verses share
one size selected by native intrinsic measurement of all sixteen lines, so
longer hemistiches fit their narrow columns while preserving a consistent
reading rhythm. The native font size values are expressed in the folio's
1100 × 1560 author space; the retained root scales it by 1.5 for output.

## Native drawing and craft

The miniature uses closed Bezier outlines, native pressure-bearing brush
splines and polygon pigment washes. Gathered garment folds radiate from
creased cloth instead of following a repeated grid. Coats vary their floral,
leaf and curled brocade motifs; washes and fine fibers give the colored
cloth restrained pigment variation. Faces vary profile proportions, noses,
skin colors, moustaches, beard length and head inclination. Turbans have
individual wrap lines, trailing cloth and a smaller silhouette; the royal
cap carries engraved bands, jewels and a feather.

Crags use overlapping ledges, branching veins, rifts, curling contours and
matte washes. Continuous fine ink joins those forms into a mineral surface.
Cypresses have slender continuous silhouettes with lanceolate leaves over
small branches. Flowering trees use tapered pressure strokes for trunks,
roots and twigs. The deer have staggered bent legs, hooves, fine fur marks,
profile features and branching antlers. Asymmetric S-shaped fire tongues
turn back into their plumes, with smaller detached curls over painted logs.

The frame alternates flowers, split palmettes and paisley forms over gilded
arabesques. Fine secondary stems, leaves, stipple, inner rules and rosettes
provide detail at several scales. Shape-fitted native Paint gradients dress
the foil marks; the pigment and miniature use matte colors and native
washes. All random drawing uses fixed seeds.

## Retention and motion

`setup` describes one retained native Compose tree. Paper, border, headings,
illumination, miniature and ruling are keyed Pen pictures. Native
Compose/Weave text stands above those pictures. The whole opaque printed
page is held in one explicitly declared Texture cache. A small ember picture
is a sibling outside that cache, where native opacity and translation
animations lift and dim its sparks. Static art is authored once rather than
repainted by a sketch update loop. The scene contains no raster art assets.

An explicitly declared Picture retains commands but still replays them.
This known opaque folio is therefore an appropriate place to declare its
Texture boundary. The texture combines source-over artwork and text; the
independent ember overlay keeps the page static during animation. Cache
choice is an authoring observation, not a library defect.

## Run and evidence

From `apps/spell-circle-canvas`:

```sh
UV_CACHE_DIR=/private/tmp/sigil-study-uv-cache \
  build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire \
  ../grimoire/sketches/shahnameh_folio/shahnameh_folio.py \
  --frame /private/tmp/shahnameh-redo-final.png --at 3 \
  --state /private/tmp/shahnameh-redo-python-state
```

Use `--at 0` for the spark overlay's other state. Open the same entry without
`--frame` for native live authoring. The file-frame lane is raster even if
`--gpu` initializes a device executor. The separate `--window-bench` and
`--shot` checks identify their renderer as Graphite GPU and exercise the
actual native window. Captures, logs, control source, brush probes and the
benchmark ledger are preserved in `build/media-study-redo/shahnameh_folio/`.

## Authoring findings

Native Persian joining, diacritics, narrow columns and mixed runs render.
The existing first-baseline and intrinsic-measurement APIs resolve placement
and line fitting. Pen rotation defaults to radians while Compose rotation
uses degrees; the painting callbacks set Pen's angle mode explicitly. That
unit mistake was corrected in the authoring, rather than reported as a
library defect.

Fine nib splines are a separate coverage issue. A native raster probe uses
widths below one pixel, dense spacing and no scatter or jitter. The nib
strokes become dotted while the same-width Pen curve remains continuous.
Changing the existing tool to one fibre produces continuous pressure ink.
The nib routes through `Stamps.cpp` to the sprite batch in
`sigilskia/draw/Direct.h`; that lowering sets paint antialiasing and calls
Skia's vertex renderer. Skia's installed `SkCanvas.h` explicitly states that
vertex drawing ignores paint antialiasing. The coverage intent therefore
requires a supported lowering rather than that flag. A regression should
render thin sprites and dense nib strokes at several subpixel translations
on raster and Graphite and verify smooth, nonvanishing coverage along the
run. The study uses the existing fibre tip to author its fine ink.

The clipping binding invokes a zero-argument callback, while the generated
Python declaration says the callback receives a Pen. The study closes over
the existing Pen to match runtime behavior. The checked binding is
`src/common/python/draw/Pen.cpp` at its `shape()` invocation; the declaration
is supplied by `apps/python/sigil/typing/refinements/pen.py`. A regression
should type-check the accepted callback form and render contained mineral
ink, including inverted clipping and restoration of the outer clip.

The Unicode primitive supports an explicit bidi base direction, but
Paragraph analysis calls it with automatic direction. A numeric folio label
followed by an English title can adopt a different base from its Persian
page. Passing the existing direction value through a paragraph partial
would avoid inserting control characters into authored text.
`ParagraphAnalysis.cpp` calls the Unicode primitive without that argument;
the checked Paragraph, ParagraphBlock, ParagraphStyle and LayoutOptions
declarations do not carry it. Native analysis of `۱۵۲۵ · Folio 22v` adopts
level zero with automatic direction but returns mixed levels with explicit
RTL. A regression should compare Paragraph analysis with explicit Unicode
analysis and invalidate cached analysis when the base direction changes.
