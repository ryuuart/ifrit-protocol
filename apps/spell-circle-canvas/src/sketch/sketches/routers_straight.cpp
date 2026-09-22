/** @file
 * routers_straight — one pair of anchors, every stock route between them.
 *
 * A `Router` is a plain function of the two endpoint RECTS returning the
 * routed path, and a `RailRouter` the same over an ordered run of anchor
 * points. There is no enum of route kinds and no route object: these are
 * the stock values, and a caller's own function is a peer of them. A
 * connecting operator carries one and attaches the wire it routes; the
 * routed path becomes that wire's own outline, so any PathFormat dresses
 * it.
 *
 * The two that are easy to confuse are `orthogonal`'s bends. `MidX` is
 * the Z every node-graph editor defaults to — half way over, one vertical
 * run, half way in. `HFirst` and `VFirst` are the two Ls: bend AT the
 * target column, or bend AT the source column. A circuit trace bends at
 * the target column; a flowchart drops out of the source first.
 *
 * `orthogonal()` with no arguments is NOT the bend overload with
 * defaults. It emits its degenerate verbs verbatim and that output is
 * frozen, because existing routes depend on it byte for byte; the bend
 * form is the spelling for new work, and it collapses collinear points so
 * an axis-aligned pair emits ONE segment rather than three with
 * zero-length ends.
 *
 * A corner either ROUNDS or is CUT at 45°, and the cut wins when both are
 * set. Octilinear is a RailRouter, not a Router: it wants the whole
 * anchor run, so it is reached through `connect::Along` and never
 * through `connect::Between`.
 *
 * EDIT THESE FIRST
 *   kRadius — the corner radius the rounded routes take, px.
 *   kChamfer — the 45° cut, which wins over a radius when both are set.
 *   kBulge — the arc router's bulge, as a fraction of the chord.
 */

// TAGS: Geometry/Diagrams

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Connect.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 900};
constexpr float kCell = 328;
constexpr float kPicture = 226;
constexpr float kNode = 66;

constexpr float kRadius = 12;    // the corner radius, px
constexpr float kChamfer = 14;   // the 45 degree cut, which wins over a radius
constexpr float kBulge = 0.26f;  // the arc's bulge, as a fraction of the chord

constexpr material::Color kNodeFill{0.17f, 0.18f, 0.21f, 1};

/** The two nodes every cell routes between, at the same two places in
 *  every cell, so the ROUTER is the only thing that differs. */
Element endpoint(const std::string& key, float x, float y) {
  return kit::at(box().key(key).fill(Fill::color(kNodeFill)), x, y, kNode, 28);
}

Element plate(const std::string& tag, Operator route) {
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children({stack()
                     .inset(0)
                     .operators({std::move(route)})
                     .children({endpoint(tag + "-a", 16, 26),
                                // The two are deliberately NOT on a 45
                                // degree chord: an octilinear leg would
                                // otherwise consume the whole run and read
                                // as a straight line.
                                endpoint(tag + "-b", kCell - kNode - 16,
                                         kPicture - 28 - 62)})});
}

/** The mark every cell's wire is drawn with, so the router is the only
 *  thing that differs between them. */
PathFormat wireMark() {
  return PathFormat{
      .width = 1.6f,
      .strokeFill = Fill::color(sketch::kit::theme().palette.figure)};
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, const std::string& tag,
                                 Operator route) {
  return {.title = title,
          .control = call,
          .figure = plate(tag, std::move(route)),
          .note = note};
}

}  // namespace

struct RoutersStraight {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const auto wire = [](const std::string& tag, Router router) {
      return Operator(connect::Between{.from = tag + "-a",
                                       .to = tag + "-b",
                                       .router = std::move(router),
                                       .gap = 4,
                                       .wire = wireMark()});
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "Routes between two anchors",
         .subtitle =
             "Hold the endpoints still and change the path between them.",
         .footer = "A wire between two nodes consumes their two endpoint "
                   "rectangles. A wire along a run consumes every stop in "
                   "it."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  A CONNECTOR BETWEEN TWO RECTANGLES",
                  .note = "Identical endpoints · three ways to cross the gap"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("DIRECT", "straight() · gap = 4",
                            "A direct chord joins the same two endpoint "
                            "rectangles.",
                            "st", wire("st", routers::straight())),
                       cell("ORTHOGONAL", "orthogonal(MidX)",
                            "Split the horizontal distance with a vertical run "
                            "in the middle.",
                            "mx",
                            wire("mx",
                                 routers::orthogonal(routers::Bend::MidX))),
                       cell("CURVED", "arc(0.26)",
                            "Bow the chord by 26 per cent of its own length.",
                            "ar", wire("ar", routers::arc(kBulge)))},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::sectionHeader(
                 {.label = "02  CONTROL THE TURN",
                  .note = "A corner belongs to the route, not the node"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("ROUND AN L", "orthogonal(HFirst, 12)",
                            "Travel horizontally first; round the corner at "
                            "the target column.",
                            "hf",
                            wire("hf", routers::orthogonal(
                                           routers::Bend::HFirst, kRadius))),
                       cell("CHAMFER AN L", "orthogonal(VFirst, 0, 14)",
                            "Travel vertically first; cut the corner at 45°.",
                            "vf",
                            wire("vf",
                                 routers::orthogonal(routers::Bend::VFirst, 0,
                                                     kChamfer))),
                       cell("FOLLOW AN ANCHOR RUN",
                            "Along{…, octilinear(8)}",
                            "A run of stops is followed with a 45° leg and "
                            "a straight remainder.",
                            "oc",
                            Operator(connect::Along{
                                .stops = {Anchor{"oc-a", {0.5f, 0.5f}, 4},
                                          Anchor{"oc-b", {0.5f, 0.5f}, 4}},
                                .router = routers::octilinear(8),
                                .wire = wireMark()}))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(RoutersStraight, "Kit · API",
             "the same two anchors routed straight, as both orthogonal Ls "
             "and the Z between them, bowed as an arc, and threaded "
             "octilinearly along a run of stops")
