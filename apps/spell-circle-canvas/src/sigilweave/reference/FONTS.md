# SigilWeave — the font service and the shaper

The chapter on the service object every layout call is handed, the
immutable shaped run it caches, and the platform port that finds a face
in the first place. `README.md` beside the library is the front page.

## FontContext: the per-thread service

The per-thread service object at the centre of the pipeline: font
management — HarfBuzz faces, variable-font clones, fallback resolution —
and the content-addressed word shape cache, with observable statistics
for tests and benchmarks. Create one `FontContext` per layout thread and
hand it to every `Paragraph` and `layoutParagraph` call.

It owns three caches:

- an `hb_face` and `hb_font` per `SkTypeface`, so font data is parsed
  once, ever;
- per typeface, code point and language, the glyph coverage and the font
  fallback;
- the word shape cache.

None of it is locked: create one `FontContext` per layout thread. All
caches key off `SkTypeface::uniqueID`, so typefaces must outlive the
context or be consistently owned by it — they are ref'd where retained.

### Fallback

`FontContext::FallbackResolver` chooses a fallback for a code point
missing from the primary typeface. Returning null leaves the primary in
place, and therefore permits a missing glyph. The default resolver uses
`SkFontMgr::matchFamilyStyleCharacter`; applications can instead encode
their own family lists, script preferences, or platform cascade policy.
The resolver is called only on a fallback-cache miss.

The language tag it is handed is a borrowed view valid only for the
duration of the call, and it is NOT guaranteed to be NUL-terminated —
copy it before handing it to any C API that expects a C string, and
never pass its data pointer through directly.

`FontContext::resolveTypeface` returns the typeface to shape a code point
with: the primary — or the default — when it covers the code point,
otherwise the configured resolver's match. It is memoized per primary,
code point and language, so warm itemization passes never invoke the
resolver.

### Varied faces

`FontContext::variedTypeface` returns the memoized varied clone of a base
for a set of variations — the base itself, or the context default when
the base is null, when the variations are empty or cloning fails.

Memoization is the correctness mechanism, not just a speed-up: repeated
identical requests return the *same* `SkTypeface` object, so its unique
id is a stable shape-cache identity. A fresh clone per shape would mint a
new id every time and defeat the cache. The pipeline calls this for every
`ShapingStyle` with non-empty variations; applications only need it to
inspect the resolved face themselves.

`FontContext::variedTypefaceTransient` returns a varied clone this
context does NOT retain — it lives exactly as long as the caller's
reference.

It is for a coordinate that is not on a ladder. The memo above is keyed
on the coordinate's exact bytes and has no cap and no eviction, so a
caller that feeds it a continuously varying value adds a permanently
retained clone per frame forever; that is a leak whether or not the
caller ever asks for the same value twice. The cost model here is the
honest one for such a caller: the face is built fresh on every call and
its glyphs are rasterized fresh, and the price is CONSTANT per frame
rather than growing with how long the process has been running.

What that bounds, and what it does not:

- Bounded: this context retains nothing. The clone and everything hanging
  off it are released when the last caller reference drops.
- NOT bounded by this class: Skia's own strike cache sits underneath and
  keys on the face, so a distinct coordinate still mints distinct glyph
  strikes. That cache evicts against its own byte budget, so the memory
  stays capped, but the rasterization is genuinely repeated.

A face from there has no stable identity — two calls with the same
coordinate return different objects — so it must never be used where an
identity is a cache key that outlives the frame. That rules out
`ShapingStyle::variations`, whose whole point is a stable shape-cache
identity; it suits a draw-time drive, where the identity is only a batch
key within one frame's draw.

`FontContext::variedTypefaceCount` reports how many varied clones this
context is retaining — the memo `FontContext::variedTypeface` fills and
`FontContext::variedTypefaceTransient` does not. Tests assert a bound on
it; nothing else should read it.

### Asking a face a question

`FontContext::axisIsAdvanceInvariant` is true exactly when driving an
axis anywhere in its design range leaves every glyph advance of the face
unchanged, with advances sampled at both axis extremes. It is the gate a
draw-time variation drive asks: an advance-invariant axis — GRAD, most
fonts' `slnt` — may be animated at DRAW time without reshaping, while
`wght` moves advances on most fonts and must re-shape instead. It is
false when the face lacks the axis entirely.

`FontContext::glyphAdvanceEm` is the pen travel a glyph adds in a face,
as a fraction of the em, along the axis a run of the given orientation
steps on: the horizontal advance for a level run, the VERTICAL advance
for an upright column.

It is read from the same font the shaper reads, so it is the number the
pen actually moves by — Skia's glyph metrics carry no vertical advance at
all. A face with no vertical metrics table answers with the one vertical
advance it gives every glyph, which is a fact about that face and not a
failure: in it, every glyph does step the same distance down a column.

Ems rather than pixels, because the pixel advance scales with the size
the glyph is drawn at and a caller comparing two advances is asking about
the FACE. It is zero when both the base and the context default are null.

### Purging

