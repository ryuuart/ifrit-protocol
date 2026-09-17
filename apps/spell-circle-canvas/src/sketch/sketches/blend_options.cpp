/** @file
 * blend_options — what `blend::Options` decides about a run between two
 * outlines.
 *
 * The blend tool interpolates OUTLINES, not pixels: two paths are
 * resampled by arc length, their contours aligned cyclically, and every
 * intermediate is a real path that could be stroked, filled or handed to
 * any other operator. What the options struct settles is how many
 * intermediates there are, what rides along with them, what a third key
 * does to the run, and — the last two bands — where the run travels at
 * all. Six bands, one question each.
 *
 *   1. HOW MANY, STATED. `Spacing::Steps` is a count the author picks —
 *      eight intermediates between a five-point star and a circle. The
 *      correspondence is what makes the run readable: arms shorten and
 *      the hub swells rather than points wandering across each other.
 *   2. A THIRD KEY. `make()` takes a span, and the spine is split into
 *      one span per key PAIR, so a waypoint bends the run without
 *      changing how it is spaced. Colour rides the same parameter.
 *   3. WHAT ELSE INTERPOLATES. A Key with no fill and a `stroke` carries
 *      its stroke WIDTH across too, so the run thins as it goes.
 *   4. HOW MANY, DERIVED. `Spacing::SmoothColor` is not a count: the
 *      blend chooses one such that adjacent steps differ by less than the
 *      eye resolves, so the run reads as a continuous field rather than a
 *      stack of shapes. Nothing names a step count; the colour distance
 *      does. Beside it, two OPEN waves blended stroke to stroke at a
 *      stated count — an open path has no inside, so the whole figure is
 *      the interpolated outline, and the count is what decides whether it
 *      reads as a ribbon or as rails.
 *   5. WHERE IT TRAVELS. Without a spine the steps walk the straight line
 *      between the keys' centroids. `options.spine` replaces that line,
 *      and `Spacing::Distance` measures the walk in px OF SPINE rather
 *      than as a count — so the density stays even wherever the curve
 *      doubles back, which a count cannot do.
 *   6. WHICH WAY THEY FACE. `Orientation::AlignToPath` turns each step to
 *      the spine's tangent. Against band 5's `AlignToPage` — the same
 *      spine, the same spacing, the steps upright — this is the
 *      difference between beads on a wire and confetti on a line.
 *
 * The spine both spine bands walk is an Archimedean spiral, the shape
 * that makes those two dials visible at once: the tangent turns through
 * more than two full revolutions, and the arc length per revolution
 * grows, so a count would crowd the middle and starve the rim.
 *
 * Which spacing is right is a question about the PICTURE, which is why
 * every spelling stands on one sheet.
 *
 * EDIT THESE FIRST
 *   Options::steps          — intermediates per key pair.
 *   Options::samples        — arc-length samples per contour while
 *                             interpolating; low values show the polygon.
 *   Options::smoothOutlines — fit each step with cubics instead.
 *   Options::distance       — px of spine between steps, in bands 5 and 6.
 *   the ribbon's steps      — under about twenty it separates into rails.
 */

// TAGS: Motion/Transitions

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace arrange = sigil::geometry::arrange;
namespace blend = sigil::geometry::path::blend;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;

namespace {

constexpr float kBand = 856;       // a full-width band's drawn width, px
constexpr float kRun = 150;        // the height of a run band
constexpr float kWide = 210;       // the height of the derived-count band
constexpr float kSpine = 300;      // one spine cell, square-ish
constexpr float kSpineCell = 551;  // (kBand - the gap between the two) / 2

constexpr SkColor4f kCellGround{0.085f, 0.085f, 0.105f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.055f, 0.075f, 1};
  look.palette.ink = {0.90f, 0.91f, 0.94f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.24f, 1};
  look.type.captionLabel = {.size = 12, .track = 0.6f};
  look.spacing.marginX = 40;
  look.spacing.marginTop = 40;
  look.spacing.marginBottom = 18;
  return look;
}

/** A generator's outline at a diameter, centred on a point. The shape
 *  kit inscribes its figures in a box at the origin; every figure on
 *  this sheet is placed by its centre instead. */
template <class Shape>
SkPath at(const Shape& shape, float radius, SkPoint center) {
  return shape.path({radius * 2, radius * 2})
      .makeTransform(
          SkMatrix::Translate(center.fX - radius, center.fY - radius));
}

/** A generator's outline centred on the ORIGIN — a spined blend places
 *  its own steps, so its keys carry no position of their own. */
