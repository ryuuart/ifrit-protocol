// crt_bloom.cpp — BLOOM IS THE SAME THING TWICE, and scanlines are
// three layers that ADD.
// =============================================================================
// Two constructions, both of them additive, both of them made of nodes
// that already exist.
//
//   THE BLOOM  the headline is described TWICE: once sharp on top, and
//     once underneath, blurred and blended with kPlus. There is no glow
//     primitive and there does not need to be one — a halo is a dim
//     spread copy of the thing that glows, and stating it that way keeps
//     the halo's colour, weight and position exactly the headline's
//     because it IS the headline. The blurred copy carries
//     `Cache::Texture`: it never changes, and a blur re-run every frame
//     over a headline that is standing still is the most expensive
//     nothing in a scene.
//   THE FIELD  three scanline leaves at different phases and different
//     insets, each blended with kPlus. Where two overlap the brightness
//     ADDS, so the field is uneven wherever the layers agree — which is
//     the whole reason to stack three cheap layers instead of drawing
//     one expensive correct one.
//
// The scanlines are `custom()` leaves because a rule every six pixels is
// a loop, not a tree: a hundred boxes would describe, lay out, reconcile
// and paint where one paint program suffices, and nothing about them
// needs to be addressable.
//
// EDIT THESE FIRST
//   the blur sigma        — how far the halo spreads.
//   the three tints       — they add, so each one is a CONTRIBUTION and
//                           not a colour; darker than you expect.
//   the scanline stride   — six px is a phosphor pitch; at two it is a
//                           screen door, at twenty it is a venetian blind.

#include <include/core/SkPaint.h>
#include <include/effects/SkImageFilters.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {860, 520};

/** One field of horizontal rules at @p phase, added to what is under
 *  it. Absolute and inset by its caller, so the three layers can cover
 *  different parts of the frame. */
Element scanlines(float phase, SkColor4f tint) {
  return custom([phase, tint](SkCanvas& canvas, const PaintContext& ctx) {
           SkPaint paint;
           paint.setColor4f(tint, nullptr);
           // the loop walks a distance; the accumulated float is the position
           // NOLINTNEXTLINE(clang-analyzer-security.FloatLoopCounter,bugprone-float-loop-counter)
           for (float y = phase; y < ctx.size.height(); y += 6.0f)
             canvas.drawRect(SkRect::MakeXYWH(0, y, ctx.size.width(), 2.4f),
                             paint);
         })
      .absolute()
      .inset(0)
      .blend(SkBlendMode::kPlus);
}

}  // namespace

struct CrtBloom final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.02f, 0.03f, 0.05f, 1});
    ctx.captureAt(1.0);

    const auto headline = [](SkColor4f color) {
      return text(u8"PHOSPHOR", type({.size = 96, .color = color}));
    };
    const auto caption = [](const char8_t* line) {
      return text(line,
                  type({.size = 17, .color = {0.604f, 0.643f, 0.733f, 1}}));
    };

    ctx.composer.render(
        stack()
            .fill(Fill::color({0.02f, 0.03f, 0.05f, 1}))
            .child(box()
                       .column()
                       .padding(70)
                       .gap(10)
                       .inset(0)
                       .zIndex(2)
                       .child(headline({0.616f, 0.949f, 1.0f, 1}))
                       .child(caption(u8"plus-blended scanline layers "
                                      u8"brighten where they stack;"))
                       .child(caption(u8"the halo is the same headline, "
                                      u8"blurred and plus-blended.")))
            .child(box()
                       .column()
                       .padding(70)
                       .inset(0)
                       .zIndex(1)
                       .effect(Effect::filter(
                           SkImageFilters::Blur(14, 14, nullptr)))
                       .blend(SkBlendMode::kPlus)
                       .cache(Cache::Texture)
                       .child(headline({0.165f, 0.498f, 0.588f, 1})))
            .child(scanlines(0.0f, {0.10f, 0.22f, 0.16f, 1}).zIndex(0))
            .child(scanlines(2.0f, {0.16f, 0.10f, 0.20f, 1})
                       .inset(0, 0, 120, 0)
                       .zIndex(0))
            .child(scanlines(4.0f, {0.05f, 0.12f, 0.24f, 1})
                       .inset(140, 40, 0, 0)
                       .zIndex(0)));
  }
};

SIGIL_SKETCH(CrtBloom, "Kit · API",
             "additive layers — a bloom that is the headline described "
             "twice, over three plus-blended scanline fields")
