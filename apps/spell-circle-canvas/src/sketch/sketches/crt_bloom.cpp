// crt_bloom.cpp — THE PRIMITIVE AND THE CONSTRUCTION IT NAMES.
// =============================================================================
// A halo is a dim spread copy of the thing that glows, and the library
// has a verb for exactly that: `Effect::glow(colour, sigma)` re-emits the
// layer blurred beneath itself. This sheet puts that verb beside the
// stack it names, so what the primitive does and what it costs to spell
// by hand are on one page.
//
//   ABOVE  one node. `Effect::glow(halo, sigma)` on the headline itself.
//     The halo is the headline's own coverage, so its weight, its
//     spacing and its position cannot drift from the letters — there is
//     nothing to keep in step.
//   BELOW  two nodes. The headline described TWICE: once sharp on top,
//     once underneath, blurred and blended with `kPlus`. The blurred
//     copy carries `Cache::Texture` — it never changes, and a blur re-run
//     every frame over a headline that is standing still is the most
//     expensive nothing in a scene.
//
// THE TWO ARE NOT THE SAME PICTURE, and the difference is the reason to
// build one by hand. `glow` composites its halo UNDER the content, which
// is a drop shadow at zero offset; the stack ADDS it, so the halo and the
// letters sum where they overlap and the core blows out. A phosphor adds.
// A shadow does not. Everything else about the two — the spread, the
// colour, the fact that the halo is the letters — is identical, which is
// what makes the comparison worth drawing rather than describing.
//
//   THE FIELD  three scanline leaves at different phases and different
//     insets, each blended with kPlus. Where two overlap the brightness
//     ADDS, so the field is uneven wherever the layers agree — which is
//     the whole reason to stack three cheap layers instead of drawing
//     one expensive correct one. The library has a scanline field
//     (`material::crtOverlay`); it does not have this stacking, and the
//     stacking is what the picture is made of.
//
// The scanlines are `custom()` leaves because a rule every six pixels is
// a loop, not a tree: a hundred boxes would describe, lay out, reconcile
// and paint where one paint program suffices, and nothing about them
// needs to be addressable.
//
// EDIT THESE FIRST
//   kSigma              — how far both halos spread. One number, both
//                         constructions, so they stay comparable.
//   the three tints     — they add, so each one is a CONTRIBUTION and
//                         not a colour; darker than you expect.
//   the scanline stride — six px is a phosphor pitch; at two it is a
//                         screen door, at twenty it is a venetian blind.

#include <include/core/SkPaint.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {900, 700};
/** The spread, shared by both constructions so the two are comparable. */
constexpr float kSigma = 14.0f;

const SkColor4f kCore{0.616f, 0.949f, 1.0f, 1};
const SkColor4f kHalo{0.165f, 0.498f, 0.588f, 1};
const SkColor4f kCaption{0.745f, 0.788f, 0.867f, 1};

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

Element headline(SkColor4f color) {
  return text(u8"PHOSPHOR", type({.size = 96, .color = color}));
}

Element caption(const char8_t* line) {
  return text(line, type({.size = 17, .color = kCaption}));
}

}  // namespace

struct CrtBloom final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background({0.02f, 0.03f, 0.05f, 1});
    ctx.captureAt(1.0);

    // ABOVE — one node. The effect owns the whole construction.
    Element primitive =
        box()
            .column()
            .padding(64, 46)
            .gap(10)
            .inset(0, 0, 0, kCanvas.height() * 0.5f)
            .zIndex(2)
            .child(headline(kCore).effect(Effect::glow(kHalo, kSigma)))
            .child(caption(u8"Effect::glow(colour, sigma) \xc2\xb7 one node; "
                           u8"the halo composites UNDER the letters"));

    // BELOW — two nodes at the same place, the second one blurred and
    // ADDED. The blur is spelled as an axis-aligned directional blur
    // rather than a raw Skia filter: at 0° it IS a Gaussian blur, and it
    // carries a comparable recipe, so a re-described equal blur prunes
    // where a built filter can only compare by pointer.
    Element built = stack().inset(0, kCanvas.height() * 0.5f, 0, 0);
    built.child(box()
                    .column()
                    .padding(64, 46)
                    .gap(10)
                    .inset(0)
                    .zIndex(2)
                    .child(headline(kCore))
                    .child(caption(u8"the same headline described twice "
                                   u8"\xc2\xb7 the halo is ADDED, so the "
                                   u8"core blows out")));
    built.child(box()
                    .column()
                    .padding(64, 46)
                    .inset(0)
                    .zIndex(1)
                    .effect(Effect::directionalBlur(kSigma, 0.0f, kSigma))
                    .blend(SkBlendMode::kPlus)
                    .cache(Cache::Texture)
                    .child(headline(kHalo)));

    ctx.composer.render(
        stack()
            .fill(Fill::color({0.02f, 0.03f, 0.05f, 1}))
            .child(std::move(primitive))
            .child(std::move(built))
            .child(scanlines(0.0f, {0.10f, 0.22f, 0.16f, 1}).zIndex(0))
            .child(scanlines(2.0f, {0.16f, 0.10f, 0.20f, 1})
                       .inset(0, 0, 120, 0)
                       .zIndex(0))
            .child(scanlines(4.0f, {0.05f, 0.12f, 0.24f, 1})
                       .inset(140, 40, 0, 0)
                       .zIndex(0)));
  }
};

SIGIL_SKETCH(CrtBloom, "Kit \xc2\xb7 API",
             "Effect::glow beside the stack it names \xe2\x80\x94 one node "
             "against two, over three plus-blended scanline fields")
