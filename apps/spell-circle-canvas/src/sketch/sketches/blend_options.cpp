// blend_options.cpp — WHAT `blend::Options` DECIDES about a run between
// two outlines.
// =============================================================================
// The blend tool interpolates OUTLINES, not pixels: two paths are
// resampled by arc length, their contours aligned cyclically, and every
// intermediate is a real path that could be stroked, filled or handed to
// any other operator. What the options struct settles is how many of
// them there are, what rides along with them, and what a third key does
// to the run. Four bands, one question each.
//
//   1. HOW MANY, STATED. `Spacing::Steps` is a count the author picks —
//      eight intermediates between a five-point star and a circle. The
//      correspondence is what makes the run readable: arms shorten and
//      the hub swells rather than points wandering across each other.
//   2. A THIRD KEY. `make()` takes a span, and the spine is split into
//      one span per key PAIR, so a waypoint bends the run without
//      changing how it is spaced. Colour rides the same parameter.
//   3. WHAT ELSE INTERPOLATES. A Key with no fill and a `stroke` carries
//      its stroke WIDTH across too, so the run thins as it goes.
//   4. HOW MANY, DERIVED. `Spacing::SmoothColor` is not a count: the
//      blend chooses one such that adjacent steps differ by less than
//      the eye resolves, so the run reads as a continuous field rather
//      than a stack of shapes. Nothing on the left names a step count;
//      the colour distance does. Beside it, two OPEN waves blended
//      stroke to stroke at a stated count — an open path has no inside,
//      so the whole figure is the interpolated outline, and the count is
//      what decides whether it reads as a ribbon or as rails.
//
// Which spacing is right is a question about the PICTURE, which is why
// both spellings stand on one sheet.
//
// EDIT THESE FIRST
//   Options::steps        — intermediates per key pair.
//   Options::samples      — arc-length samples per contour while
//                           interpolating; low values show the polygon.
//   Options::smoothOutlines — fit each step with cubics instead.
//   the ribbon's steps    — under about twenty it separates into rails.

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/kit/Silhouettes.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilsketch/canvas/Sketch.h>

#include <cmath>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace blend = sigil::geometry::path::blend;

namespace {

/** A shape generator's outline at a diameter, centred on a point. The
 *  shape kit inscribes its figures in a box at the origin; every figure
 *  on this sheet is placed by its centre instead. */
template <class Shape>
SkPath at(const Shape& shape, float radius, SkPoint center) {
  return shape.path({radius * 2, radius * 2})
      .makeTransform(
          SkMatrix::Translate(center.fX - radius, center.fY - radius));
}

/** An OPEN sine run from one point to another. Open on purpose: it is
 *  what makes the last blend a ribbon of strokes rather than a stack of
 *  filled lozenges. */
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

struct BlendOptions final : sketch::Sketch {
  void draw(SkCanvas& canvas) const {
    // 1 — the two-key run at a stated count.
    {
      blend::Key from{at(shapes::star(5, 30.0f / 70.0f), 70, {110, 120}),
                      {1.0f, 0.42f, 0.30f, 1}};
      blend::Key to{at(shapes::circle(), 64, {1090, 120}),
                    {0.30f, 0.62f, 1.0f, 1}};
      blend::Options options;
      options.steps = 8;
      blend::draw(canvas, blend::make(from, to, options));
    }
    // 2 — a waypoint between the ends.
    {
      const blend::Key keys[3] = {
          {at(shapes::star(4, 28.0f / 70.0f), 70, {110, 320}),
           {1.0f, 0.85f, 0.25f, 1}},
          {at(shapes::squircle(3.6f), 60, {600, 320}), {0.35f, 1.0f, 0.65f, 1}},
          {at(shapes::star(12, 52.0f / 66.0f), 66, {1090, 320}),
           {0.75f, 0.4f, 1.0f, 1}}};
      blend::Options options;
      options.steps = 5;
      options.smoothOutlines = true;
      blend::draw(canvas, blend::make(keys, options));
    }
    // 3 — stroke width and stroke colour interpolate as well.
    {
      blend::Key from{at(shapes::star(6, 40.0f / 72.0f), 72, {110, 520}),
                      {0, 0, 0, 0}};
      from.stroke = SkColor4f{0.2f, 0.9f, 1.0f, 1};
      from.strokeWidth = 6;
      blend::Key to{at(shapes::circle(), 64, {1090, 520}), {0, 0, 0, 0}};
      to.stroke = SkColor4f{1.0f, 0.35f, 0.75f, 1};
      to.strokeWidth = 1;
      blend::Options options;
      options.steps = 14;
      options.smoothOutlines = true;
      blend::draw(canvas, blend::make(from, to, options));
    }
    // 4a — the count is a consequence of the colours, not an input.
    {
      blend::Key from{at(shapes::squircle(3.2f), 160, {280, 790}),
                      {0.08f, 0.10f, 0.35f, 1}};
      blend::Key to{at(shapes::circle(), 26, {305, 760}),
                    {1.0f, 0.95f, 0.55f, 1}};
      blend::Options options;
      options.spacing = blend::Spacing::SmoothColor;
      options.smoothOutlines = true;
      blend::draw(canvas, blend::make(from, to, options));
    }
    // 4b — two open runs, blended stroke to stroke at a stated count.
    {
      blend::Key from{wave({640, 680}, {1180, 700}, 30, 3), {0, 0, 0, 0}};
      from.stroke = SkColor4f{0.15f, 0.85f, 1.0f, 0.9f};
      from.strokeWidth = 2.5f;
      blend::Key to{wave({620, 880}, {1160, 900}, 48, 2), {0, 0, 0, 0}};
      to.stroke = SkColor4f{1.0f, 0.3f, 0.75f, 0.9f};
      to.strokeWidth = 2.5f;
      blend::Options options;
      options.steps = 42;
      blend::draw(canvas, blend::make(from, to, options));
    }
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1240, 980);
    ctx.background({0.055f, 0.055f, 0.075f, 1});
    ctx.captureAt(1.0);
    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(BlendOptions, "Kit · API",
             "blend::Options — a stated step count, a third key, the "
             "stroke width carried across, and SmoothColor picking its "
             "own count")
