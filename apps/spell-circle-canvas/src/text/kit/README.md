# TextFlowKit

Companion utilities for TextFlow consumers, distilled from the patterns the
gallery, demo, and SpellCircle app kept hand-rolling. Core TextFlow stays a
layout engine; this layer packages the *usage discipline* that keeps animated
text cheap. It is Qt-free, depends only on TextFlow and Skia, and ships as
its own target (`textflow::TextFlowKit`, the future vcpkg `Kit` component).

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

| Utility | The trap it prevents |
| --- | --- |
| `RebuildGuard<Keys...>` | Rebuild-on-input-change caches whose key is smeared across members and an if-condition; the key becomes one declared tuple. |
| `CachedValue<Value, Keys...>` | Expensive derived objects (paths, shaders) rebuilt per frame because caching them was boilerplate. |
| `LayoutGuard<Keys...>` | Re-laying text out every frame — or forgetting `revision()`/`needsShaping()` in a hand-rolled guard and freezing edits. Both are baked in; you declare only the inputs the library can't see. |
| `quantize()` | Animated layout inputs (a breathing measure) that change sub-pixel per frame and defeat the guard above. |
| `GlyphBuckets<Key, Placement>` | Per-glyph choreography turning into per-glyph draw calls; generalizes `textflow::GlyphRSXformBatches` to arbitrary bucket keys and draw passes. |
| `makeStyle()` / `drawLabel()` | Ten-line single-span style and caption rituals, reinvented per tool. |
| `mixedScriptFiller()` | Every showcase growing subtly different stress content; timings stay comparable on a shared deterministic corpus. |
| `Stopwatch` / `toMicroseconds()` | Frame-timing brackets duplicated across targets. |

`textflow::SingleLineParagraphCache` (in core) is the companion for
high-frequency short labels; `drawLabel()` documents when to graduate to it.

## The shape of a well-behaved scene

```cpp
class MyScene {
  textflow::Paragraph m_paragraph;
  textflowkit::RebuildGuard<std::u16string, const SkTypeface *, float> m_content;
  textflowkit::LayoutGuard<SkISize, textflow::TextAlignment> m_layoutGuard;
  textflow::ParagraphLayout m_layout;

  void render(SkCanvas *canvas, SkISize size, ...) {
    // 1. Content: rebuilt only when its inputs change.
    m_content.ensure({text, typeface.get(), fontSize}, [&] {
      m_paragraph.clear();
      m_paragraph.appendText(text, textflowkit::makeStyle(fontSize, color));
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

The gallery scenes under `src/gallery/scenes/` are the living reference:
each one is built on core TextFlow plus exactly these utilities.
