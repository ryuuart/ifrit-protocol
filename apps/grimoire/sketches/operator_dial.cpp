/** @file
 * operator_dial — one ring of children stating their hour, and the
 * operators that read the fact.
 *
 * Every child states one fact about itself, `attribute("hour", n)`, and
 * nothing about where that puts it. The sheet keeps the children and
 * changes only the operator list, in two halves.
 *
 * PLACED BY THE FACT. `layouts::Radial{.lane = "hour", .divisions = 12}`
 * stands each child at its own hour, so the children can be written in
 * any order and hours can be missing, where the same ring told nothing of
 * the lane stands them at their index and the dial comes out wrong.
 * `.facing` turns each child along its radius, a paint-only turn the
 * layout never sees, and `layouts::Jitter` listed after the ring nudges
 * what the ring placed — the second operator reads where the first one
 * left each child.
 *
 * WHAT THE OPERATORS ADD. `stamp::ByLane` makes a numeral for every child
 * stating an hour; `pin::ByLane` hangs a callout off the children that
 * ask for one, and the callout that would leave the dial takes its
 * fallback and hangs inward; `outline::Around` draws a band along the
 * dial's own outline and `outline::Hull` encloses the children classed
 * `chosen`, behind everything. The band is the operator of the box AROUND
 * the dial, because a node applying operators of its own is one closed
 * node to the scope above it, and only that scope can find it.
 *
 * EDIT THESE FIRST
 *   kWritten — the hours written in the first half, in the order written.
 *   kChosen — the hours the hull encloses.
 *   kCallouts — the hours that ask for a callout.
 *   kJitter — how far the nudged dial's numerals stray, px.
 */

// TAGS: Kit/Layout

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilcompose/kit/Outline.h>
#include <sigilcompose/kit/Pin.h>
#include <sigilcompose/kit/Stamp.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilsketch/scene2d/Scene2D.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace shapes = sigil::geometry::shapes;
using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 1070};
constexpr float kCell = 328;
constexpr float kPicture = 300;
constexpr float kDial = 252;   // the dial's side, so its ring is round
constexpr float kJitter = 10;  // how far a nudged numeral strays, px
// Deliberately out of order, with 4, 7 and 10 missing: the lane places
// each where it belongs, the index places each where it was written.
constexpr std::array<int, 9> kWritten = {12, 3, 9, 6, 1, 11, 2, 8, 5};
constexpr std::array<int, 4> kChosen = {1, 2, 3, 4};  // the hull's hours
constexpr std::array<int, 2> kCallouts = {3, 9};      // the pinned hours

template <size_t Count>
bool listed(const std::array<int, Count>& hours, int hour) {
  return std::find(hours.begin(), hours.end(), hour) != hours.end();
}

/** A numeral that states its hour and nothing about where that is. */
Element numeral(int hour) {
  return text(std::to_string(hour))
      .key("numeral-" + std::to_string(hour))
      .attribute("hour", hour);
}

/** A point that states its hour, and what else it asks for: a class the
 *  hull reads, a callout the pin reads. What stands on it is stamped. */
Element point(int hour) {
  const sketch::kit::Theme& sheet =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  Element node = sigil::compose::point()
                     .key("hour-" + std::to_string(hour))
                     .attribute("hour", hour);
  if (listed(kChosen, hour)) node.styleClass("chosen");
  if (listed(kCallouts, hour))
    node.attribute(
        "callout",
        pin::Request{
            .element =
                box()
                    .borderRadius({4})
                    .fill(Fill::color(sheet.palette.cellGround))
                    .foreground(stroke(1.0f, Fill::color(sheet.palette.rule)))
                    .padding({2, 6})
                    .justifyContent(Justify::Center)
                    .children({text(std::to_string(hour) + " o'clock")}),
            .size = {84, 22},
            .where = {
                .on = {1, 0.5f},
                .at = {0, 0.5f},
                .offset = {14, 0},
                .fallbacks = {
                    {.on = {0, 0.5f}, .at = {1, 0.5f}, .offset = {-14, 0}}}}});
  return node;
}

std::vector<Element> writtenNumerals() {
  std::vector<Element> children;
  for (int hour : kWritten) children.push_back(numeral(hour));
  return children;
}

std::vector<Element> twelvePoints() {
  std::vector<Element> children;
  for (int hour = 1; hour <= 12; ++hour) children.push_back(point(hour));
  return children;
}

layouts::Radial byLane(bool facing = false) {
  return {.radiusFraction = 0.78f,
          .lane = "hour",
          .divisions = 12,
          .facing = facing};
}

Operator numerals() {
  return stamp::ByLane{
      .lane = "hour", .key = "numerals", .make = [](const Scope::Node& at) {
        return text(std::to_string(*at.attribute<int>("hour")))
            .centerAt({0, 0});
      }};
}

