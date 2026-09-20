# SigilWeave — the type vocabulary

The chapter on how a passage is SET: the two halves a text style splits
into, the partial a call site writes, the relative length a size may be
stated in, the named classes a sheet registers, and the decorations and
passes a glyph run is dressed with. `README.md` beside the library is
the front page.

## The two halves, and why

The style vocabulary every other SigilWeave header speaks is one include
over its subjects. A `TextStyle` splits into two halves on purpose:

- `ShapingStyle` — typeface, size, letter spacing, language, OpenType
  features, vertical form. Baked into the shape-cache key: change any
  field and the words it covers are re-shaped.
- `PaintStyle` — an `SkPaint` foreground plus ordered glyph-paint passes
  behind and above it, and the line decorations. Resolved at draw time
  only: recolouring, animating a shader, or restyling effects never
  re-shapes and never relayouts.

The split is about who owns glyph advances, not about what is visible: a
change is paint-side only if it cannot move a glyph. One appearance
change escapes `PaintStyle` without re-shaping — an advance-invariant
variable-font axis driven through `ParagraphLayout::LiveVariations` at
draw time, which reuses the shaped positions precisely because the axis
leaves advances alone.

`TextStyle::variation` sets or replaces one variable-font axis, in place
when the axis is already present, so styles built by the same call
sequence share one varied-typeface memo entry. `TextStyle::weight` is the
`wght` axis, fluently — `style.weight(650)` — beside
`TextStyle::opticalSize` and `TextStyle::condense`. Every axis set this
way lands in `ShapingStyle::variations` and so participates in shaping
identity: animating one re-shapes the words it covers, which is required
for `wght` because it moves advances on most faces. To animate weight
without re-shaping, use a face with an advance-invariant axis (`GRAD` on
the faces that have it) and drive it at draw time through
`ParagraphLayout::LiveVariations` instead of setting it on the style.

`TextStyle` holds the two halves together; `Type` is the PARTIAL a call
site names a style's numbers in, every field optional, with `Length` for
a size stated against one it does not carry; `TypeSheet` is a base style
and the named partials over it. Attach styles to text through
`Paragraph` or `ParagraphBuilder`.

## ShapingStyle: what re-shapes

The subset of style that affects glyph selection and metrics. These
fields are baked into the shape-cache key, so changing any of them
re-shapes the words it covers. Everything that only affects how
already-positioned glyphs are painted belongs in `PaintStyle` instead and
never invalidates shaping.

`ShapingStyle::scaleX` is horizontal glyph condensation — CSS
`font-stretch` by transform: glyph shapes AND advances scale by this on
the x axis. It is how a face with no `wdth` axis is condensed or
extended. Letter spacing is NOT scaled, matching CSS. It is part of the
shape-cache key, and vertical text condenses glyph width only, never
column advance.

`ShapingStyle::wordSpacing` is extra pixels added to each word's
trailing-whitespace glue (CSS `word-spacing`). It is applied after the
whitespace is measured, so changing it re-derives words at pure
shape-cache-hit cost — it is compared for restyle detection but is NOT
part of the shape-cache key. Negative values shrink gaps; the glue is
floored at zero.

`ShapingStyle::languageTag` is the BCP-47 language used both for
language-sensitive font fallback and by HarfBuzz to select OpenType
language systems and localized (`locl`) substitutions. It is deliberately
part of the shape key: even when the resolved typeface is unchanged,
language can change its emitted glyphs. Bidi direction is analyzed
separately and does not come from this tag.

`ShapingStyle::variations` are design-space overrides applied to the
typeface — or the context default — before shaping, the ergonomic
alternative to pre-building a varied `SkTypeface` through
`SkFontArguments` yourself. Resolution goes through `FontContext`'s
memoized clone cache, so the varied face's `uniqueID` is a stable
shape-cache identity and HarfBuzz mirrors the same design position Skia
rasterizes. It is order-sensitive: `[{"wght",700},{"wdth",80}]` and its
permutation resolve to equivalent faces but occupy two memo entries, so
keep a consistent order at call sites.

`ShapingStyle::aliased` draws glyphs with HARD edges — no antialiasing.
It lives on the style because Skia takes glyph edging from the `SkFont`,
never from the paint: `paint.foreground.setAntiAlias(false)` is silently
ignored on text, so a caller has no other way to ask for aliased glyphs
while still going through shaping, bidi, fallback and flow geometry. It
selects a rasterisation, not a face: the outlines are unchanged and only
their coverage is thresholded, which is what small bitmap-era UI type
looks like. It is part of the shape-cache key, since the shaped run
carries the flag through to the `SkFont` used at draw time.

