/** @file
 * Paragraph openings compared on one passage, then two ways to make room
 * at the margin. Initial size follows the face's cap height and line pitch;
 * nested styles stop at a word count or delimiter. The ornament instead
 * subtracts a silhouette, while list markers occupy a hanging indent.
 */
// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Typeset.h>
#include <sigilgeometry/kit/Silhouettes.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <optional>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr float kMeasure = 288;
constexpr float kBodySize = 14;
constexpr float kHang = 22;
constexpr material::Color kBody{0.85f, 0.86f, 0.89f, 1};
constexpr const char* kPassage =
    "When the first words set the tone, the reader finds a way into the page. "
    "An opening can carry a quiet initial, a change of voice, or a mark at "
    "the margin. The paragraph keeps its own rhythm.";

weave::Type serif(float size, material::Color color, float tracking = 0) {
  return {.face = weave::ports::face({"Iowan Old Style", "Georgia", "serif"}),
          .size = size,
          .color = material::skia::toSkColor(color),
          .track = tracking};
}

Element opening(std::optional<kit::NestedStyle> nested) {
  Text body =
      document::paragraph(kPassage)
          .font(serif(kBodySize, kBody))
          .width(kMeasure)
          .initialLetter(
              {.lines = 3,
               .margin = 8,
               .style = serif(kBodySize, sketch::kit::theme().palette.figure)});
  if (nested) body.spanStyle(kit::nestedRun(*nested), nested->style);
  return sketch::kit::well({.width = 328, .height = 218, .padding = 20})
      .children({std::move(body)});
}

Element ornament() {
  const auto& look = sketch::kit::theme();
  Element star =
      kit::at(box()
                  .key("opening-star")
                  .absolute()
                  .shape(sigil::geometry::shapes::star(8, 0.48f, 0.12f))
                  .fill(Fill::color(look.palette.figure))
                  .children({text("W")
                                 .font(serif(31, look.palette.ground))
                                 .absolute()
                                 .left(16)
                                 .top(16)}),
              0, 0, 68, 74);
  return sketch::kit::well({.width = 501, .height = 170, .padding = 20})
      .children({box().width(461).children(
          {std::move(star),
           document::paragraph(std::string_view(kPassage).substr(1))
               .font(serif(kBodySize, kBody))
               .width(461)
               .contentFlowAround("opening-star", 8)})});
}

Element hangingList() {
  const auto& look = sketch::kit::theme();
  const std::vector<std::u8string> items = {
      u8"The marker holds its own margin.",
      u8"Wrapped lines return to the text edge, leaving the marker clear while "
      u8"the paragraph carries on below it."};
  const std::vector<std::u8string> marks = {u8"1.", u8"2."};
  const std::vector<std::u8string> nested = {
      u8"An inner level adds one more indent."};
  const std::vector<std::u8string> dashes = {u8"—"};
  Element guide = box().absolute().left(kHang).top(0).width(1).height(130).fill(
      Fill::color(look.palette.rule));
  return sketch::kit::well({.width = 501, .height = 170, .padding = 20})
      .children({box().width(461).children(
          {std::move(guide),
           box().column().gap(12).children(
               {kit::bullets(items, marks, serif(kBodySize, kBody), kHang, 439),
                kit::bullets(nested, dashes, serif(kBodySize, look.palette.ash),
                             kHang, 417)
                    .margin(0, 0, 0, kHang)})})});
}
}  // namespace

struct BulletsDropCap {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 790}, .captureAt = 0.05});
    const weave::Type openingVoice =
        serif(kBodySize, sketch::kit::theme().palette.figure, 0.35f);
    ctx.composer.render(sketch::kit::page(
        {.title = "A way into the paragraph",
         .subtitle = "One passage at 14 px · three opening treatments, then "
                     "two different kinds of margin",
         .footer =
             "The initial spans three lines. A nested style follows the words "
             "when copy changes; the list's indent belongs to every line."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases =
                      {{.title = "INITIAL ONLY",
                        .control = "3 lines · 8 px stand-off",
                        .figure = opening({}),
                        .note = "Cap height and line pitch determine the "
                                "letter's size."},
                       {.title = "THE FIRST FIVE WORDS",
                        .control = "NestedStyle::Until::Words",
                        .figure = opening(kit::NestedStyle{
                            .until = kit::NestedStyle::Until::Words,
                            .count = 5,
                            .style = openingVoice}),
                        .note = "The changed voice ends after “set”; the "
                                "paragraph continues."},
                       {.title = "THROUGH THE COMMA",
                        .control = "NestedStyle::Until::Delimiter",
                        .figure =
                            opening(kit::NestedStyle{
                                .until = kit::NestedStyle::Until::Delimiter,
                                .delimiter = u8",",
                                .style = openingVoice}),
                        .note =
                            "The opening phrase includes its punctuation."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::comparison(
                 {.cases = {{.title = "A SHAPED OPENING",
                             .control = "One silhouette · flowAround",
                             .figure = ornament(),
                             .note = "The lines enter the star's notches; the "
                                     "body responds to its contour."},
                            {.title = "A HANGING MARKER",
                             .control = "22 px per level · kit::bullets",
                             .figure = hangingList(),
                             .note =
                                 "The rule marks the text edge. Numerals stay "
                                 "outside it; nested items move inward."}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(BulletsDropCap, "Kit · API",
             "one paragraph opened by a dropped initial and two nested styles, "
             "beside a shaped ornament and a hanging list")
