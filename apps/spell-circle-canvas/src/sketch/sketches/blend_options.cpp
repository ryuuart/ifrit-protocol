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

#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilgeometry/path/blend/Blend.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace blend = sigil::geometry::path::blend;
namespace shapes = sigil::geometry::shapes;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr float kBand = 1140;     // a full-width band's drawn width, px
constexpr float kRun = 150;       // the height of a run band
constexpr float kWide = 210;      // the height of the derived-count band
constexpr float kSpine = 300;     // one spine cell, square-ish
constexpr float kSpineCell = 561; // (kBand - the gap between the two) / 2

constexpr SkColor4f kGround{0.055f, 0.055f, 0.075f, 1};
constexpr SkColor4f kCellGround{0.085f, 0.085f, 0.105f, 1};
constexpr SkColor4f kInk{0.90f, 0.91f, 0.94f, 1};
constexpr SkColor4f kAsh{0.56f, 0.58f, 0.66f, 1};
constexpr SkColor4f kRule{0.19f, 0.20f, 0.24f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(12, kInk, 0.6f),
          .note = label(11, kAsh, 0.3f),
          .gap = 7,
          .noteMeasure = kBand};
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
    const float t = (float)i / (float)n;
    const float x = from.fX + (to.fX - from.fX) * t;
    const float y = from.fY + (to.fY - from.fY) * t +
                    amplitude * std::sin(t * (float)cycles * 6.2831853f);
    i == 0 ? (void)b.moveTo({x, y}) : (void)b.lineTo({x, y});
  }
  return b.detach();
}

using Painter = void (*)(SkCanvas&);

Element band(std::string key, float width, float height, const char* call,
             const char* note, Painter paint) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   custom(std::move(key),
                          [paint](SkCanvas& canvas, const PaintContext&) {
                            paint(canvas);
                          })
                       .width(width)
                       .height(height)
                       .fill(Fill::color(kCellGround)));
}

// 1 — the two-key run at a stated count.
void statedCount(SkCanvas& canvas) {
  blend::Key from{at(shapes::star(5, 30.0f / 70.0f), 62, {80, kRun / 2}),
                  {1.0f, 0.42f, 0.30f, 1}};
  blend::Key to{at(shapes::circle(), 56, {kBand - 80, kRun / 2}),
                {0.30f, 0.62f, 1.0f, 1}};
  blend::Options options;
  options.steps = 8;
  blend::draw(canvas, blend::make(from, to, options));
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
  blend::Options options;
  options.steps = 5;
  options.smoothOutlines = true;
  blend::draw(canvas, blend::make(keys, options));
}

// 3 — stroke width and stroke colour interpolate as well.
void strokes(SkCanvas& canvas) {
  blend::Key from{at(shapes::star(6, 40.0f / 72.0f), 62, {80, kRun / 2}),
                  {0, 0, 0, 0}};
  from.stroke = SkColor4f{0.2f, 0.9f, 1.0f, 1};
  from.strokeWidth = 6;
  blend::Key to{at(shapes::circle(), 56, {kBand - 80, kRun / 2}),
                {0, 0, 0, 0}};
  to.stroke = SkColor4f{1.0f, 0.35f, 0.75f, 1};
  to.strokeWidth = 1;
  blend::Options options;
  options.steps = 14;
  options.smoothOutlines = true;
  blend::draw(canvas, blend::make(from, to, options));
}

// 4 — the count is a consequence of the colours, not an input; and the
// same question asked of two open runs at a stated count.
void derivedCount(SkCanvas& canvas) {
  {
    blend::Key from{at(shapes::squircle(3.2f), 96, {200, kWide / 2}),
                    {0.08f, 0.10f, 0.35f, 1}};
    blend::Key to{at(shapes::circle(), 16, {222, kWide / 2 - 18}),
                  {1.0f, 0.95f, 0.55f, 1}};
    blend::Options options;
    options.spacing = blend::Spacing::SmoothColor;
    options.smoothOutlines = true;
    blend::draw(canvas, blend::make(from, to, options));
  }
  {
    blend::Key from{wave({470, 40}, {kBand - 40, 52}, 24, 3), {0, 0, 0, 0}};
    from.stroke = SkColor4f{0.15f, 0.85f, 1.0f, 0.9f};
    from.strokeWidth = 2.5f;
    blend::Key to{wave({450, kWide - 40}, {kBand - 60, kWide - 30}, 38, 2),
                  {0, 0, 0, 0}};
    to.stroke = SkColor4f{1.0f, 0.3f, 0.75f, 0.9f};
    to.strokeWidth = 2.5f;
    blend::Options options;
    options.steps = 42;
    blend::draw(canvas, blend::make(from, to, options));
  }
}

