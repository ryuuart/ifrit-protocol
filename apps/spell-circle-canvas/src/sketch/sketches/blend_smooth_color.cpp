// blend_smooth_color.cpp — ONE API: blend::Spacing, the dial that decides
// HOW MANY intermediates a blend makes.
// =============================================================================
// `Spacing::Steps` is a count the author picks. `Spacing::SmoothColor`
// is not: the blend chooses a count such that adjacent steps differ by
// less than the eye resolves, so the run reads as a continuous field
// rather than a stack of shapes. Which one is right is a question about
// the PICTURE, and the two halves of this sheet are the two answers.
//
//   LEFT   SmoothColor. One large blob shrinking into a small bright
//          one, dark blue to warm white. Nothing here names a step
//          count; the colour distance does.
//   RIGHT  Steps, at a count high enough to read as a ribbon. Two OPEN
//          waves, blended stroke to stroke — an open path has no inside,
//          so the whole figure is the interpolated outline.
//
// EDIT THESE FIRST
//   the left options' spacing — swap in Spacing::Steps with a count and
//   the field bands.
//   the right options' steps  — under about twenty the ribbon separates
//   into rails.

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace blend = sigil::geometry::path::blend;

namespace {

/** A shape generator's outline at a diameter, centred on a point. */
template <class Shape>
SkPath at(const Shape& shape, float radius, SkPoint center) {
  return shape.path({radius * 2, radius * 2})
      .makeTransform(
          SkMatrix::Translate(center.fX - radius, center.fY - radius));
}

/** An OPEN sine run from one point to another. Open on purpose: it is
 *  what makes the right-hand blend a ribbon of strokes rather than a
 *  stack of filled lozenges. */
SkPath wave(SkPoint from, SkPoint to, float amplitude, int cycles) {
  SkPathBuilder b;
  const int n = 96;
  for (int i = 0; i <= n; ++i) {
    const float t = (float)i / (float)n;
    const float x = from.fX + (to.fX - from.fX) * t;
    const float y = from.fY + (to.fY - from.fY) * t +
                    amplitude * std::sin(t * (float)cycles * 6.2831853f);
    i == 0 ? (void)b.moveTo({x, y}) : (void)b.lineTo({x, y});
  }
  return b.detach();
}

}  // namespace

struct BlendSmoothColor final : sketch::Sketch {
  void draw(SkCanvas& canvas) const {
    // LEFT — the count is a consequence of the colours, not an input.
    {
      blend::Key from{at(shapes::squircle(3.2f), 240, {320, 340}),
                      {0.08f, 0.10f, 0.35f, 1}};
      blend::Key to{at(shapes::circle(), 36, {350, 300}),
                    {1.0f, 0.95f, 0.55f, 1}};
      blend::Options options;
      options.spacing = blend::Spacing::SmoothColor;
      options.smoothOutlines = true;
      blend::draw(canvas, blend::make(from, to, options));
    }
    // RIGHT — two open runs, blended stroke to stroke.
    {
      blend::Key from{wave({700, 120}, {1180, 240}, 36, 3), {0, 0, 0, 0}};
      from.stroke = SkColor4f{0.15f, 0.85f, 1.0f, 0.9f};
      from.strokeWidth = 2.5f;
      blend::Key to{wave({660, 520}, {1160, 660}, 64, 2), {0, 0, 0, 0}};
      to.stroke = SkColor4f{1.0f, 0.3f, 0.75f, 0.9f};
      to.strokeWidth = 2.5f;
      blend::Options options;
      options.steps = 42;
      blend::draw(canvas, blend::make(from, to, options));
    }
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1240, 720);
    ctx.background({0.047f, 0.047f, 0.071f, 1});
    ctx.captureAt(1.0);
    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(BlendSmoothColor, "Kit · API",
             "blend::Spacing — SmoothColor picking its own step count "
             "against a stated count over two open runs")