`ShapingStyle::opticalKerning` SETS EVERY PAIR AS TIGHT AS THIS FACE'S
OWN EVEN PAIR, by measuring the letters rather than by reading the
face's kerning table.

A face's kerning is a designer's table of pairs. This is the answer when
there is none, or when a line mixes faces that never met: each adjacent
pair's outlines are measured for the narrowest distance between them, and
the pair is closed — or opened — until that distance is the one the
face's own reference pair leaves. The face's table is switched off while
this is on, because the two are answers to the same question and a page
takes one of them.

IT IS AN APPROXIMATION. A designer kerns by judging the white between two
letters as an area and as a rhythm; this measures a distance in bands. A
pair a designer would have opened for legibility, and a pair whose white
is wide but shallow, both come out tighter here. What the library does
NOT decide is how tight type should be: the reference is the face's own
even pair, so a loose face stays loose. It reaches between the letters of
one word — two words are separated by a space, whose own width is the
setting's to spend.

`FontFeature` is one OpenType feature setting, e.g. `{"liga", 0}` to
disable ligatures. `FontVariation` is one variable-font axis override,
e.g. `{"wght", 650}`, applied to a style's typeface through
`FontContext::variedTypeface`, which memoizes the varied `SkTypeface`
clone so identical typeface-and-variations pairs share one instance — and
therefore one shape-cache identity.

`TextTransform` is the case transformation applied to a span's text just
before shaping (CSS `text-transform`). The shaped glyphs come from the
transformed text while the paragraph's stored text, edit ranges and query
results all remain untransformed — matching how browser engines treat the
property as a rendering effect, not an edit. Because the transformed text
is itself the shape-cache key text, "HELLO" typed directly and "hello"
with `TextTransform::kUppercase` share one cache entry. Case mapping is
locale-sensitive through `ShapingStyle::languageTag`, so Turkish
dotless-i works unprompted. Length-changing mappings (German ß → SS) make
per-character cluster indices within such a word approximate for
hit-testing; line breaking runs on the untransformed text.
`TextTransform::kCapitalize` titlecases only the first letter of each
word and leaves the rest of the word untouched — CSS semantics, not
ICU's lowercase-the-remainder title mapping.

`VerticalForm` is how a span behaves when its paragraph is laid out
vertically, and is ignored in horizontal paragraphs.
`VerticalForm::kTateChuYoko` (縦中横) is shaped horizontally and set
upright in the column, for short runs like two-digit numbers in vertical
prose.

## PaintStyle: what does not

Draw-time glyph appearance with explicit composition order. Underlays are
drawn in vector order — back to front — followed by
`PaintStyle::foreground`, then overlays in vector order. The default
style owns no vectors and remains exactly one glyph draw. Every added
layer costs one additional draw for its style and font bucket; blur and
image filters may add backend-specific work beyond that. Updating any
paint or shader through `Paragraph::setPaint` is visible to an existing
`ParagraphLayout`.

`PaintStyle::baselineShift` is how far this span's glyphs sit ABOVE their
line's baseline, in pixels — negative sinks them below it. Superscripts,
subscripts, an inline symbol lifted onto the x-height. It is placement
rather than shaping: the advances are the face's own either way, so a
shifted span costs no re-shape and shares every cache entry with an
unshifted one. Straight horizontal runs only — down a column the same
idea is a step ACROSS the axis, which is what `Decoration::offset`
already means there.

`PaintLayer` is one additional rendering of the positioned glyphs.
`PaintLayer::paint` is intentionally the complete `SkPaint` vocabulary
rather than a SigilWeave mirror of selected fields: callers may use
colours, animated shaders, strokes, mask, image and colour filters, path
effects, and custom blenders. `PaintLayer::offset` moves only this pass,
which makes shadows and displaced highlights cheap without a saved layer.

The paint is applied as configured, with one reading: a TRANSPARENT
colour on it means the foreground's colour — the pass is drawn in
whatever colour the text is set in, on its own stroke, blur and offset,
so a halo stated once stands under every colour. A pass that should draw
nothing is left out rather than set transparent.

`PaintLayer::material` is a SigilMaterial instance this pass shades with,
in place of the paint's own shader. It is held by pointer: the style
feature links no renderer, so the material is resolved at draw time
through the resolver the paint feature registers — a pass whose material
has no resolver draws with the paint alone. It compares by identity, like
every binding: two passes sharing one instance are one pass, and two
equal instances held separately are two.

