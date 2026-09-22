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

// TAGS: Geometry/Layout

#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Frame.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace arrange = sigil::geometry::arrange;
namespace draw = sigil::draw;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 940};
constexpr float kCell = 328;
constexpr float kPicture = 232;

constexpr float kRadius = 86;  // the frame's r = 1, px
constexpr float kUnits = 7;    // the Grid's canvas px per artefact unit
constexpr float kSnap = 7;     // the pitch it rounds results to, px

constexpr material::Color kFaint{0.28f, 0.29f, 0.34f, 1};
constexpr material::Color kFigure{0.88f, 0.82f, 0.66f, 1};
constexpr material::Color kWarm{0.96f, 0.62f, 0.30f, 1};
constexpr material::Color kCool{0.44f, 0.72f, 0.96f, 1};

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 11, .mono = true};
  return look;
}

SkPoint middle() { return {kCell * 0.5f, kPicture * 0.5f}; }

/** THE PEN A LINE OF THIS SHEET IS DRAWN WITH: a colour, a width, and no
 *  fill under it — every mark on these plates is a stroke except the discs
 *  a reading stands on. */
void pen(draw::Pen& p, material::Color colour, float width) {
  p.noFill();
  p.stroke(colour);
  p.strokeWeight(width);
}

/** The dial every frame cell is read against: the r = 1 circle and a
 *  hub, so a reading in normalised radius has something to be normal
 *  to. */
void dial(draw::Pen& p, const path::PolarFrame& frame) {
  pen(p, kFaint, 1.0f);
  p.circle(frame.centre.fX, frame.centre.fY, frame.radius * 2.0f);
  p.noStroke();
  p.fill(kFaint);
  p.circle(frame.centre.fX, frame.centre.fY, 4.0f);
}

/** A reading at (deg, rNorm): a spoke out to it, a disc on it, and the
 *  unit direction the frame says runs outward there. */
void reading(draw::Pen& p, const path::PolarFrame& frame, float deg,
             material::Color colour) {
  const SkPoint at = frame.at(deg, 0.78f);
  const SkPoint out = frame.at(deg, 0.90f);
  const SkVector dir = frame.dir(deg);
  pen(p, colour, 1.3f);
  p.line(frame.centre.fX, frame.centre.fY, at.fX, at.fY);
  p.line(out.fX, out.fY, out.fX + dir.fX * 20, out.fY + dir.fY * 20);
  p.noStroke();
  p.fill(colour);
  p.circle(at.fX, at.fY, 8.0f);
}

/** The rim's twelve ticks, from 0.90 of the radius out to the rim. */
void ticks(draw::Pen& p, const path::PolarFrame& frame) {
  pen(p, kFaint, 1.0f);
  for (float d = 0; d < 360; d += 30)
    p.line(frame.at(d, 0.90f).fX, frame.at(d, 0.90f).fY, frame.at(d, 1.0f).fX,
           frame.at(d, 1.0f).fY);
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const std::string& note,
                                 std::function<void(draw::Pen&)> drawing) {
  return {.title = title,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture},
              graphics(call,
                       [drawing = std::move(drawing)](draw::Pen& p) {
                         p.angleMode(draw::DEGREES);
                         drawing(p);
                       })),
          .note = note};
}

}  // namespace

