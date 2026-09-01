// blend_keys.cpp — ONE API: path::blend::make(keys, options), on the
// dial that decides how many intermediates fall between two outlines.
// =============================================================================
// The blend tool interpolates OUTLINES, not pixels: two paths are
// resampled by arc length, their contours aligned cyclically, and every
// intermediate is a real path that could be stroked, filled or handed to
// any other operator. Three rows, one question each.
//
//   1. TWO KEYS, EIGHT STEPS. A five-point star to a circle. The
//      correspondence is what makes the run readable — arms shorten and
//      the hub swells rather than points wandering across each other.
//   2. THREE KEYS. `make()` takes a span, and the spine is split into
//      one span per key PAIR, so a waypoint bends the run without
//      changing how it is spaced. Colour rides the same parameter.
//   3. OUTLINE ONLY. A Key with no fill and a `stroke` carries its
//      stroke WIDTH through the blend too, so the run thins as it goes.
//
// EDIT THESE FIRST
//   Options::steps        — intermediates per key pair.
//   Options::samples      — arc-length samples per contour while
//                           interpolating; low values show the polygon.
//   Options::smoothOutlines — fit each step with cubics instead.

#include <include/core/SkMatrix.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilsketch/canvas/Sketch.h>

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

}  // namespace

struct BlendKeys final : sketch::Sketch {
  void draw(SkCanvas& canvas) const {
    // Row 1 — the two-key run.
    {
      blend::Key from{at(shapes::star(5, 30.0f / 70.0f), 70, {110, 130}),
                      {1.0f, 0.42f, 0.30f, 1}};
      blend::Key to{at(shapes::circle(), 64, {1090, 130}),
                    {0.30f, 0.62f, 1.0f, 1}};
      blend::Options options;
      options.steps = 8;
      blend::draw(canvas, blend::make(from, to, options));
    }
    // Row 2 — a waypoint between the ends.
    {
      const blend::Key keys[3] = {
          {at(shapes::star(4, 28.0f / 70.0f), 70, {110, 330}),
           {1.0f, 0.85f, 0.25f, 1}},
          {at(shapes::squircle(3.6f), 60, {600, 330}), {0.35f, 1.0f, 0.65f, 1}},
          {at(shapes::star(12, 52.0f / 66.0f), 66, {1090, 330}),
           {0.75f, 0.4f, 1.0f, 1}}};
      blend::Options options;
      options.steps = 5;
      options.smoothOutlines = true;
      blend::draw(canvas, blend::make(keys, options));
    }
    // Row 3 — stroke width and stroke colour interpolate as well.
    {
      blend::Key from{at(shapes::star(6, 40.0f / 72.0f), 72, {110, 540}),
                      {0, 0, 0, 0}};
      from.stroke = SkColor4f{0.2f, 0.9f, 1.0f, 1};
      from.strokeWidth = 6;
      blend::Key to{at(shapes::circle(), 64, {1090, 540}), {0, 0, 0, 0}};
      to.stroke = SkColor4f{1.0f, 0.35f, 0.75f, 1};
      to.strokeWidth = 1;
      blend::Options options;
      options.steps = 14;
      options.smoothOutlines = true;
      blend::draw(canvas, blend::make(from, to, options));
    }
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1240, 720);
    ctx.background({0.063f, 0.063f, 0.078f, 1});
    ctx.captureAt(1.0);
    ctx.composer.render(custom([this](SkCanvas& canvas, const PaintContext&) {
                          draw(canvas);
                        }).inset(0));
  }
};

SIGIL_SKETCH(BlendKeys, "Kit · API",
             "path::blend::make() — a two-key run, a three-key run, and a "
             "stroke-only blend that carries its width across")
