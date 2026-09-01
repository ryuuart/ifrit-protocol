// path_booleans.cpp — THE PATH OPERATOR VOCABULARY, on one sheet.
// =============================================================================
// Every operator in `geometry::path::ops` takes outlines and returns an
// outline. That is the whole contract, and it is why they compose: the
// output of a boolean is a legal input to an offset, which is a legal
// input to a distort, and none of them knows what made its argument.
//
//   ROW 1  THE BOOLEANS. The same star and circle under all four, with
//          the two operands outlined faintly behind each result, so what
//          each one kept is readable rather than asserted.
//   ROW 2  OFFSET, both ways. One squircle, offset in even steps inward
//          and outward; the heavy ring is the un-offset original. An
//          offset is not a scale: the outward steps round the corners
//          off, and the inward ones lose the shoulders entirely and
//          arrive at a plain rounded rect.
//   ROW 2R A RECIPE. `chain()` composes three steps into one `PathOp`
//          value — the non-destructive form: the recipe is a value that
//          can be applied to any outline, not a sequence of edits made
//          to one.
//   ROW 3  THE DISTORTS, one base star through each. Roughen is seeded,
//          so it is the same picture every run; Zigzag is drawn twice to
//          show its `smooth` flag, which is the difference between sine
//          ridges and saw teeth.
//
// EDIT THESE FIRST
//   the Row 2 recipe's steps — reorder them and the picture changes: a
//   roughen before an offset is smoothed away by the offset.
//   Roughen::seed          — a different jitter, still reproducible.
//   PuckerBloat::amount    — ±1 is full strength either way.

#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <sigilcompose/shape/Shapes.h>
#include <sigilgeometry/path/Ops.h>
#include <sigilsketch/canvas/Sketch.h>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace ops = sigil::geometry::path::ops;

namespace {

/** A shape generator's outline at a diameter, centred on a point. */
template <class Shape>
SkPath at(const Shape& shape, float radius, SkPoint center) {
  return shape.path({radius * 2, radius * 2})
      .makeTransform(
          SkMatrix::Translate(center.fX - radius, center.fY - radius));
}

void fillPath(SkCanvas& canvas, const SkPath& path, SkColor4f color) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setColor4f(color);
  canvas.drawPath(path, paint);
}

void outlinePath(SkCanvas& canvas, const SkPath& path, SkColor4f color,
                 float width) {
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(width);
  paint.setColor4f(color);
  canvas.drawPath(path, paint);
}

}  // namespace

struct PathBooleans final : sketch::Sketch {
  void draw(SkCanvas& canvas) const {
    // Row 1 — the four booleans on one pair of operands.
    {
      const float y = 140;
      const SkColor4f ink = {0.85f, 0.9f, 1.0f, 1};
      struct Case {
        SkPath (*op)(const SkPath&, const SkPath&);
      };
      const Case cases[] = {
          {ops::unite}, {ops::subtract}, {ops::intersect}, {ops::exclude}};
      float x = 170;
      for (const Case& c : cases) {
        const SkPath a = at(shapes::star(5, 34.0f / 78.0f), 78, {x - 18, y});
        const SkPath b = at(shapes::circle(), 52, {x + 34, y + 18});
        outlinePath(canvas, a, {0.4f, 0.5f, 0.7f, 0.5f}, 1.5f);
        outlinePath(canvas, b, {0.4f, 0.5f, 0.7f, 0.5f}, 1.5f);
        fillPath(canvas, c.op(a, b), ink);
        x += 300;
      }
    }
    // Row 2 left — offset rings, inward and outward from one blob.
    {
      const SkPath base = at(shapes::squircle(3.0f), 70, {250, 400});
      for (int i = -2; i <= 3; ++i) {
        const SkPath ring = ops::offset(base, (float)i * 22.0f);
        outlinePath(canvas, ring,
                    {0.3f + 0.12f * (float)(i + 2),
                     0.75f - 0.09f * (float)(i + 2), 1.0f, 0.9f},
                    i == 0 ? 4.0f : 2.0f);
      }
    }
    // Row 2 right — one recipe value, applied to one outline.
    {
      const SkPath base = at(shapes::circle(), 80, {700, 400});
      outlinePath(canvas, base, {0.4f, 0.5f, 0.7f, 0.6f}, 1.5f);
      const ops::PathOp recipe =
          ops::chain({ops::offsetBy(18), ops::Zigzag{7, 30, true},
                      ops::Roughen{2.5f, 6, 11}});
      fillPath(canvas, recipe(base), {1.0f, 0.62f, 0.3f, 0.95f});
    }
    // Row 3 — the distort menu over one base star.
    {
      const float y = 640;
      const SkPath base = shapes::star(6, 38.0f / 70.0f)
                              .path({140, 140})
                              .makeTransform(SkMatrix::Translate(-70, -70));
      struct Row {
        SkPath path;
        SkColor4f color;
      };
      const Row rows[] = {
          {ops::Roughen{5, 7, 3}.apply(base), {0.55f, 0.95f, 0.7f, 1}},
          {ops::Zigzag{6, 26, false}.apply(base), {0.95f, 0.85f, 0.4f, 1}},
          {ops::Zigzag{6, 26, true}.apply(base), {0.95f, 0.6f, 0.4f, 1}},
          {ops::PuckerBloat{-0.6f}.apply(base), {0.7f, 0.55f, 0.95f, 1}},
          {ops::PuckerBloat{0.7f}.apply(base), {0.45f, 0.75f, 0.95f, 1}},
          {ops::Twirl{100}.apply(base), {0.95f, 0.5f, 0.7f, 1}},
      };
      float x = 130;
      for (const Row& row : rows) {
        canvas.save();
        canvas.translate(x, y);
        fillPath(canvas, row.path, row.color);
        canvas.restore();
        x += 200;
      }
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

SIGIL_SKETCH(PathBooleans, "Kit · API",
             "geometry::path::ops — the four booleans over one pair, "
             "offsets both ways, a chain() recipe as a value, and the "
             "distorts over one star")