## Decoration: the bands

One line decoration — underline, strikethrough, overline or highlight —
as band geometry plus a band fill, resolved with a run's paint at draw
time.

Decorations live on the paint side on purpose: adding, removing or
recolouring one never re-shapes and never relayouts, exactly like paint
layers. Thickness and position default to the font's own metrics — the
`SkFontMetrics` underline and strikeout values, with sensible fallbacks
when a face reports none — so the zero-argument spelling
`PaintStyle::addDecoration({})` is a correct underline.

By default a decoration spans the decorated range, not individual words:
contiguous same-style runs on a line merge into one continuous band that
also covers the glue between words (CSS behaviour — an underlined
sentence is one line, a highlight reads like one marker stroke).
Skip-ink breaks come only from glyph ink, never from word gaps.
`Decoration::Span::kPerWord` opts back into one band per word, for
spell-check squiggles and word chips.

`Decoration::Kind::kHighlight` is the background member of the family: a
full-text-height band, ascent to descent by default, drawn *beneath*
every glyph pass, so with the default range spanning it renders as a
continuous highlighter stroke behind the words and their gaps.

A decoration separates two concerns: *band geometry* — kind, span, side,
thickness, offset, ink skipping — and *band fill*. The fill has two
spellings: `Decoration::color` is the lightweight one, and
`Decoration::paint` is the full `SkPaint` vocabulary — shaders (a shader
swapped per frame goes through `Paragraph::setPaint` without relayout,
exactly like glyph paint), blend modes, mask filters. Glyphs and
decorations resolve their fills independently, so a shaded highlight
under plain-coloured text — or the reverse — needs no coordination
between the two. Multi-pass band effects compose the same way glyph
passes do: stack several decorations with the same geometry and different
fills.

**Scope.** Decorations render on straight runs, set either way. Down a
column the band turns with the type — an underline runs beside the column
on its right, an overline on its left, a strikethrough down the column
axis, and a highlight covers the whole em box — and it draws through the
glyphs' ink, because skip-ink intercepts are cut out of a horizontal band
window that a column's band is not. Transformed runs, on a path or on a
rotated interval, carry no band at all: it would have to follow the curve
they ride.

Which side of the run's axis a band takes is `Decoration::side`, and the
two writing modes disagree about the default: a column's underline stands
on the RIGHT, which is the side a vertical setting reads its emphasis
line on, where CSS's `auto` would put it on the left.
`Decoration::Side::kOpposite` is that other placement, and it is the same
swap along a line — an underline above the type, an overline below it.

An underline and an overline are one band on opposite sides of that axis
— below the line and above it, right of the column and left of it — so
taking the opposite side is taking the other one's anchor. A
strikethrough and a highlight are anchored ACROSS the type rather than
beside it and have no second side to take, so both ignore it. So does an
explicit `Decoration::offset`, which names the band's near edge outright
and leaves nothing to choose.

`Decoration::paint`, when set, is applied verbatim — nothing is
overridden, exactly like a `PaintLayer` wrapping a caller-configured
`SkPaint` — and it takes precedence over `Decoration::color`, whose
resolution rules, including the translucent highlight default, no longer
apply: alpha, anti-aliasing and everything else are the caller's. Band
geometry is untouched — the paint fills the same rect segments a plain
colour would, ink skipping included.

## Type: the partial

`Type` is a text style's parameters as a PARTIAL: every field optional,
so a call site states the two it changes and says nothing about the rest.
With the total an unset field falls back to — `initialType` — the two
merges (`merge` folds one partial into another, `overlay` resolves one
against what it inherits from), and the `TextStyle` a total builds
(`toTextStyle`, `textStyle`). `Type` decides nothing: there is no type
scale and no opinion about which face stands in for which, and a study's
decisions are its own.

It is two things at once, and they do not pull against each other.

A DESIGNATED-INIT AGGREGATE rather than a positional signature. Every
caller needs a face, a size and a colour, and then some subset of
tracking, condensation and variable-font axes — and which subset differs
per call site. A positional helper cannot grow another parameter without
breaking every existing call; an aggregate can.

```cpp
textStyle({.face = faceMono, .size = 10.5f, .color = kInk, .track = 1.2f})
```

AND A PARTIAL: a field nobody set is not a field set to a default. A
`Type` that names a weight and nothing else says "heavier, and the rest
as it was", which is what lets a style be stated once at the top of a
tree and adjusted at a leaf. `overlay` is the one step of that; the
values an unset field means, when nothing is left above it to inherit
from, are `initialType`'s.

