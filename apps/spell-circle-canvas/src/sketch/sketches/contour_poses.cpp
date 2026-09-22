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

// TAGS: Geometry/Paths

#include <include/core/SkPathBuilder.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/draw/Draw.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigildraw/Pen.h>
#include <sigilgeometry/kit/Generators.h>
#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Pose.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <functional>
#include <span>
#include <string>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace path = sigil::geometry::path;
namespace shapes = sigil::geometry::shapes;
namespace arrange = sigil::geometry::arrange;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 940};
constexpr float kCell = 328;
constexpr float kPicture = 236;

constexpr int kStations = 24;     // poses cut out of the whole run
constexpr float kCornerDeg = 30;  // the turn that counts as a corner
constexpr float kWindow = 26;     // a corner window's reach, px

constexpr material::Color kFaint{0.28f, 0.29f, 0.34f, 1};
constexpr material::Color kFigure{0.86f, 0.80f, 0.66f, 1};
constexpr material::Color kWarm{0.95f, 0.62f, 0.30f, 1};
constexpr material::Color kCool{0.44f, 0.70f, 0.95f, 1};

/** The specimen sheet, in this one's caption voice. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.type.captionLabel = {.size = 12, .track = 1.2f};
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

using sigil::draw::Pen;

/** The pen set for a stroked mark, and for a filled one. */
void inked(Pen& pen, material::Color color, float width) {
  pen.noFill();
  pen.stroke(color);
  pen.strokeWeight(width);
}
void filled(Pen& pen, material::Color color) {
  pen.noStroke();
  pen.fill(color);
}

/** The outline itself, faint, under every cell that marks something on
 *  it. */
void ghost(Pen& pen, const SkPath& outline) {
  inked(pen, kFaint, 1.2f);
  pen.shape(outline);
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const std::string& note,
                                 std::function<void(Pen&)> draw) {
  return {.title = title,
          .control = call,
          .figure = sketch::kit::well(
              {.width = kCell, .height = kPicture, .clip = false},
              pen(call, [draw = std::move(draw)](Pen& pen) { draw(pen); })),
          .note = note};
}

}  // namespace