/** Bands 5 and 6: the same run over the same spiral, spaced by distance,
 *  differing only in what `orientation` says. */
void spined(SkCanvas& canvas, blend::Orientation orientation) {
  blend::Key from{centred(shapes::star(3, 16.0f / 34.0f), 30),
                  {1.0f, 0.9f, 0.3f, 0.95f}};
  blend::Key to{centred(shapes::star(7, 12.0f / 30.0f), 26),
                {0.4f, 0.5f, 1.0f, 0.95f}};
  blend::Options options;
  options.spacing = blend::Spacing::Distance;
  options.distance = 30;
  // The spiral is inscribed in a square inside the cell, so both cells
  // walk one spine and only the orientation differs.
  const float side = kSpine - 40;
  options.spine = shapes::spiral(2.2f).path({side, side}).makeTransform(
      SkMatrix::Translate((kSpineCell - side) / 2, 20));
  options.orientation = orientation;
  options.smoothOutlines = true;
  blend::draw(canvas, blend::make(from, to, options));
}

void spineUpright(SkCanvas& canvas) {
  spined(canvas, blend::Orientation::AlignToPage);
}
void spineTurned(SkCanvas& canvas) {
  spined(canvas, blend::Orientation::AlignToPath);
}

}  // namespace

struct BlendOptions final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1200, 1300);
    ctx.background(kGround);
    // Every step is computed from the keys and the options; nothing here
    // reads the clock.
    ctx.captureAt(0.05);

    std::vector<Element> bands;
    bands.push_back(band("steps", kBand, kRun, "Options{.steps = 8}",
                         "a count the author picks \xe2\x80\x94 the arms "
                         "shorten and the hub swells, because the contours "
                         "are aligned before anything is interpolated",
                         statedCount));
    bands.push_back(band("waypoint", kBand, kRun,
                         "make({a, b, c}, {.steps = 5, .smoothOutlines})",
                         "a third key splits the spine into one span per "
                         "PAIR: the run bends without its spacing changing",
                         waypoint));
    bands.push_back(band("stroke", kBand, kRun,
                         "Key{.stroke, .strokeWidth} \xc2\xb7 steps = 14",
                         "a key with no fill carries its stroke WIDTH "
                         "across too, so the run thins from 6 px to 1",
                         strokes));
    bands.push_back(
        band("derived", kBand, kWide,
             "Spacing::SmoothColor \xc2\xb7 and two OPEN keys at steps = 42",
             "left: no count is named \xe2\x80\x94 the blend picks one so "
             "adjacent steps differ by less than the eye resolves. right: "
             "an open path has no inside, so the count decides between a "
             "ribbon and rails",
             derivedCount));

    Element spineRow = kit::cells(
        {.cells = {band("spine.page", kSpineCell, kSpine,
                        "Spacing::Distance{30} \xc2\xb7 spine = spiral(2.2) "
                        "\xc2\xb7 AlignToPage",
                        "the walk is measured in px of SPINE, so the "
                        "density holds where a count would crowd the middle "
                        "and starve the rim; the beads stay upright",
                        spineUpright),
                   band("spine.path", kSpineCell, kSpine,
                        "the same run \xc2\xb7 AlignToPath",
                        "each step turns to the tangent \xe2\x80\x94 beads "
                        "on a wire, against confetti on a line beside it",
                        spineTurned)},
         .gap = 18});
    bands.push_back(std::move(spineRow));

    ctx.composer.render(
        kit::sheet({.title = toU8("BLEND OPTIONS \xc2\xb7 how many steps, "
                                 "what rides along, and where they walk"),
                    .subtitle = toU8("path::blend interpolates OUTLINES: "
                                     "every intermediate is a real path"),
                    .footer = toU8("Sketchbook \xc2\xb7 blend_options"),
                    .titleStyle = label(15, kInk, 2.2f),
                    .subtitleStyle = label(11, kAsh, 0.7f),
                    .footerStyle = label(10, kAsh, 0.3f),
                    .marginX = 30,
                    .marginTop = 24,
                    .marginBottom = 18,
                    .ground = Fill::color(kGround),
                    .rule = Fill::color(kRule)},
                   kit::cells({.cells = std::move(bands),
                               .column = true,
                               .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(BlendOptions, "Kit \xc2\xb7 API",
             "blend::Options \xe2\x80\x94 a stated step count, a third key, "
             "the stroke width carried across, SmoothColor picking its own "
             "count, and a spiral spine walked upright and turned")