struct FrameGrid {
  void setup(sketch::SketchContext& ctx) {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

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

    ctx.composer.render(sketch::kit::page(
        {.title = "Where a coordinate lands",
         .subtitle = "Orientation belongs to the frame. Scale, gaps and "
                     "snapping belong to the map from units to pixels.",
         .footer = "The same numeric input becomes a different position only "
                   "when its coordinate convention changes."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  CHOOSE A COORDINATE FRAME",
                  .note = "Light: 0° · amber: 126°"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("NORTH / CLOCKWISE", "zero = North · sense = CW",
                            "Zero points north; angles increase clockwise. "
                            "Both readings use 0° and 126°.",
                            [](draw::Pen& p) {
                              const path::PolarFrame frame{.centre = middle(),
                                                      .radius = kRadius};
                              dial(p, frame);
                              ticks(p, frame);
                              reading(p, frame, 0, kFigure);
                              reading(p, frame, 126, kWarm);
                            }),
                       cell("EAST / COUNTERCLOCKWISE",
                            "zero = East · sense = CCW",
                            "The same readings with zero pointing east and "
                            "angles increasing counterclockwise.",
                            [](draw::Pen& p) {
                              const path::PolarFrame frame{
                                  .centre = middle(),
                                  .radius = kRadius,
                                  .zero = path::Zero::East,
                                  .sense = path::Sense::CCW};
                              dial(p, frame);
                              ticks(p, frame);
                              reading(p, frame, 0, kFigure);
                              reading(p, frame, 126, kWarm);
                            }),
                       cell("FRAMES WITHIN FRAMES", "scaled / turned / about",
                            "Scale, rotate and relocate a frame while "
                            "retaining its angle convention.",
                            [](draw::Pen& p) {
                              const path::PolarFrame frame{.centre = middle(),
                                                      .radius = kRadius};
                              dial(p, frame);
                              const path::PolarFrame inner = frame.scaled(0.62f);
                              dial(p, inner);
                              reading(p, inner, 126, kFigure);
                              reading(p, frame.turned(15), 126, kWarm);
                              const path::PolarFrame satellite =
                                  frame.scaled(0.3f).about(frame.at(30, 0.66f));
                              dial(p, satellite);
                              reading(p, satellite, 126, kCool);
                            })},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "02  PLACE IN THAT SPACE",
                  .note = "Arrangement answers positions; it does not draw"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("OPEN OR CLOSED RUN",
                            "onRing \u00b7 7 stations over 270\u00b0",
                            kit::formatted(
                                "seven items over 270° "
                                "· Turn::Open steps "
                                "%.1f° and lands on both "
                                "ends; Turn::Closed steps "
                                "%.1f° and stops short",
                                (double)arrange::step(270,
                                                      7, arrange::Turn::Open),
                                (double)arrange::step(
                                    270,
                                    7, arrange::Turn::Closed)),
                            [](draw::Pen& p) {
                              const SkPoint c = middle();
                              constexpr float kStart =
                                  -2.3561945f;  // 135 deg from +x
                              constexpr float kSweep = 4.712389f;
                              const auto ring = [&](float r, arrange::Turn turn,
                                                    material::Color colour) {
                                pen(p, kFaint, 1.0f);
                                p.arc(c.fX, c.fY, 2 * r, 2 * r, -135, 135,
                                      draw::OPEN);
                                p.noStroke();
                                p.fill(colour);
                                for (size_t i = 0; i < 7; ++i) {
                                  const SkPoint at = arrange::onRing(
                                      i, 7, c, {r, r}, kStart, kSweep, turn);
                                  p.circle(at.fX, at.fY, 10.0f);
                                }
                              };
                              ring(94, arrange::Turn::Open, kWarm);
                              ring(56, arrange::Turn::Closed, kCool);
                            }),
                       cell("MODULES AND SPANS",
                            "moduleSize / cellAt / cellRect",
                            "A two-by-two span includes the internal gutters "
                            "of the four-by-three grid.",
                            [](draw::Pen& p) {
                              const SkSize container{kCell - 40, kPicture - 40};
                              const SkSize gap{10, 10};
                              const SkSize module =
                                  arrange::moduleSize(container, 4, 3, gap);
                              const SkPoint origin{20, 20};
                              const auto cellBox = [&](SkRect r) {
                                p.rect(r.x(), r.y(), r.width(), r.height());
                              };
                              pen(p, kFaint, 1.0f);
                              for (size_t i = 0; i < 12; ++i)
                                cellBox(arrange::cellRect(arrange::cellAt(i, 4),
                                                          module, gap, origin));
                              pen(p, kWarm, 1.8f);
                              cellBox(arrange::cellRect({1, 1}, module, gap,
                                                        origin, 2, 2));
                            }),
                       cell("CONTINUOUS OR SNAPPED", "scale = 7 · snap = 0 / 7",
                            "Cool preserves the curve. Warm rounds positions "
                            "to a 7 px lattice.",
                            [unit, snapped, figure](draw::Pen& p) {
                              const auto trace = [&](const path::Grid& grid,
                                                     material::Color colour,
                                                     float dy) {
                                pen(p, colour, 1.8f);
                                p.beginShape();
                                for (const SkPoint& at : grid.map(figure))
                                  p.vertex(at.fX, at.fY + dy);
                                p.endShape();
                              };
                              trace(unit, kCool, 0);
                              trace(snapped, kWarm, 88);
                            })},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(FrameGrid, "Kit · API",
             "the polar frame carrying its own angle convention, the unit "
             "map carrying scale, origin and snap, and the arrangements "
             "that are functions of numbers alone")