template <class Shape>
SkPath centred(const Shape& shape, float radius) {
  return shape.path({radius * 2, radius * 2})
      .makeTransform(SkMatrix::Translate(-radius, -radius));
}

/** An OPEN sine run from one point to another. Open on purpose: it is
 *  what makes the last blend a ribbon of strokes rather than a stack of
 *  filled lozenges. */
SkPath wave(SkPoint from, SkPoint to, float amplitude, int cycles) {
  SkPathBuilder b;
  const int n = 96;
  for (int i = 0; i <= n; ++i) {
    const float t = arrange::along(0.0f, 1.0f, (size_t)i, (size_t)n + 1,
                                   arrange::Turn::Open);
    const float x = from.fX + (to.fX - from.fX) * t;
    const float y = from.fY + (to.fY - from.fY) * t +
                    amplitude * std::sin(t * (float)cycles * 6.2831853f);
    i == 0 ? (void)b.moveTo({x, y}) : (void)b.lineTo({x, y});
  }
  return b.detach();
}

using Painter = void (*)(SkCanvas&);

sketch::kit::ComparisonCase band(const char* caseTitle, std::string key,
                                 float width, float height, const char* call,
                                 const char* note, Painter paint) {
  return {.title = caseTitle,
          .control = call,
          .figure = custom(std::move(key),
                           [paint](SkCanvas& canvas) { paint(canvas); })
                        .width(width)
                        .height(height)
                        .fill(Fill::color(kCellGround)),
          .note = note};
}

Element explained(sketch::kit::ComparisonCase one) {
  const auto& look = sketch::kit::theme();
  return box()
      .row()
      .alignItems(Align::Start)
      .gap(24)
      .children({box().column().width(240).gap(12).children(
                     {document::label(std::move(one.title)),
                      text(std::move(one.control))
                          .font(look.font({.size = 10.5f, .mono = true}))
                          .ink(look.palette.ash),
                      document::caption(std::move(one.note))}),
                 std::move(one.figure)});
}

// 1 — the two-key run at a stated count.
void statedCount(SkCanvas& canvas) {
  blend::Key from{at(shapes::star(5, 30.0f / 70.0f), 62, {80, kRun / 2}),
                  {1.0f, 0.42f, 0.30f, 1}};
  blend::Key to{at(shapes::circle(), 56, {kBand - 80, kRun / 2}),
                {0.30f, 0.62f, 1.0f, 1}};
  blend::draw(canvas, blend::make(from, to, {.steps = 8}));
}

// 2 — a waypoint between the ends.
void waypoint(SkCanvas& canvas) {
  const blend::Key keys[3] = {
      {at(shapes::star(4, 28.0f / 70.0f), 62, {80, kRun / 2}),
       {1.0f, 0.85f, 0.25f, 1}},
      {at(shapes::squircle(3.6f), 54, {kBand / 2, kRun / 2 - 28}),
       {0.35f, 1.0f, 0.65f, 1}},
      {at(shapes::star(12, 52.0f / 66.0f), 58, {kBand - 80, kRun / 2}),
       {0.75f, 0.4f, 1.0f, 1}}};
  blend::draw(canvas, blend::make(keys, {.steps = 5, .smoothOutlines = true}));
}

// 3 — stroke width and stroke colour interpolate as well.
void strokes(SkCanvas& canvas) {
  const blend::Key from{
      .path = at(shapes::star(6, 40.0f / 72.0f), 62, {80, kRun / 2}),
      .fill = {0, 0, 0, 0},
      .stroke = SkColor4f{0.2f, 0.9f, 1.0f, 1},
      .strokeWidth = 6};
  const blend::Key to{.path = at(shapes::circle(), 56, {kBand - 80, kRun / 2}),
                      .fill = {0, 0, 0, 0},
                      .stroke = SkColor4f{1.0f, 0.35f, 0.75f, 1},
                      .strokeWidth = 1};
  blend::draw(canvas,
              blend::make(from, to, {.steps = 14, .smoothOutlines = true}));
}

// 4 — the count is a consequence of the colours, not an input; and the
// same question asked of two open runs at a stated count.
void derivedCount(SkCanvas& canvas) {
  {
    blend::Key from{at(shapes::squircle(3.2f), 96, {200, kWide / 2}),
                    {0.08f, 0.10f, 0.35f, 1}};
    blend::Key to{at(shapes::circle(), 16, {222, kWide / 2 - 18}),
                  {1.0f, 0.95f, 0.55f, 1}};
    blend::draw(canvas, blend::make(from, to,
                                    {.spacing = blend::Spacing::SmoothColor,
                                     .smoothOutlines = true}));
  }
  {
    const blend::Key from{.path = wave({470, 40}, {kBand - 40, 52}, 24, 3),
                          .fill = {0, 0, 0, 0},
                          .stroke = SkColor4f{0.15f, 0.85f, 1.0f, 0.9f},
                          .strokeWidth = 2.5f};
    const blend::Key to{
        .path = wave({450, kWide - 60}, {kBand - 60, kWide - 52}, 38, 2),
        .fill = {0, 0, 0, 0},
        .stroke = SkColor4f{1.0f, 0.3f, 0.75f, 0.9f},
        .strokeWidth = 2.5f};
    blend::draw(canvas, blend::make(from, to, {.steps = 42}));
  }
}

