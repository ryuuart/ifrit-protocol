/** @file
 * frame_grid — a figure's own coordinate systems, and the placements
 * that are functions of numbers alone.
 *
 * A `Frame` converts (angle, radius) measured off a reference drawing
 * into a point, in the angle convention that drawing uses. It is a VALUE
 * and not a `polar()` helper because the convention is the whole point:
 * engraved and statistical plates measure clockwise from twelve
 * o'clock, Skia measures from due east, and written as a helper that
 * difference is a sign flip and a −90 repeated at every call site.
 *
 * A `Grid` is the other unit map: author in the artefact's own units and
 * multiply once. It needs scale, origin and snap together — an
 * artefact's box is rarely at the canvas origin and a pixel-art plate
 * wants its positions on a pitch — and more than one has to be alive at
 * once, which a free function cannot do without a second name.
 *
 * `arrange::` is neither. It knows nothing about what is being placed:
 * it takes the centre, the radii, the module and the gaps, answers one
 * point or one rect, and allocates nothing. That is what lets a layout
 * scheme measuring children and a routine filling a buffer of sprite
 * positions reach the same body.
 *
 * EDIT THESE FIRST
 *   kRadius — the frame's r = 1 in px.
 *   kUnits  — the Grid's canvas px per artefact unit.
 *   kSnap   — the canvas-px pitch the Grid rounds results to.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <functional>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace path = sigil::geometry::path;
namespace arrange = sigil::geometry::arrange;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 740};
constexpr float kCell = 341;
constexpr float kPicture = 232;

constexpr float kRadius = 86;  // the frame's r = 1, px
constexpr float kUnits = 7;    // the Grid's canvas px per artefact unit
constexpr float kSnap = 7;     // the pitch it rounds results to, px

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kFaint{0.28f, 0.29f, 0.34f, 1};
constexpr SkColor4f kFigure{0.88f, 0.82f, 0.66f, 1};
constexpr SkColor4f kWarm{0.96f, 0.62f, 0.30f, 1};
constexpr SkColor4f kCool{0.44f, 0.72f, 0.96f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(11, kInk),
          .note = label(10.5f, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

SkPaint strokePaint(SkColor4f color, float width) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setStyle(SkPaint::kStroke_Style);
  p.setStrokeWidth(width);
  p.setColor4f(color);
  return p;
}

SkPaint fillPaint(SkColor4f color) {
  SkPaint p;
  p.setAntiAlias(true);
  p.setColor4f(color);
  return p;
}

SkPoint middle() { return {kCell * 0.5f, kPicture * 0.5f}; }

/** The dial every frame cell is read against: the r = 1 circle and a
 *  hub, so a reading in normalised radius has something to be normal
 *  to. */
void dial(SkCanvas& canvas, const path::Frame& frame) {
  canvas.drawCircle(frame.centre, frame.radius, strokePaint(kFaint, 1.0f));
  canvas.drawCircle(frame.centre, 2.0f, fillPaint(kFaint));
}

/** A reading at (deg, rNorm): a spoke out to it, a disc on it, and the
 *  unit direction the frame says runs outward there. */
void reading(SkCanvas& canvas, const path::Frame& frame, float deg,
             SkColor4f colour) {
  const SkPoint at = frame.at(deg, 0.78f);
  canvas.drawLine(frame.centre, at, strokePaint(colour, 1.3f));
  canvas.drawCircle(at, 4.0f, fillPaint(colour));
  const SkVector dir = frame.dir(deg);
  canvas.drawLine(frame.at(deg, 0.90f),
                  {frame.at(deg, 0.90f).fX + dir.fX * 20,
                   frame.at(deg, 0.90f).fY + dir.fY * 20},
                  strokePaint(colour, 1.3f));
}

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&)> draw) {
  return kit::cell(
      voice(), toU8(call), toU8(note),
      kit::well({.width = kCell,
                 .height = kPicture,
                 .ground = Fill::color(kCellGround)},
                custom(call, [draw = std::move(draw)](SkCanvas& canvas,
                                                      const PaintContext&) {
                  draw(canvas);
                })));
}

}  // namespace