Operator band() {
  return outline::Around{
      .key = "dial",
      .across = 6,
      .formation = sigil::geometry::path::Formation::Inner,
      .fill =
          Fill::color(sketch::kit::featureTheme(sketch::kit::Density::Spacious)
                          .palette.rule)};
}

Operator chosenHull() {
  material::Color glow =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious).palette.figure;
  glow.a = 0.18f;
  return Operator(outline::Hull{.styleClass = "chosen",
                                .margin = 18,
                                .fill = Fill::color(glow)})
      .zIndex(-1);
}

/** THE DIAL every cell shows: a round face keyed for the band, the
 *  children under the operators given. `banded` hands the box around the
 *  dial the band, the one scope that can find the dial's outline. */
Element dial(std::vector<Element> children, std::vector<Operator> operators,
             bool banded = false) {
  Element around = box().width(kDial).height(kDial);
  if (banded) around.operators({band()});
  return sketch::kit::well(
      {.width = kCell,
       .height = kPicture,
       .content = sketch::kit::Well::Content{}},
      around.children(
          {box()
               .key("dial")
               .inset(0)
               .shape(shapes::circle())
               .foreground(sigil::compose::stroke(
                   1.0f, Fill::color(sketch::kit::featureTheme(
                                         sketch::kit::Density::Spacious)
                                         .palette.rule)))
               .operators(std::move(operators))
               .children(std::move(children))}));
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

}  // namespace

struct OperatorDial {
  void setup(sketch::Scene2DContext& ctx) {
    // nothing moves; the sheet is complete at once
    sketch::kit::stage(
        ctx,
        {.theme = sketch::kit::featureTheme(sketch::kit::Density::Spacious),
         .size = kCanvas,
         .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "Read off the children",
         .subtitle = "Every child states its hour. Only the operator list "
                     "changes from cell to cell.",
         .footer = "An arranging operator places during layout; an adding "
                   "one builds beside the children once layout has settled."},
        box().column().gap(26).children(
            {sketch::kit::section(
                 {.label = "01  PLACED BY THE FACT",
                  .note = "Nine numerals, written out of order, three hours "
                          "missing"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("BY INDEX", "Radial{}",
                                "Each numeral stands where it was written: "
                                "twelve first, then three, then nine.",
                                dial(writtenNumerals(),
                                     {layouts::Radial{.radiusFraction =
                                                          0.78f}})),
                           cell("BY THE LANE",
                                "Radial{.lane = \"hour\", .divisions = 12}",
                                "Each numeral stands at its own hour, and the "
                                "missing hours leave gaps.",
                                dial(writtenNumerals(), {byLane()})),
                           cell("FACING, THEN NUDGED",
                                "Radial{.lane, .facing = true}, Jitter{}",
                                "The ring turns each numeral along its "
                                "radius; the jitter after it moves what the "
                                "ring placed.",
                                dial(writtenNumerals(),
                                     {byLane(true),
                                      layouts::Jitter{.seed = 3,
                                                      .amount = kJitter}}))},
                      .measure = 1020,
                      .gap = 18})),
             sketch::kit::section(
                 {.label = "02  WHAT THE OPERATORS ADD",
                  .note = "Twelve points · stamped and pinned · banded and "
                          "hulled · all four"},
                 sketch::kit::comparison(
                     {.cases =
                          {cell("STAMPED AND PINNED",
                                "stamp::ByLane, pin::ByLane",
                                "A numeral on every point; the callouts hang "
                                "to the right, and three o'clock, which would "
                                "leave the dial, takes its fallback.",
                                dial(twelvePoints(),
                                     {byLane(), numerals(),
                                      pin::ByLane{.lane = "callout"}})),
                           cell("BANDED AND HULLED",
                                "Around{\"dial\"}, Hull{.styleClass}",
                                "A band inside the dial's own edge, applied "
                                "by the box around the dial; a hull, grown "
                                "18 px, round the four chosen hours.",
                                dial(twelvePoints(),
                                     {byLane(), numerals(), chosenHull()},
                                     true)),
                           cell("ALL FOUR", "two lists",
                                "Placed, stamped, pinned and hulled by the "
                                "dial's list; banded by the box around it.",
                                dial(twelvePoints(),
                                     {byLane(), numerals(), chosenHull(),
                                      pin::ByLane{.lane = "callout"}},
                                     true))},
                      .measure = 1020,
                      .gap = 18}))})));
  }
};

SIGIL_SKETCH(OperatorDial, "Kit · API",
             "one dial of children stating their hour: placed by index, by "
             "the hour lane, facing and nudged, then stamped with numerals, "
             "pinned with callouts that fall back inward, banded along the "
             "dial's edge and hulled by class")
