# SigilWeaveKit

Companion utilities for SigilWeave consumers: the usage discipline that
keeps animated text cheap. Core SigilWeave stays a layout engine; this
layer packages that discipline. It is Qt-free, rests on the paint feature
(a label draws) and Skia, asks the Unicode leaf the character questions
its tables are derived from, and ships as its own target
(`sigil::weave::SigilWeaveKit`, the `Kit` component of the exported
package).

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
| `LayoutGuard.h` | `LayoutGuard<Keys...>` | Re-laying text out every frame — or forgetting `revision()`/`needsShaping()` in a hand-rolled guard and freezing edits. Both are baked in; you declare only the inputs the library can't see. |
| `GlyphBuckets.h` | `GlyphBuckets<Key, Placement>` | Per-glyph choreography turning into per-glyph draw calls; generalizes `sigil::weave::GlyphRSXformBatches` to arbitrary bucket keys and draw passes. |
| `Labels.h` | `makeStyle()` / `drawLabel()` | Ten-line single-span style and caption rituals, reinvented per tool. |
| `SampleText.h` | `mixedScriptFiller()` | Every showcase growing subtly different stress content; timings stay comparable on a shared deterministic corpus. |
| `Hyphenation.h` | `PatternHyphenator`, `englishHyphenationPatterns()` | The engine growing an opinion about where a language's words break. |
| `LineTables.h` | `kinsoku::japanese()`, `hanging::latin()`, `hanging::japanese()` | The engine growing an opinion about which marks may stand at a line's edge, and how far one may hang past it. |

`sigil::weave::SingleLineParagraphCache` (the engine's `cache` feature) is the companion for
high-frequency short labels; `drawLabel()` documents when to graduate to it.

The plain keyed guard the layout guard is built on knows nothing about
text, so it is not here: `sigil::core::RebuildGuard`,
`sigil::core::CachedValue` and `sigil::core::quantizeKey`, in
`<sigilcore/cache/Rebuild.h>`, are what a scene declares its non-text
derived values against.

## The tables: data the engine asks for and does not hold

The engine asks exactly one question about where a word may break
(`paragraph/Hyphenation.h`) and holds no answer, because where a word may
be broken is a fact about its language rather than about text layout.
`PatternHyphenator` answers it by Liang's pattern method — a word is
padded, every substring is looked up in a loaded table, the odd values it
collects are break points, and an explicit exception spelling overrides the
lot:

```cpp
static const kit::PatternHyphenator english("en",
                                            kit::englishHyphenationPatterns());
ParagraphLayoutOptions options;
options.hyphenation.patterns = &english;   // borrowed; outlives the layout
```

A table answers only for the language it was loaded for: a paragraph set in
a tag that does not start with it gets no answer at all, because a word
broken by the wrong language's rules is a misspelling and no answer is
merely a ragged line. `load()` takes the standard pattern-file text, so any
table in that format is a peer of the one here — a caller's own file, a
different language, a house exception list.

### Another language's patterns

The method matches LETTERS, of any script, so the engine is not English's.
`load()` takes UTF-8 pattern text: whitespace-separated patterns of letters
with digits between them and `.` for a word boundary, a `%` running a
comment to the end of its line, and optionally a line reading `exceptions`
followed by hyphenated spellings. That is the format TeX established and
the format the pattern tables published for other languages ship in — the
`hyph-utf8` collection carries one file per language for around seventy of
them, each under its own terms:

```cpp
// Read the file however this application reads its assets.
const std::string patterns = readTextFile("hyph-de-1996.pat.txt");
static const kit::PatternHyphenator german("de", patterns);
options.hyphenation.patterns = &german;    // one table per language tag
```

The corpus itself is NOT in this repository: pattern tables are data a
document ships with, they are large, and each travels under its own
licence. `englishHyphenationPatterns()` is carried only because a kit with no table
at all cannot be tried.

What the method cannot express is a language whose break rewrites the word
— the spellings that gain or change a letter across the break. A table
proposes positions; it never respells.

`englishHyphenationPatterns()` is a SUBSET of Liang's English (US) table, and it
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
  sigil::core::RebuildGuard<std::u16string, const SkTypeface *, float> m_content;
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