struct FrameGrid final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

    const path::Grid unit{.scale = kUnits, .origin = {30, 26}};
    const path::Grid snapped{
        .scale = kUnits, .origin = {30, 26}, .snap = kSnap};
    // One drawing in artefact units, mapped by two grids that differ in
    // one field.
    const std::vector<SkPoint> figure = [] {
      std::vector<SkPoint> p;
      for (int i = 0; i <= 40; ++i) {
        const float t = (float)i / 40.0f;
        p.push_back({t * 34.0f, 9.0f - 6.0f * std::sin(t * 6.2831853f)});
      }
      return p;
    }();

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("FRAME AND GRID \xc2\xb7 path::Frame, path::Grid, "
                           "arrange::onRing / moduleSize / cellRect"),
             .subtitle = toU8("dials \xc2\xb7 the frame's zero and sense "
                              "\xc2\xb7 the module and the gaps \xc2\xb7 the "
                              "grid's scale (7 px per unit) and snap (7 px)"),
             .footer = toU8("arrange:: knows nothing about what is being "
                            "placed \xe2\x80\x94 it takes numbers and "
                            "answers one point or one rect, which is what "
                            "lets a layout scheme and a sprite buffer reach "
                            "the same body"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {kit::cells(
                          {.cells =
                               {cell("Frame{.zero = North, .sense = CW}",
                                     "the engraver's convention \xc2\xb7 "
                                     "at(deg, rNorm) and dir(deg) read in "
                                     "the plate's own units, 0\xc2\xb0 at "
                                     "twelve o'clock",
                                     [](SkCanvas& canvas) {
                                       const path::Frame frame{
                                           .centre = middle(),
                                           .radius = kRadius};
                                       dial(canvas, frame);
                                       for (float d = 0; d < 360; d += 30)
                                         canvas.drawLine(
                                             frame.at(d, 0.90f),
                                             frame.at(d, 1.0f),
                                             strokePaint(kFaint, 1.0f));
                                       reading(canvas, frame, 0, kFigure);
                                       reading(canvas, frame, 126, kWarm);
                                     }),
                                cell("\xe2\x80\xa6"
                                     ".zero = East, "
                                     ".sense = CCW",
                                     "the SAME two numbers, 0\xc2\xb0 and "
                                     "126\xc2\xb0, in Skia's convention "
                                     "running the other way \xc2\xb7 the "
                                     "value carries it, not the call site",
                                     [](SkCanvas& canvas) {
                                       const path::Frame frame{
                                           .centre = middle(),
                                           .radius = kRadius,
                                           .zero = path::Zero::East,
                                           .sense = path::Sense::CCW};
                                       dial(canvas, frame);
                                       for (float d = 0; d < 360; d += 30)
                                         canvas.drawLine(
                                             frame.at(d, 0.90f),
                                             frame.at(d, 1.0f),
                                             strokePaint(kFaint, 1.0f));
                                       reading(canvas, frame, 0, kFigure);
                                       reading(canvas, frame, 126, kWarm);
                                     }),
                                cell("scaled(0.62) \xc2\xb7 turned(15) "
                                     "\xc2\xb7 about(c)",
                                     "derived frames inherit the "
                                     "convention, which is where it "
                                     "otherwise gets silently dropped "
                                     "\xc2\xb7 turned composes and inverts",
                                     [](SkCanvas& canvas) {
                                       const path::Frame frame{
                                           .centre = middle(),
                                           .radius = kRadius};
                                       dial(canvas, frame);
                                       const path::Frame inner =
                                           frame.scaled(0.62f);
                                       dial(canvas, inner);
                                       reading(canvas, inner, 126, kFigure);
                                       reading(canvas, frame.turned(15), 126,
                                               kWarm);
                                       const path::Frame satellite =
                                           frame.scaled(0.3f).about(
                                               frame.at(30, 0.66f));
                                       dial(canvas, satellite);
                                       reading(canvas, satellite, 126, kCool);
                                     })},
                           .gap = 14}),
                      kit::cells(
                          {.cells =
                               {cell("arrange::onRing(i, n, \xe2\x80\xa6"
                                     ", Turn)",
                                     kit::format(
                                         "seven items over 270\xc2\xb0 "
                                         "\xc2\xb7 Turn::Open steps "
                                         "%.1f\xc2\xb0 and lands on both "
                                         "ends; Turn::Closed steps "
                                         "%.1f\xc2\xb0 and stops short",
                                         (
                                             double)arrange::
                                             step(270, 7, arrange::Turn::Open),
                                         (
                                             double)arrange::
                                             step(270, 7, arrange::Turn::Closed)),
                                     [](SkCanvas& canvas) {
                                       const SkPoint c = middle();
                                       constexpr float kStart =
                                           -2.3561945f;  // 135 deg from +x
                                       constexpr float kSweep = 4.712389f;
                                       const auto ring = [&](float r,
                                                             arrange::Turn turn,
                                                             SkColor4f colour) {
                                         SkPathBuilder arc;
                                         arc.addArc(SkRect::MakeXYWH(
                                                        c.fX - r, c.fY - r,
                                                        2 * r, 2 * r),
                                                    -135, 270);
                                         canvas.drawPath(
                                             arc.detach(),
                                             strokePaint(kFaint, 1.0f));
                                         for (size_t i = 0; i < 7; ++i)
                                           canvas.drawCircle(
                                               arrange::onRing(i, 7, c, {r, r},
                                                               kStart, kSweep,
                                                               turn),
                                               5, fillPaint(colour));
                                       };
                                       ring(94, arrange::Turn::Open, kWarm);
                                       ring(56, arrange::Turn::Closed, kCool);
                                     }),
                                cell("moduleSize + cellAt + cellRect",
                                     "the module that fits 4 by 3 of itself "
                                     "plus the gaps EXACTLY into the "
                                     "container \xc2\xb7 a block spans and "
                                     "swallows the gaps it crosses",
                                     [](SkCanvas& canvas) {
                                       const SkSize container{kCell - 40,
                                                              kPicture - 40};
                                       const SkSize gap{10, 10};
                                       const SkSize module =
                                           arrange::moduleSize(container, 4, 3,
                                                               gap);
                                       const SkPoint origin{20, 20};
                                       for (size_t i = 0; i < 12; ++i)
                                         canvas.drawRect(
                                             arrange::cellRect(
                                                 arrange::cellAt(i, 4), module,
                                                 gap, origin),
                                             strokePaint(kFaint, 1.0f));
                                       canvas.drawRect(
                                           arrange::cellRect({1, 1}, module,
                                                             gap, origin, 2, 2),
                                           strokePaint(kWarm, 1.8f));
                                     }),
                                cell("Grid{.scale = 7, .snap = 0 | 7}",
                                     "one drawing in artefact units through "
                                     "two grids \xc2\xb7 s() is a LENGTH "
                                     "and takes no origin; x() and y() are "
                                     "positions and do",
                                     [unit, snapped, figure](SkCanvas& canvas) {
                                       const auto trace =
                                           [&](const path::Grid& grid,
                                               SkColor4f colour, float dy) {
                                             SkPathBuilder b;
                                             bool first = true;
                                             for (const SkPoint& p :
                                                  grid.map(figure)) {
                                               const SkPoint q{p.fX, p.fY + dy};
                                               first ? b.moveTo(q)
                                                     : b.lineTo(q);
                                               first = false;
                                             }
                                             canvas.drawPath(
                                                 b.detach(),
                                                 strokePaint(colour, 1.8f));
                                           };
                                       trace(unit, kCool, 0);
                                       trace(snapped, kWarm, 88);
                                     })},
                           .gap = 14})},
                 .column = true,
                 .gap = 18}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(FrameGrid, "Kit \xc2\xb7 API",
             "the polar frame carrying its own angle convention, the unit "
             "map carrying scale, origin and snap, and the arrangements "
             "that are functions of numbers alone")
