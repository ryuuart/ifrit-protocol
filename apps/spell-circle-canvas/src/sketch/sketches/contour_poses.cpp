/** @file
 * contour_poses — one outline read as a coordinate, station by station.
 *
 * A `Contour` measures a sub-path ONCE and then answers every query as a
 * distance along it, so two constructions that ask for the same distance
 * land on the same point. `poseAlong` adds the two conventions every
 * caller placing something along a curve would otherwise write for
 * itself: the sideways direction (`Pose::normal`, a quarter turn toward
 * +y, which in Skia's y-down space is the RIGHT of travel), and what a
 * distance outside the curve means — `Wrap::Clamp` parks at the nearer
 * end, `Wrap::Around` comes round, and only on closed geometry.
 *
 * The span overload walks a whole PATH as one coordinate: every contour
 * in order, each starting where the last ended, with `totalLength` as
 * that coordinate's extent. So a figure cut into several pieces still
 * carries one continuous measure, which is what a run of stamps needs.
 *
 * Corners are found the same way — by walking, not by reading segment
 * types — so a corner is a turn sharper than an angle rather than a
 * vertex the path happens to record, and `cornerWindows` returns the
 * PIECES of the outline near those turns, or everything else.
 *
 * EDIT THESE FIRST
 *   kStations   — how many poses the run is cut into.
 *   kCornerDeg  — the turn, in degrees, that counts as a corner.
 *   kWindow     — the reach of a corner window, px.
 */

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Pose.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <span>
#include <string>
#include <vector>

namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace arrange = sigil::geometry::arrange;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 772};
constexpr float kCell = 340;
constexpr float kPicture = 252;

constexpr int kStations = 24;     // poses cut out of the whole run
constexpr float kCornerDeg = 30;  // the turn that counts as a corner
constexpr float kWindow = 26;     // a corner window's reach, px

constexpr SkColor4f kFaint{0.28f, 0.29f, 0.34f, 1};
constexpr SkColor4f kFigure{0.86f, 0.80f, 0.66f, 1};
constexpr SkColor4f kWarm{0.95f, 0.62f, 0.30f, 1};
constexpr SkColor4f kCool{0.44f, 0.70f, 0.95f, 1};

/** The house sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.type.captionLabel = {.size = 12, .track = 1.2f};
  look.type.captionNote = {.size = 10.5f, .mono = true};
  look.spacing.captionGap = 8;
  return look;
}

/** The subject: a five-pointed star with bowed arms, centred in the
 *  picture. Ten real corners, alternating sharp and shallow, which is
 *  what makes the corner threshold visible as a threshold. */
SkPath subject() {
  const float art = kPicture - 56;
  return shapes::star(5, 0.46f, 0.13f)
      .path({art, art})
      .makeTransform(
          SkMatrix::Translate((kCell - art) * 0.5f, (kPicture - art) * 0.5f));
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

SkPoint sk(glm::vec2 v) { return {v.x, v.y}; }

/** The outline itself, faint, under every cell that marks something on
 *  it. */
void ghost(SkCanvas& canvas, const SkPath& p) {
  canvas.drawPath(p, strokePaint(kFaint, 1.2f));
}

Element cell(const char* call, const std::string& note,
             std::function<void(SkCanvas&)> draw) {
  return sketch::kit::caption(
      kCell, toU8(call), toU8(note),
      sketch::kit::well(
          {.width = kCell, .height = kPicture, .clip = false},
          custom(call, [draw = std::move(draw)](SkCanvas& canvas,
                                                const PaintContext&) {
            draw(canvas);
          })));
}

}  // namespace

