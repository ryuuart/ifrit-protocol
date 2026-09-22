/** @file
 * pins_and_hulls — the stock adding operators beside the wires: what is
 * hung off a node, what is drawn along its edge, what encloses a set.
 *
 * Every panel is the same skeleton, a ring of points each stating its
 * hour and a few stating more, and only the operator list changes. The
 * first stamps a numeral on every point and pins a callout to the ones
 * that ask for one — the callout nearest the edge takes its fallback and
 * hangs inward. The second draws a band along the dial's own outline and
 * a hull around the points classed `chosen`, behind everything. The
 * third is the whole dial: stamped, pinned, banded and hulled by one
 * list, the picture the model was written for.
 *
 * EDIT THESE FIRST
 *   kChosen — the hours the hull encloses.
 *   kCallouts — the hours that ask for a callout.
 */

// TAGS: Kit/Layout

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Outline.h>
#include <sigilcompose/kit/Pin.h>
#include <sigilcompose/kit/Stamp.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 620};
constexpr float kCell = 328;
constexpr float kPicture = 300;
constexpr std::array<int, 4> kChosen = {1, 2, 3, 4};    // the hull's hours
constexpr std::array<int, 2> kCallouts = {3, 9};        // the pinned hours

/** One hour: a point stating its hour, and what else it asks for. */
Element hour(int number) {
  Element point = compose::point().key("h" + std::to_string(number))
                      .attribute("hour", number);
  if (std::find(kChosen.begin(), kChosen.end(), number) != kChosen.end())
    point.styleClass("chosen");
  if (std::find(kCallouts.begin(), kCallouts.end(), number) != kCallouts.end())
    point.attribute(
        "callout",
        pin::Request{
            .element = box()
                           .borderRadius({4})
                           .fill(Fill::color(sketch::kit::theme().palette.cellGround))
                           .foreground(stroke(
                               1.0f, Fill::color(sketch::kit::theme().palette.rule)))
                           .padding({2, 6})
                           .justifyContent(Justify::Center)
                           .children({text(std::to_string(number) + " o'clock")}),
            .size = {84, 22},
            .where = {.on = {1, 0.5f},
                      .at = {0, 0.5f},
                      .offset = {14, 0},
                      .fallbacks = {{.on = {0, 0.5f},
                                     .at = {1, 0.5f},
                                     .offset = {-14, 0}}}}});
  return point;
}

Operator numerals() {
  return stamp::ByLane{.lane = "hour",
                       .key = "numerals",
                       .make = [](const Scope::Node& at) {
                         return text(std::to_string(*at.attribute<int>("hour")))
                             .centerAt({0, 0});
                       }};
}

Operator ring() {
  return layouts::Radial{.radiusFraction = 0.72f, .lane = "hour", .divisions = 12};
}

Operator dialBand() {
  return outline::Around{.key = "dial",
                         .across = across(6),
                         .formation = geometry::path::Formation::Inner,
                         .fill = Fill::color(sketch::kit::theme().palette.rule)};
}

Operator chosenHull() {
  SkColor4f glow = sketch::kit::theme().palette.figure;
  glow.fA = 0.18f;
  return Operator(outline::Hull{.styleClass = "chosen",
                                .margin = 18,
                                .fill = Fill::color(glow)})
      .zIndex(-1);
}

/** The dial: a circle keyed for the band, the hours under the operators. */
Element dial(std::vector<Operator> operators) {
  std::vector<Element> hours;
  for (int number = 1; number <= 12; ++number) hours.push_back(hour(number));
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children({box().inset(0).children(
          {box()
               .key("dial")
               .inset(24)
               .shape(shapes::circle())
               .operators(std::move(operators))
               .children(hours)})});
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

}  // namespace

struct PinsAndHulls {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "Built on the skeleton",
         .subtitle = "Twelve points stating their hour. What stands on them "
                     "is what the operators add.",
         .footer = "stamp::ByLane makes one element per node; pin::ByLane "
                   "hangs a request where it fits; outline::Around bands one "
                   "edge; outline::Hull encloses a class."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  THE SAME POINTS, THREE OPERATOR LISTS",
                  .note = "Stamped and pinned · banded and hulled · all four"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("STAMPED AND PINNED",
                            "Radial{lane}, stamp::ByLane, pin::ByLane",
                            "A numeral on every point; the callouts hang "
                            "outward, and nine o'clock takes its fallback.",
                            dial({ring(), numerals(),
                                  pin::ByLane{.lane = "callout"}})),
                       cell("BANDED AND HULLED",
                            "Around{\"dial\"}, Hull{.styleClass = \"chosen\"}",
                            "A band inside the dial's own edge; a hull, grown "
                            "18 px, round the four chosen hours.",
                            dial({ring(), numerals(), dialBand(), chosenHull()})),
                       cell("ALL FOUR", "one list",
                            "Placed, stamped, pinned, banded and hulled: five "
                            "operators over twelve points.",
                            dial({ring(), numerals(), dialBand(), chosenHull(),
                                  pin::ByLane{.lane = "callout"}}))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(PinsAndHulls, "Kit · API",
             "twelve points stating their hour, stamped with numerals, "
             "pinned with callouts that fall back inward, banded along the "
             "dial's edge and hulled by class")