A SIZE MAY BE RELATIVE: `em(0.75f)` of a size decided elsewhere, `rem(2)`
of the root's, `lh(1)` of the line's. It is resolved as it is overlaid,
where the number it is relative to is known, so a total `Type` always
carries pixels.

It carries everything a passage inherits: how it is shaped — face, size,
tracking, condensation, axes, features, language, kerning, word spacing,
case, vertical form, glyph edging — and how it is painted — the colour,
the paint's antialias and colour ladder, the line decorations, the passes
beneath and above the glyphs. What is NOT here is the foreground paint's
own state beyond its colour: a shader, a mask-filter blur, a blend mode.
Those are added to the RETURNED style at the call site, and a leaf that
carries them is set in a whole `TextStyle`.

### The fields that need saying

`Type::face` unset is the face inherited, or the `FontContext`'s default
family plus its fallback chain when nothing above names one. A STATED
NULL — `.face = nullptr`, or `defaultFace()` — is the default family
outright, whatever an ancestor named: the one way a passage under a faced
ancestor returns to the context's own family, as CSS's `font-family:
initial` is.

`Type::track` is tracking added after each cluster. Pixels are implicit,
so `.track = 1.2f` is that many; `em(-0.08f)` is a fraction of the SIZE
THE TYPE RESOLVES TO, resolved as it is overlaid, so one register can
state the tracking a face is set at and hold it at every size. It is NOT
per-mille: a reference that quotes tracking in per-mille is a thousandth
of an em.

`Type::weight` present and greater than 0 is a `wght` axis. Weight
changes advances, so it participates in shaping identity. Present and 0
is the face's own weight, stated: it overrides an inherited axis with
none. `Type::slant` present and non-zero is a `slnt` axis, negative
leaning right, per the OpenType sign.

`Type::color8` sends the colour to the paint through an 8-bit sRGB word —
Skia's `setColor(SkColor)` — instead of as float. It is NOT a no-op and
not an equivalent spelling: the round trip quantises each channel to one
of 256 values, and Skia climbs a byte back to float by multiplying by
1/255 where a hex colour divides by 255, which lands one ulp apart on 126
of the 256 byte values. A palette taken from a reference's own ARGB words
wants this ladder; a colour computed in float does not. A run or a span
restyle written as a partial takes the ladder from the partial itself:
one that names a colour under a `color8` base restates the flag to keep
the byte ladder, since the base's colour is already a number in its paint
and only the colour named here is sent through the byte.

`Type::variations` is anything else in design space — appended after
weight and slant, so the order is stable and two styles built the same
way share one varied-face memo entry. A merge replaces an axis already
present where it stands rather than appending a second setting of it.

`Type::features` is a SET LIST that REPLACES the inherited one whole, as
CSS's `font-feature-settings` does: a passage that wants the inherited
features and one more restates them. `Type::decorations`,
`Type::underlays` and `Type::overlays` replace the inherited list the
same way. A decoration that names no colour is drawn in the colour the
text is set in, so an underline stated once stands under every colour; a
pass whose paint colour is transparent is drawn in the colour the text is
set in, on its own stroke, blur and offset.

### Resolving one

`reshapes` answers whether the fields a partial sets change how a passage
is SHAPED — its face, size, tracking, condensation, axes, features,
language, kerning, word spacing, case, vertical form or glyph edging — as
against how it is painted, which its colour, the paint's antialias and
ladder, and the decorations and passes change. A consumer that can
repaint a range without re-shaping it asks this first.

