/** @file
 * routers_straight — one pair of anchors, every stock route between them.
 *
 * A `Router` is a plain function of the two endpoint RECTS returning the
 * routed path, and a `RailRouter` the same over an ordered run of anchor
 * points. There is no enum of route kinds and no route object: these are
 * the stock values, and a caller's own function is a peer of them. The
 * routed path arrives as the connector's `PaintContext::outline`, so any
 * PathFormat dresses it.
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
 * anchor run, so it is reached through `rail()` and never through
 * `connector()`.
 *
 * EDIT THESE FIRST
 *   kRadius — the corner radius the rounded routes take, px.
 *   kChamfer — the 45° cut, which wins over a radius when both are set.
 *   kBulge — the arc router's bulge, as a fraction of the chord.
 */

// TAGS: Geometry/Diagrams

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Routers.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 163;
constexpr float kPicture = 176;
constexpr float kNode = 46;

constexpr float kRadius = 12;    // the corner radius, px
constexpr float kChamfer = 14;   // the 45 degree cut, which wins over a radius
constexpr float kBulge = 0.26f;  // the arc's bulge, as a fraction of the chord

constexpr SkColor4f kNodeFill{0.17f, 0.18f, 0.21f, 1};

/** The two nodes every cell routes between, at the same two places in
 *  every cell, so the ROUTER is the only thing that differs. */
Element endpoint(const std::string& key, float x, float y) {
  return kit::at(box().key(key).fill(Fill::color(kNodeFill)), x, y, kNode, 28);
}

Element plate(const std::string& tag, Element route) {
  const PathFormat wire{
      .width = 1.6f,
      .strokeFill = Fill::color(sketch::kit::theme().palette.figure)};
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children(
          {stack().inset(0).children(
               {endpoint(tag + "-a", 16, 26),
                // The two are deliberately NOT on a 45 degree chord:
                // an octilinear leg would otherwise consume the whole
                // run and read as a straight line.
                endpoint(tag + "-b", kCell - kNode - 16, kPicture - 28 - 62)}),
           std::move(route).inset(0).foreground(wire)});
}

Element cell(const char* call, const char* note, const std::string& tag,
             Element route) {
  return sketch::kit::caption(kCell, call, note, plate(tag, std::move(route)));
}

}  // namespace

struct RoutersStraight {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::specimenTheme());
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    const auto wire = [](const std::string& tag, Router router) {
      return connector(tag + "-a", tag + "-b", std::move(router), 4);
    };

    ctx.composer.render(sketch::kit::page(
        {.title = "The stock routes",
         .subtitle = "dials · the router · the bend "
                     "(MidX, HFirst, VFirst) · the corner "
                     "radius (12 px) or the 45° cut (14 px, "
                     "which wins) · the arc's bulge (0.26 of "
                     "the chord)",
         .footer = "a Router is a function of the two endpoint "
                   "rects and a RailRouter one over the whole "
                   "anchor run — which is why octilinear "
                   "is reached through rail() and never through "
                   "connector()"},
        kit::cells(
            {.cells =
                 {cell("routers::straight()",
                       "centre to centre · the connector default, "
                       "here as a named value, with a 4 px gap pulling "
                       "each end back",
                       "st", wire("st", routers::straight())),
                  cell("orthogonal(Bend::MidX)",
                       "the Z · half way over, one vertical run, "
                       "half way in — what a node graph "
                       "defaults to",
                       "mx",
                       wire("mx", routers::orthogonal(routers::Bend::MidX))),
                  cell("orthogonal(Bend::HFirst, 12)",
                       "an L bending AT THE TARGET column, its turn "
                       "rounded · the circuit trace",
                       "hf",
                       wire("hf", routers::orthogonal(routers::Bend::HFirst,
                                                      kRadius))),
                  cell("orthogonal(Bend::VFirst, 0, 14)",
                       "the other L, out of the SOURCE first, its turn "
                       "cut at 45° · a chamfer wins over a "
                       "radius",
                       "vf",
                       wire("vf", routers::orthogonal(routers::Bend::VFirst, 0,
                                                      kChamfer))),
                  cell("routers::arc(0.26)",
                       "the chord bowed by a fraction of its own length "
                       "· the sign picks the side",
                       "ar", wire("ar", routers::arc(kBulge))),
                  cell("rail({a, b}, octilinear(8))",
                       "the metro-map RailRouter · the leg runs "
                       "45° for the shorter delta and finishes "
                       "straight, and rail() is its only door",
                       "oc",
                       rail({Anchor{"oc-a", {0.5f, 0.5f}, 4},
                             Anchor{"oc-b", {0.5f, 0.5f}, 4}},
                            routers::octilinear(8)))},
             .gap = 10})));
  }
};

SIGIL_SKETCH(RoutersStraight, "Kit · API",
             "the same two anchors routed straight, as both orthogonal Ls "
             "and the Z between them, bowed as an arc, and threaded "
             "octilinearly through a rail")
