/** @file
 * attribute_ring — a dial placed by what its numerals SAY, not by the
 * order they were written in.
 *
 * Every numeral states one fact about itself, `attribute("hour", n)`, and
 * says nothing about where that puts it. The operator on the ring reads
 * the fact: `layouts::Radial{.lane = "hour", .divisions = 12}` stands each
 * child at its own hour, so the children can be written in any order and
 * hours can be missing, where the same ring told nothing of the lane
 * stands them at their index and the dial comes out wrong. `.facing`
 * turns each numeral along its radius, a paint-only turn the layout never
 * sees, and `layouts::Jitter` listed after the ring nudges what the ring
 * placed — the second operator reads where the first one left each child.
 *
 * EDIT THESE FIRST
 *   kHours — the hours written, in the order they are written.
 *   kJitter — how far the nudged dial's numerals stray, px.
 */

// TAGS: Kit/Layout

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Layouts.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

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
constexpr float kJitter = 10;  // how far a nudged numeral strays, px
// Deliberately out of order, with 4, 7 and 10 missing: the lane places
// each where it belongs, the index places each where it was written.
constexpr std::array<int, 9> kHours = {12, 3, 9, 6, 1, 11, 2, 8, 5};

/** One numeral, stating its hour and nothing about where that is. */
Element numeral(int hour) {
  return text(std::to_string(hour))
      .key("hour-" + std::to_string(hour))
      .attribute("hour", hour);
}

/** The dial: a ring of numerals under the operators given. */
Element dial(std::vector<Operator> operators) {
  std::vector<Element> numerals;
  numerals.reserve(kHours.size());
  for (int hour : kHours) numerals.push_back(numeral(hour));
  return sketch::kit::well({.width = kCell, .height = kPicture})
      .children({box()
                     .inset(0)
                     .shape(shapes::circle())
                     .foreground(sigil::compose::stroke(
                         1.0f,
                         Fill::color(sketch::kit::theme().palette.rule)))
                     .operators(std::move(operators))
                     .children(numerals)});
}

sketch::kit::ComparisonCase cell(const char* title, const char* call,
                                 const char* note, Element figure) {
  return {.title = title,
          .control = call,
          .figure = std::move(figure),
          .note = note};
}

}  // namespace

struct AttributeRing {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide presentation(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "Placed by a fact",
         .subtitle = "The numerals are written out of order, with three "
                     "hours missing. Only the operator changes.",
         .footer = "A child states attribute(\"hour\", n). The ring told the "
                   "lane reads it; the ring told nothing places by index."},
        box().column().gap(22).children(
            {sketch::kit::sectionHeader(
                 {.label = "01  THE SAME CHILDREN, THREE OPERATOR LISTS",
                  .note = "By index · by the hour lane · by the lane, "
                          "facing and nudged"}),
             sketch::kit::comparison(
                 {.cases =
                      {cell("BY INDEX", "Radial{}",
                            "Each numeral stands where it was written: "
                            "twelve first, then three, then nine.",
                            dial({layouts::Radial{.radiusFraction = 0.78f}})),
                       cell("BY THE LANE", "Radial{.lane = \"hour\", .divisions = 12}",
                            "Each numeral stands at its own hour, and the "
                            "missing hours leave gaps.",
                            dial({layouts::Radial{.radiusFraction = 0.78f,
                                                  .lane = "hour",
                                                  .divisions = 12}})),
                       cell("FACING, THEN NUDGED",
                            "Radial{.lane, .facing = true}, Jitter{}",
                            "The ring turns each numeral along its radius; "
                            "the jitter after it moves what the ring placed.",
                            dial({layouts::Radial{.radiusFraction = 0.78f,
                                                  .lane = "hour",
                                                  .divisions = 12,
                                                  .facing = true},
                                  layouts::Jitter{.seed = 3,
                                                  .amount = kJitter}}))},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(AttributeRing, "Kit · API",
             "nine numerals written out of order, placed by index, by the "
             "hour each one states, and by that lane facing outward with "
             "a jitter nudging what the ring placed")
