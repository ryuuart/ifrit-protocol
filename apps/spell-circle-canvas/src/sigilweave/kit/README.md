# SigilWeaveKit

Companion utilities for SigilWeave consumers, distilled from the patterns the
gallery, demo, and SpellCircle app kept hand-rolling. Core SigilWeave stays a
layout engine; this layer packages the *usage discipline* that keeps animated
text cheap. It is Qt-free, rests on the paint feature (a label draws) and Skia,
asks the Unicode leaf the character questions its tables are derived from, and
ships as its own target (`sigil::weave::SigilWeaveKit`, the future vcpkg `Kit` component).

## Why this library exists

The optimizations that make a 60 fps text scene affordable are invisible and
easy to get wrong:

- Re-shaping a paragraph, or re-breaking its lines, on frames where only
  paint changed silently multiplies cost by the frame rate.
- The guard against that lives in a hand-written cache key — `m_last*`
  members plus an if-condition — and forgetting one member (typically the
  paragraph's `revision()` or `needsShaping()`) silently freezes live edits.
- Thousands of individually animated glyphs must collapse into a handful of
  draw calls, which means bucketing by draw state — another structure every
  effect reinvents.

Each utility here exists to make one of those keys or structures explicit.
The per-situation variation — what rebuilding means, how a bucket is drawn —
is deliberately a callable you pass in, not a policy the library imposes.

## Inventory

One header per utility under `include/sigilweave/kit/`, and
`kit/SigilWeaveKit.h` for all of them:

| Header | Utility | The trap it prevents |
| --- | --- | --- |
| `RebuildGuard.h` | `RebuildGuard<Keys...>` | Rebuild-on-input-change caches whose key is smeared across members and an if-condition; the key becomes one declared tuple. |
| `CachedValue.h` | `CachedValue<Value, Keys...>` | Expensive derived objects (paths, shaders) rebuilt per frame because caching them was boilerplate. |
| `LayoutGuard.h` | `LayoutGuard<Keys...>` | Re-laying text out every frame — or forgetting `revision()`/`needsShaping()` in a hand-rolled guard and freezing edits. Both are baked in; you declare only the inputs the library can't see. |
| `Quantize.h` | `quantize()` | Animated layout inputs (a breathing measure) that change sub-pixel per frame and defeat the guard above. |
| `GlyphBuckets.h` | `GlyphBuckets<Key, Placement>` | Per-glyph choreography turning into per-glyph draw calls; generalizes `sigil::weave::GlyphRSXformBatches` to arbitrary bucket keys and draw passes. |
| `Labels.h` | `makeStyle()` / `drawLabel()` | Ten-line single-span style and caption rituals, reinvented per tool. |
| `SampleText.h` | `mixedScriptFiller()` | Every showcase growing subtly different stress content; timings stay comparable on a shared deterministic corpus. |
| `Palette.h` | `palette::kInk`, `kPaper`, … | Every showcase picking its own near-black and off-white. |
| `Timing.h` | `Stopwatch` / `toMicroseconds()` | Frame-timing brackets duplicated across targets. |
| `Hyphenation.h` | `PatternHyphenator`, `patterns::english()` | The engine growing an opinion about where English words break. |
| `LineTables.h` | `kinsoku::japanese()`, `hanging::latin()`, `hanging::japanese()` | The engine growing an opinion about which marks may stand at a line's edge, and how far one may hang past it. |

`sigil::weave::SingleLineParagraphCache` (the engine's `cache` feature) is the companion for
high-frequency short labels; `drawLabel()` documents when to graduate to it.

## The tables: data the engine asks for and does not hold

The engine asks exactly one question about where a word may break
(`paragraph/Hyphenation.h`) and holds no answer, because where an English
word may be broken is a fact about English rather than about text layout.
`PatternHyphenator` answers it by Liang's pattern method — a word is
padded, every substring is looked up in a loaded table, the odd values it
collects are break points, and an explicit exception spelling overrides the
lot:

```cpp
static const kit::PatternHyphenator english("en",
                                            kit::patterns::english());
ParagraphLayoutOptions options;
options.hyphenation.patterns = &english;   // borrowed; outlives the layout
```

A table answers only for the language it was loaded for: a paragraph set in
a tag that does not start with it gets no answer at all, because a word
broken by the wrong language's rules is a misspelling and no answer is
merely a ragged line. `load()` takes the standard pattern-file text, so any
table in that format is a peer of the one here — a caller's own file, a
different language, a house exception list.

`patterns::english()` is a SUBSET of Liang's English (US) table, and it
says so where it is defined, together with the terms the original travels
under. A subset proposes fewer break points than the whole table, never a
different one where the whole table would inhibit — which is why the
exception spellings whose inhibitions matter most are carried with it. A
document that needs the whole table loads the whole table.

`LineTables.h` is the same idea at a line's two EDGES. `kinsoku::` names
the characters that may not open or close a line, and the engine settles
the prohibition by never opening that boundary; `hanging::` names how far
a character may stand outside the measure, as a fraction of its own
advance, which is optical margin alignment down a page and burasagari down
a column.

`kinsoku::japanese()` is DERIVED, not typed. Which characters may not stand
at a line's edge is already a property every character carries — its
line-break class — so the set is read off that: the closing punctuation and
parentheses, the non-starters, the conditional Japanese starters, the
exclamation and question marks and the infix numeric separators may not
open a line; the opening punctuation may not close one. It is then narrowed
to the characters standing in a FULL-WIDTH CELL, which is the punctuation
of the ideographic grid and exactly what the convention is about, leaving
ASCII punctuation to the segmentation, which carries the same classes
already.

That the set is largely already true is worth knowing rather than hiding.
The segmentation runs UAX #14 under a locale tailoring
(`Paragraph::setLineBreakLocale`, "ja@lb=strict"), and the tailoring is
where a script's own prohibitions come from FIRST; the table is what a
HOUSE adds on top. Its value is that it is a table, so a house's own
additions are a value rather than a patch.

## The shape of a well-behaved scene

```cpp
class MyScene {
  sigil::weave::Paragraph m_paragraph;
  sigil::weave::kit::RebuildGuard<std::u16string, const SkTypeface *, float> m_content;
  sigil::weave::kit::LayoutGuard<SkISize, sigil::weave::TextAlignment> m_layoutGuard;
  sigil::weave::ParagraphLayout m_layout;

  void render(SkCanvas *canvas, SkISize size, ...) {
    // 1. Content: rebuilt only when its inputs change.
    m_content.ensure({text, typeface.get(), fontSize}, [&] {
      m_paragraph.clear();
      m_paragraph.appendText(text, sigil::weave::kit::makeStyle(fontSize, color));
    });

    // 2. Restyle freely: setPaint/shader swaps are paint-only, never a relayout.

    // 3. Layout: memoized on (content revision, declared inputs).
    m_layoutGuard.ensure(m_paragraph, {size, alignment}, [&] {
      m_layout = layoutParagraph(fontContext, m_paragraph, flow, options);
    });

    // 4. Draw every frame; animation lives in paint and draw, not layout.
    m_layout.drawBatched(canvas, m_paragraph);
  }
};
```

The gallery scenes under `src/sigilweave/examples/gallery/src/scenes/` are the living reference:
each one is built on core SigilWeave plus exactly these utilities.