struct ContourPoses final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    // nothing moves; the sheet is complete at once
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const SkPath figure = subject();
    const std::vector<path::Contour> contours = path::Contour::of(figure);
    const float total = path::totalLength(contours);
    const std::span<const path::Contour> run{contours};

    float sharpest = 0;
    const std::vector<path::Contour::Corner> corners =
        contours.empty()
            ? std::vector<path::Contour::Corner>{}
            : contours.front().corners(kCornerDeg, 6.0f, 2.0f, &sharpest);

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("CONTOUR POSES \xc2\xb7 Contour::of + poseAlong + "
                       "corners + cornerWindows"),
         .subtitle = toU8("dials \xc2\xb7 the station count (24) "
                          "\xc2\xb7 the corner angle (30\xc2\xb0) "
                          "\xc2\xb7 the window reach (26 px)"),
         .footer = toU8("one measurement, taken in Contour::of and "
                        "shared by every copy \xe2\x80\x94 each cell "
                        "below asks the same run of contours a "
                        "different question about the same distances")},
        kit::cells(
            {.cells =
                 {kit::cells(
                      {.cells =
                           {cell("Contour::of(path)",
                                 kit::format(
                                     "%zu contour \xc2\xb7 closed %s "
                                     "\xc2\xb7 totalLength %.1f px "
                                     "\xc2\xb7 seam ringed",
                                     contours
                                         .size(),
                                     path::closedThroughout(run) ? "yes"
                                                                 : "no",
                                     (
                                         double)total),
                                 [figure, contours](SkCanvas& canvas) {
                                   const std::span<const path::Contour> run{
                                       contours};
                                   canvas.drawPath(figure,
                                                   strokePaint(kFigure, 2.0f));
                                   const path::Pose head =
                                       path::poseAlong(run, 0.0f);
                                   canvas.drawCircle(sk(head.position), 5,
                                                     strokePaint(kWarm, 1.6f));
                                   const glm::vec2 tip =
                                       head.position + head.tangent * 22.0f;
                                   canvas.drawLine(sk(head.position), sk(tip),
                                                   strokePaint(kWarm, 1.6f));
                                 }),
                            cell("poseAlong(contours, d) \xc2\xb7 "
                                 "Pose::normal",
                                 kit::format(
                                     "%d stations by arrange::along(0, "
                                     "%.0f, i, n, Turn::Closed) "
                                     "\xc2\xb7 each tick on the pose's "
                                     "normal",
                                     kStations, (double)total),
                                 [figure, contours, total](SkCanvas& canvas) {
                                   const std::span<const path::Contour> run{
                                       contours};
                                   ghost(canvas, figure);
                                   for (int i = 0; i < kStations; ++i) {
                                     const float d =
                                         arrange::along(0.0f, total, (size_t)i,
                                                        (size_t)kStations,
                                                        arrange::Turn::Closed);
                                     const path::Pose p =
                                         path::poseAlong(run, d);
                                     canvas.drawLine(
                                         sk(p.position - p.normal * 4.0f),
                                         sk(p.position + p.normal * 13.0f),
                                         strokePaint(kFigure, 1.8f));
                                     canvas.drawCircle(sk(p.position), 1.8f,
                                                       fillPaint(kWarm));
                                   }
                                 }),
                            cell("Wrap::Clamp vs Wrap::Around",
                                 kit::format("the same 12 distances from "
                                             "\xe2\x88\x92"
                                             "0.2 to 1.2 of "
                                             "totalLength, joined in order "
                                             "\xc2\xb7 the outer chain parks "
                                             "at the ends, the inner one comes "
                                             "round the seam"),
                                 [figure, contours, total](SkCanvas& canvas) {
                                   const std::span<const path::Contour> run{
                                       contours};
                                   ghost(canvas, figure);
                                   // Each policy's twelve stations
                                   // joined in order: where a chain
                                   // stalls, several distances have
                                   // resolved to one place.
                                   SkPathBuilder parked, round;
                                   for (int i = 0; i < 12; ++i) {
                                     const float f =
                                         -0.2f + 1.4f * (float)i / 11.0f;
                                     const path::Pose clamped = path::poseAlong(
                                         run, f * total, path::Wrap::Clamp);
                                     const path::Pose around = path::poseAlong(
                                         run, f * total, path::Wrap::Around);
                                     const SkPoint out =
                                         sk(clamped.position +
                                            clamped.normal * 11.0f);
                                     const SkPoint in =
                                         sk(around.position -
                                            around.normal * 11.0f);
                                     (i ? parked.lineTo(out)
                                        : parked.moveTo(out));
                                     (i ? round.lineTo(in) : round.moveTo(in));
                                     canvas.drawCircle(out, 3.6f,
                                                       fillPaint(kWarm));
                                     canvas.drawCircle(in, 3.6f,
                                                       fillPaint(kCool));
                                   }
                                   canvas.drawPath(parked.detach(),
                                                   strokePaint(kWarm, 1.0f));
                                   canvas.drawPath(round.detach(),
                                                   strokePaint(kCool, 1.0f));
                                 })},
                       .gap = 14}),
                  kit::cells(
                      {.cells =
                           {cell("Contour::corners(30\xc2\xb0)",
                                 kit::format(
                                     "%zu corners \xc2\xb7 sharpest turn "
                                     "%.0f\xc2\xb0 \xc2\xb7 each drawn "
                                     "as its in tangent and its out "
                                     "tangent",
                                     corners.size(), (double)sharpest),
                                 [figure, corners, contours](SkCanvas& canvas) {
                                   const std::span<const path::Contour> run{
                                       contours};
                                   ghost(canvas, figure);
                                   for (const path::Contour::Corner& c :
                                        corners) {
                                     const path::Pose p =
                                         path::poseAlong(run, c.distance);
                                     canvas.drawLine(
                                         sk(p.position - c.in * 18.0f),
                                         sk(p.position),
                                         strokePaint(kCool, 1.6f));
                                     canvas.drawLine(
                                         sk(p.position),
                                         sk(p.position + c.out * 18.0f),
                                         strokePaint(kWarm, 1.6f));
                                     canvas.drawCircle(
                                         sk(p.position), 3.2f,
                                         strokePaint(kFigure, 1.4f));
                                   }
                                 }),
                            cell("cornerWindows(26, true, 30\xc2\xb0)",
                                 "the pieces of the outline WITHIN the "
                                 "window of a corner, kept",
                                 [figure](SkCanvas& canvas) {
                                   ghost(canvas, figure);
                                   canvas.drawPath(
                                       path::cornerWindows(figure, kWindow,
                                                           true, kCornerDeg),
                                       strokePaint(kWarm, 3.0f));
                                 }),
                            cell("cornerWindows(26, false, 30\xc2\xb0)",
                                 "the complement \xe2\x80\x94 everything "
                                 "the windows did not claim, which is "
                                 "the run a straight ornament may take",
                                 [figure](SkCanvas& canvas) {
                                   ghost(canvas, figure);
                                   canvas.drawPath(
                                       path::cornerWindows(figure, kWindow,
                                                           false, kCornerDeg),
                                       strokePaint(kCool, 3.0f));
                                 })},
                       .gap = 14})},
             .column = true,
             .gap = 18})));
  }
};

SIGIL_SKETCH(ContourPoses, "Kit \xc2\xb7 API",
             "one outline measured once and read as a coordinate: poses at "
             "stations along it, the two wrap policies, its corners and the "
             "windows around them")