struct ContourPoses {
  void setup(sketch::SketchContext& ctx) {
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
        {.title = "Read a path by distance",
         .subtitle = "A contour turns one outline into a continuous "
                     "coordinate: position, direction and neighbourhood.",
         .footer = "The ring marks the seam. Every view queries the same "
                   "measured path."},
        box().column().gap(20).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  DISTANCE ALONG THE OUTLINE",
                  .note = "One measurement shared by every view"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("THE MEASURED CONTOUR", "Contour::of(path)",
                            kit::formatted(
                                "%zu contour · closed %s "
                                "· totalLength %.1f px "
                                "· seam ringed",
                                contours.size(),
                                path::closedThroughout(run) ? "yes" : "no",
                                (double)total),
                            [figure, contours](Pen& pen) {
                              const std::span<const path::Contour> run{
                                  contours};
                              inked(pen, kFigure, 2.0f);
                              pen.shape(figure);
                              const path::Pose head =
                                  path::poseAlong(run, 0.0f);
                              const glm::vec2 tip =
                                  head.position + head.tangent * 22.0f;
                              inked(pen, kWarm, 1.6f);
                              pen.circle(head.position.x, head.position.y, 10);
                              pen.line(head.position.x, head.position.y, tip.x,
                                       tip.y);
                            }),
                       cell("LOCAL COORDINATES", "poseAlong(d).normal",
                            "Twenty-four equal stations. Every tick follows "
                            "the local normal.",
                            [figure, contours, total](Pen& pen) {
                              const std::span<const path::Contour> run{
                                  contours};
                              ghost(pen, figure);
                              for (int i = 0; i < kStations; ++i) {
                                const float d = arrange::along(
                                    0.0f, total, (size_t)i, (size_t)kStations,
                                    arrange::Turn::Closed);
                                const path::Pose p = path::poseAlong(run, d);
                                const glm::vec2 from =
                                    p.position - p.normal * 4.0f;
                                const glm::vec2 to =
                                    p.position + p.normal * 13.0f;
                                inked(pen, kFigure, 1.8f);
                                pen.line(from.x, from.y, to.x, to.y);
                                filled(pen, kWarm);
                                pen.circle(p.position.x, p.position.y, 3.6f);
                              }
                            }),
                       cell("OUTSIDE THE RANGE",
                            "Clamp / Around · −0.2 to 1.2 turns",
                            "Warm clamps at the ends. Cool wraps across the "
                            "seam.",
                            [figure, contours, total](Pen& pen) {
                              const std::span<const path::Contour> run{
                                  contours};
                              ghost(pen, figure);
                              // Each policy's twelve stations
                              // joined in order: where a chain
                              // stalls, several distances have
                              // resolved to one place.
                              SkPathBuilder parked, round;
                              for (int i = 0; i < 12; ++i) {
                                const float f = -0.2f + 1.4f * (float)i / 11.0f;
                                const path::Pose clamped = path::poseAlong(
                                    run, f * total, path::Wrap::Clamp);
                                const path::Pose around = path::poseAlong(
                                    run, f * total, path::Wrap::Around);
                                const glm::vec2 out =
                                    clamped.position + clamped.normal * 11.0f;
                                const glm::vec2 in =
                                    around.position - around.normal * 11.0f;
                                const SkPoint outAt{out.x, out.y};
                                const SkPoint inAt{in.x, in.y};
                                (i ? parked.lineTo(outAt)
                                   : parked.moveTo(outAt));
                                (i ? round.lineTo(inAt) : round.moveTo(inAt));
                                filled(pen, kWarm);
                                pen.circle(out.x, out.y, 7.2f);
                                filled(pen, kCool);
                                pen.circle(in.x, in.y, 7.2f);
                              }
                              inked(pen, kWarm, 1.0f);
                              pen.shape(parked.detach());
                              inked(pen, kCool, 1.0f);
                              pen.shape(round.detach());
                            })},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "02  SELECT AROUND A TURN",
                  .note = "Cool: incoming / retained span · warm: outgoing / "
                          "corner window"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("FIND THE CORNERS", "Contour::corners(30°)",
                            kit::formatted("%zu corners · "
                                           "sharpest turn "
                                           "%.0f° · each "
                                           "drawn "
                                           "as its in tangent "
                                           "and its out "
                                           "tangent",
                                           corners.size(), (double)sharpest),
                            [figure, corners, contours](Pen& pen) {
                              const std::span<const path::Contour> run{
                                  contours};
                              ghost(pen, figure);
                              for (const path::Contour::Corner& c : corners) {
                                const path::Pose p =
                                    path::poseAlong(run, c.distance);
                                const glm::vec2 from =
                                    p.position - c.in * 18.0f;
                                const glm::vec2 to = p.position + c.out * 18.0f;
                                inked(pen, kCool, 1.6f);
                                pen.line(from.x, from.y, p.position.x,
                                         p.position.y);
                                inked(pen, kWarm, 1.6f);
                                pen.line(p.position.x, p.position.y, to.x,
                                         to.y);
                                inked(pen, kFigure, 1.4f);
                                pen.circle(p.position.x, p.position.y, 6.4f);
                              }
                            }),
                       cell("KEEP THEIR WINDOWS",
                            "cornerWindows(26, true, 30°)",
                            "Keep the outline within 26 px of a detected turn.",
                            [figure](Pen& pen) {
                              ghost(pen, figure);
                              inked(pen, kWarm, 3.0f);
                              pen.shape(path::cornerWindows(figure, kWindow,
                                                            true, kCornerDeg));
                            }),
                       cell("KEEP THE COMPLEMENT",
                            "cornerWindows(26, false, 30°)",
                            "Keep the complementary stretches between the "
                            "turns.",
                            [figure](Pen& pen) {
                              ghost(pen, figure);
                              inked(pen, kCool, 3.0f);
                              pen.shape(path::cornerWindows(figure, kWindow,
                                                            false, kCornerDeg));
                            })},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(ContourPoses, "Kit · API",
             "one outline measured once and read as a coordinate: poses at "
             "stations along it, the two wrap policies, its corners and the "
             "windows around them")
