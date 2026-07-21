// zellige_study.cpp — a STUDY of the 8-fold Islamic star-and-cross panel
// ("Breath of the Compassionate"), after the walls of Fez. REFERENCES.md §4.
//
//   ./build/bin/Release/ComposeSketch \
//       src/common/compose/sketch/sketches/zellige_study.cpp
//
// Not an invented aesthetic: patterns::girih8 runs Hankin's polygons-in-
// contact rule on the real 4.8.8 tiling (contact angle θ=45° → the {8/2}
// khatam through every octagon's edge midpoints — the crossing angles
// Lu & Steinhardt measured on medieval girih tiles), colored with zellige
// palettes. The Pattern seam does what the user-model asks: ONE generator,
// parameterized, applied to different elements at different scales and
// rotations, REGENERATED at runtime (the wall re-tiles itself every few
// seconds — each swap is exactly one changed recipe, one new bake).
//
// Carved depth is the Photoshop route (LayerStyles inner shadow), not a
// shader. Headless: ComposeSketch <this> --frame zellige.png --at 1.0

#include <sigilsketch/Sketch.h>

#include <sigilcompose/LayerStyles.h>
#include <sigilcompose/Patterns.h>

using namespace sigil::compose;
using namespace sigil::compose::util;

namespace {

constexpr float W = 960, H = 640;
constexpr SkColor4f kPlaster{0.885f, 0.850f, 0.775f, 1};
constexpr SkColor4f kInk{0.180f, 0.129f, 0.106f, 1};

sigil::weave::TextStyle type(float size, SkColor4f color, float tracking = 0) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = tracking;
  s.paint.foreground.setColor(color.toSkColor());
  s.paint.foreground.setAntiAlias(true);
  return s;
}

} // namespace

struct ZelligeStudy : sketch::Sketch {
  // Three panels of one generator at different parameters — held as
  // members (the Pattern identity contract), re-rolled in update().
  Pattern left = patterns::girih8(16, patterns::fezPalette());
  Pattern middle = patterns::girih8(26, patterns::nasridPalette());
  Pattern right = patterns::girih8(20, patterns::fezPalette());
  Pattern grain = patterns::speckle(96, 60, 0.4f, 1.1f,
                                    {{0.35f, 0.30f, 0.24f, 0.25f}});
  double nextSwap = 0.0;
  int phase = 0;

  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(W, H);
    ctx.background(kPlaster);
    nextSwap = 3.0;
    phase = 0;
    ctx.composer.render(describe(ctx));
  }

  Element panel(const Pattern &p, const char *caption) {
    return box().column().grow(1).gap(8)
        .child(box().grow(1).corners({3})
                   .fill(const_cast<Pattern &>(p).material())
                   // Carved-plaster depth: the Photoshop route, no shader.
                   .foreground(styles::InnerShadow{{0, 0, 0, 0.38f},
                                                   {0, 4},
                                                   9})
                   .foreground(styles::innerGlow({1, 1, 1, 0.18f}, 5))
                   .stroke(stroke(2.5f, Fill::color(kInk))))
        .child(text(toU8(caption), type(13, kInk, 1.2f)));
  }

  Element describe(sketch::SketchContext &ctx) {
    return stack()
        .fill(grain.material()) // speckled plaster over the clear color
        .child(
            box().column().absolute().inset(56, 44, 56, 44).gap(14)
                .child(box().row().alignItems(Align::Baseline).gap(14)
                           .child(text(toU8("ZELLIJE"), type(34, kInk, 3)))
                           .child(text(toU8("Hankin PIC · 4.8.8 · θ = 45°"),
                                       type(14, {0.42f, 0.36f, 0.30f, 1}, 1))))
                .child(box().row().grow(1).gap(22)
                           .child(panel(left, "fez · a=16"))
                           .child(panel(middle, "nasrid · a=26"))
                           .child(panel(right, "fez · a=20 · rotated"))));
    (void)ctx;
  }

  void update(double elapsed, sketch::SketchContext &ctx) override {
    if (elapsed < nextSwap)
      return;
    nextSwap = elapsed + 3.0;
    ++phase;
    // Runtime regeneration: new parameters → new recipes → each panel
    // re-bakes exactly once and the reconciler sees one changed fill.
    const float edges[3] = {14.0f + 4 * (float)(phase % 3),
                            22.0f + 6 * (float)((phase + 1) % 2),
                            18.0f + 3 * (float)(phase % 4)};
    left = patterns::girih8(edges[0], (phase % 2) ? patterns::nasridPalette()
                                                  : patterns::fezPalette());
    middle = patterns::girih8(edges[1], (phase % 2)
                                            ? patterns::fezPalette()
                                            : patterns::nasridPalette());
    right = patterns::girih8(edges[2], patterns::fezPalette());
    right.rotate((float)(phase % 8) * 22.5f);
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(ZelligeStudy)