`initialType` is EVERY FIELD ENGAGED, WITH THE VALUE AN UNSET ONE MEANS
when nothing is left above it to inherit from: a null face (the font
context's default family), 16 px, opaque black, no tracking, no
condensation, no weight and no slant axis, antialiased glyphs on an
antialiased paint, the float colour ladder rather than the 8-bit one, no
language, no features, the face's own kerning, no word spacing, the
text's own case, the column's automatic form, no decorations and no
passes. It is the root of a cascade: a partial overlaid on it is a total.

`merge` FOLDS one partial INTO another — a pure field copy, which is how
two partials written about the same text become one. Every field the
overlay sets replaces the target's; a field it leaves unset leaves the
target's alone. Variations are appended, an axis already present replaced
where it stands, so the order is stable. A RELATIVE SIZE IS COPIED
VERBATIM and not resolved: two partials together know no more about what
it is relative to than either knew alone. Resolving is `overlay`'s, where
a base is in reach.

`overlay` is one partial RESOLVED AGAINST a base — one step of a cascade.
Field by field, the overlay wins where it sets a field and the base
stands where it does not. Variations are appended, an axis already
present replaced where it stands.

A RELATIVE SIZE BECOMES PIXELS HERE — and so do a relative tracking and
word spacing, against the size the type comes to — which is the one thing
this does that a plain merge cannot. `em` multiplies the base's size,
falling back to 16 px when the base states no size or states its own
relatively; `rem` multiplies the root size; `lh` multiplies the line
height, and when that is 0 — no line height known — it multiplies 1.2
times the base size instead, a single-spaced line being about that much
of the type it is set in. The face's own number is `lineHeightOf`, where
the font service that can answer it lives; pass it in when the face is in
reach.

The `TextStyle` overload applies THE SET FIELDS OF a partial TO A STYLE
THAT IS ALREADY TOTAL — a cascade step whose base is a built `TextStyle`
rather than a `Type`. A relative size resolves against that style's own
`ShapingStyle::fontSize` for `em`, the initial 16 px for `rem`, and 1.2
times that font size for `lh`; the style carries no line height, so a
caller who knows one resolves the size itself through the `Type` overload
instead. `Type::color8` chooses the ladder for a colour the partial
itself states: a partial that sets the flag and no colour changes
nothing, because the base's colour is already a number in the paint and
re-sending it through the byte would quantise a colour nobody restated.

`toTextStyle` is the `TextStyle` a TOTAL `Type` names, each unset field
taking its `initialType` value. A size still stated relatively has
nothing left in reach to be relative to and is resolved against the
initial 16 px.

## Length: a distance stated against a size it does not carry

A style written once and set at several sizes states its distances as
MULTIPLES rather than as pixels: half the type size is one distance at 13
px and another at 48, and a length that had baked the pixels in would be
right at one size and wrong at every other.

PIXELS ARE IMPLICIT, so a plain number already is a length — `13` is
thirteen pixels and takes no suffix. The three relative units name what
they are multiples of, and a `Length` carries nothing else: WHO resolves
one is whoever knows that number, which is the whole reason to keep the
unit and leave the resolution out.

`em`, `rem` and `lh` spell the three relative units, and the suffixes
`_em`, `_rem` and `_lh` spell them again — `1.5_em`, `2_rem`, `0.5_lh` —
each in both the floating and the integral spelling, so `2_em` and
`2.0_em` are one length. There is no `_px`: a plain number already is
pixels.

## TypeSheet: the named classes

THE TYPE HALF OF A SHEET: a base style and the named partials over it —
small, ordered, comparable by value. The sheet a tree states, with a
block half beside every name, is `StyleSheet`; this is what it hands the
paragraph layer, which shapes a rich run written with a name and cannot
see a block.

The levels of a log, the states a selection switches between, the roles a
table's columns take: a handful of treatments fixed once, then addressed
at the point of use by a NAME. What carries the name is the content — a
row, a span, a cell — so the content stays a plain value and the type
treatment stays in one place.

**An entry is a PARTIAL, not a whole style.** A class states what it
CHANGES — "red", "one size down", "the mono face" — and the base supplies
everything it is silent about. That is what lets one sheet serve a
document whose base size is decided elsewhere: a class that had to spell
the whole style would have to spell the size too, and then a page set
larger would take its classes at the wrong size.

**Lookup always answers.** A name that was never registered — including
the empty name — resolves to the BASE alone. There is no null and no
failure mode: a misspelled name shows as content set in the base style,
never as content that did not draw. Callers who must know whether a name
exists ask `TypeSheet::find`, which returns the partial or null, or
`TypeSheet::contains`.

**Entries keep insertion order** and `TypeSheet::set` replaces in place,
so a sheet built by one call sequence is one value. Equality is exact and
order-sensitive — same base, same entries, same order — which is what
lets a `TypeSheet` sit inside a larger comparable value and be diffed
with it rather than reasoned about.

Lookup is a linear scan. A style sheet names a handful of roles; one
large enough for that to matter has stopped being a sheet of named
classes and become a document's worth of formatting.

A sheet may be spelled as a literal, an entry per class:
`TypeSheet{{"ts", {.size = 11}}, {"dim", {.color = grey}}}`. A name
spelled twice keeps the later entry, in the earlier one's place.

## See also

`reference/PARAGRAPHS.md` for the text these styles are attached to, and
`reference/LAYOUT.md` for the block half of a sheet.
