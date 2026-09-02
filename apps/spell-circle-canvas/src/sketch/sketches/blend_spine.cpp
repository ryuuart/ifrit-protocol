// blend_spine.cpp — ONE API: blend::Options::spine, with Spacing::Distance
// and Orientation::AlignToPath.
// =============================================================================
// A blend's steps normally travel the straight line between the two
// keys' centroids. Hand it a SPINE and that line is replaced: the run
// walks the path instead, and two further dials decide what walking it
// means.
//
//   Spacing::Distance      — spacing measured in px OF SPINE rather than
//                            as a count, so a long spine gets more steps
//                            and the density stays even wherever the
//                            curve doubles back on itself.
//   Orientation::AlignToPath — each step ROTATES to the spine's tangent.
//                            Against AlignToPage (steps stay upright and
//                            only travel) this is the difference between
//                            beads on a wire and confetti on a line.
//
// The spine here is an Archimedean spiral, which is the shape that makes
// both dials visible at once: the tangent turns through more than two
// full revolutions, and the arc length per revolution grows, so a count
// would crowd the middle and starve the rim.
//
// EDIT THESE FIRST
//   options.distance    — px of spine between steps.
//   options.orientation — AlignToPage to stop the beads turning.
//   options.reverseSpine — which end the run starts from.

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

/** A shape generator's outline at a diameter, centred on the ORIGIN —
 *  a spined blend places its own steps, so the keys carry no position
 *  of their own. */
template <class Shape>
SkPath centered(const Shape& shape, float radius) {
  return shape.path({radius * 2, radius * 2})
      .makeTransform(SkMatrix::Translate(-radius, -radius));
}

SkPath spiral(SkPoint center, float innerRadius, float growth, float turns) {
  SkPathBuilder b;
  const int n = 400;
  for (int i = 0; i <= n; ++i) {
    const float t = (float)i / (float)n;
    const float a = t * turns * 6.2831853f;
    const float r = innerRadius + t * growth;
    const SkPoint p = {center.fX + r * std::cos(a),
                       center.fY + r * std::sin(a)};
    i == 0 ? (void)b.moveTo(p) : (void)b.lineTo(p);
  }
  return b.detach();
}

}  // namespace

struct BlendSpine final : sketch::Sketch {
  void draw(SkCanvas& canvas) const {
    blend::Key from{centered(shapes::star(3, 16.0f / 34.0f), 34),
                    {1.0f, 0.9f, 0.3f, 0.95f}};
    blend::Key to{centered(shapes::star(7, 12.0f / 30.0f), 30),
                  {0.4f, 0.5f, 1.0f, 0.95f}};
    blend::Options options;
    options.spacing = blend::Spacing::Distance;
    options.distance = 34;
    options.spine = spiral({620, 360}, 40, 260, 2.2f);
    options.orientation = blend::Orientation::AlignToPath;
    options.smoothOutlines = true;
    blend::draw(canvas, blend::make(from, to, options));
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

SIGIL_SKETCH(BlendSpine, "Kit · API",
             "blend::Options::spine — a three-point star becoming a "
             "seven-point one along a spiral, spaced by distance and "
             "turned by the tangent")