`FontContext::purgeShapeCache` drops every cached shape result, and
nothing else. `FontContext::purgeAllCaches` drops every cache this
context owns: shape results, the per-typeface HarfBuzz faces and fonts,
glyph-coverage and fallback memos, varied clones, optical-kerning
measurements and interned language ids. It is for long-lived processes
whose typeface population changes over time — the per-typeface and
per-typeface-and-code-point maps are otherwise never pruned. It is safe
while shaped words are outstanding, because they own their data; the next
analysis simply re-fills at first-use cost.

`FontContext::Stats` is cache observability for tests and benchmarks, and
`FontContext::resetStats` clears the counters without clearing any cache.

## ShapedWord: the cached run

Lower-level shaping types the pipeline is built on. A `ShapedWord` is the
immutable, cache-shared result of shaping one word-sized segment with one
resolved typeface, script and direction: glyphs, advances, clusters, and
a lazily built origin-relative `SkTextBlob`. Most callers never reach for
it — `Paragraph` owns the shaping and `ParagraphLayout` owns the
placement; it is there to inspect or reuse individual glyph runs.

Instances are shared out of the shape cache; a layout never mutates one,
it only decides where to draw it. `ShapedWordReference` is the shared
handle: cheap to copy and safe to hold across layouts.

`ScriptTag` is a four-byte script tag in HarfBuzz encoding, e.g. `'Latn'`,
kept as a plain integer so HarfBuzz types stay out of the public headers.

`ShapedWord::vertical` says the run was shaped top to bottom — `vert`
forms, vertical metrics: positions stack glyphs downward from the origin,
x centred on the column axis, and the advances measure vertical pen
travel.

`shapeWord` shapes text with HarfBuzz, going through the shape cache. The
typeface must already be fallback-resolved; the right-to-left flag
selects the HarfBuzz direction, and the vertical flag shapes top to
bottom and is mutually exclusive with it.

`wordBlob` returns the shared origin-relative blob for a shaped word,
building and memoizing it on first use, and is cheap on every call after
the first.

`makeFont` is an `SkFont` configured the way SigilWeave shapes: unhinted,
subpixel, linear metrics. Rendering must match shaping or positions
drift.

`faceMetrics` is the face's own metrics at the size and design position a
shaping style names, with the variable-font clone resolved through the
context's memo so the numbers are the ones the shaped glyphs were
measured with.

`lineHeightOf` is THE FACE'S OWN SINGLE-SPACED LINE HEIGHT from metrics
already in hand: ascent to descent, plus the leading the face asks for
between its lines. It is the height a block's pitch is measured from when
its leading is the face's own, and the height its strut reports. The
overload taking a `TextStyle` resolves the face through a context — the
number to hand `overlay` as a line height when a size is stated in `lh`
and the face that answers is in reach.

## The platform port

`ports::systemFontManager` is the one place SigilWeave's tools, tests and
consumers obtain an `SkFontMgr` wired to the host operating system. Every
platform port hides behind the same call, so adding DirectWrite or
Fontconfig later touches one source file and never a call site.

Construction enumerates the installed font set, which is far too slow to
repeat per call site, so one shared instance is created lazily and reused
for the life of the process. The returned manager is immutable and safe
to hand to any number of font contexts on any thread.

`ports::pickTypeface` is the first of a list of families the system font
manager resolves, at a given style. The list is the point: reconstructing
a reference names a face that may not be installed on the machine running
the code, so callers pass the face they want followed by the stand-ins
they will accept. A family the chain leaves empty is passed over, so a
chain assembled at run time may carry a blank where it found no name. The
last resort is the default family AT THE REQUESTED STYLE, not at the
normal style — falling back to normal would silently drop the weight the
caller asked for.

The call taking a span of views is the form a COMPUTED chain takes — one
read out of a document, a settings file or a caller's own list, whose
length is not known where the call is written. The call taking a braced
list is the same call with the chain spelled out. The manager's own
family match walks the system font list, so a chain asked for more than
once goes through `ports::face`, which keeps the answer.

`ports::face` is THE SAME RESOLUTION, HELD. `ports::pickTypeface` walks
the installed font list on every call, so the answer is kept once per
families-and-style pair for the life of the process and handed back on
every later ask.

```cpp
const sk_sp<SkTypeface> face =
    weave::ports::face({"SF Mono", "Menlo", "monospace"});
```

WHY THIS IS A CALL AND NOT A `static` AT THE CALL SITE. A local `static`
holds one answer per site, so the same four families asked for in twenty
places walk the list twenty times and hand back twenty faces — and a face
is compared by POINTER wherever a style, a memo key or an inherited value
is compared, so two resolutions of one family never compare equal and
everything keyed on them re-does its work. One holder gives one answer.

It is safe from any thread: a describe runs on whichever thread the host
calls on, and the holder is guarded. The face itself is immutable and
shared, exactly as the font manager's is.

`ports::face` takes a computed chain and a spelled-out one as
`ports::pickTypeface` does. Both reach the one holder, so a chain
assembled at run time and the same chain written as literals are one
entry and one face.

Both calls have an overload spelled with a weight and a slant, for the
common case where the caller has those two numbers and not an
`SkFontStyle`.

## See also

`reference/PARAGRAPHS.md` for the document a shape cache serves, and
`reference/TYPE.md` for the style whose fields make the cache key.