/** Bands 5 and 6: the same run over the same spiral, spaced by distance,
 *  differing only in what `orientation` says. */
void spined(SkCanvas& canvas, blend::Orientation orientation) {
  blend::Key from{centred(shapes::star(3, 16.0f / 34.0f), 30),
                  {1.0f, 0.9f, 0.3f, 0.95f}};
  blend::Key to{centred(shapes::star(7, 12.0f / 30.0f), 26),
                {0.4f, 0.5f, 1.0f, 0.95f}};
  // The spiral is inscribed in a square inside the cell, so both cells
  // walk one spine and only the orientation differs.
  const float side = kSpine - 40;
  blend::draw(canvas,
              blend::make(from, to,
                          {.spacing = blend::Spacing::Distance,
                           .distance = 30,
                           .spine = shapes::spiral(2.2f)
                                        .path({side, side})
                                        .makeTransform(SkMatrix::Translate(
                                            (kSpineCell - side) / 2, 20)),
                           .orientation = orientation,
                           .smoothOutlines = true}));
}

void spineUpright(SkCanvas& canvas) {
  spined(canvas, blend::Orientation::AlignToPage);
}
void spineTurned(SkCanvas& canvas) {
  spined(canvas, blend::Orientation::AlignToPath);
}

}  // namespace

struct BlendOptions {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // Every step is computed from the keys and the options; nothing here
    // reads the clock.
    sketch::kit::stage(ctx, {.size = {1200, 1390}, .captureAt = 0.05});

    ctx.composer.render(sketch::kit::page(
        {.title = "Between two outlines",
         .subtitle = "Every intermediate is a path: change its spacing, its "
                     "keys, its style, or where it travels.",
         .footer = "The blend interpolates geometry. Its steps can be filled, "
                   "stroked and composed like any other path."},
        box().column().gap(22).children(
            {explained(band(
                 "01  CHOOSE A COUNT", "steps", kBand, kRun,
                 "Options{.steps = 8}",
                 "Eight intermediate outlines connect the star and circle. "
                 "Their contours are aligned before interpolation.",
                 statedCount)),
             explained(band("02  ADD A WAYPOINT", "waypoint", kBand, kRun,
                            "make({a, b, c}, {.steps = 5, .smoothOutlines})",
                            "A third key creates two spans. The middle shape "
                            "bends the run while spacing stays regular.",
                            waypoint)),
             explained(band("03  INTERPOLATE THE STROKE", "stroke", kBand, kRun,
                            "Key{.stroke, .strokeWidth} · steps = 14",
                            "The outline, stroke colour and width interpolate "
                            "together: the run thins from 6 px to 1 px.",
                            strokes)),
             explained(band(
                 "04  LET COLOUR PICK THE DENSITY", "derived", kBand, kWide,
                 "Spacing::SmoothColor · and two OPEN keys at "
                 "steps = 42",
                 "Left: colour difference chooses the number of steps. Right: "
                 "42 steps between open paths form a ribbon.",
                 derivedCount)),
             sketch::kit::sectionHeader(
                 {.label = "05  PUT THE RUN ON A SPINE",
                  .note =
                      "Same spiral · 30 px spacing · one orientation change"}),
             sketch::kit::comparison(
                 {.cases = {band("UPRIGHT TO THE PAGE", "spine.page",
                                 kSpineCell, kSpine,
                                 "Spacing::Distance{30} · spine = spiral(2.2) "
                                 "· AlignToPage",
                                 "Even distance along the spine; every mark "
                                 "remains upright.",
                                 spineUpright),
                            band("FOLLOW THE TANGENT", "spine.path", kSpineCell,
                                 kSpine, "the same run · AlignToPath",
                                 "The same spine and spacing; each mark turns "
                                 "with the tangent.",
                                 spineTurned)},
                  .measure = 1120,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(BlendOptions, "Kit · API",
             "blend::Options — a stated step count, a third key, "
             "the stroke width carried across, SmoothColor picking its own "
             "count, and a spiral spine walked upright and turned")
