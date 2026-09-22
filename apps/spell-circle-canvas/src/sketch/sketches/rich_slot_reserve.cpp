/** @file
 * Inline objects and reserved bands change a paragraph before it breaks.
 * The slot is one unbreakable word; its baseline drop locates its bottom.
 * A tall slot enlarges the whole block's strut. Reserved bands add room
 * before or after every line, independently of the inline object.
 */
// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/paragraph/RichText.h>

#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr float kMeasure = 288;
constexpr material::Color kInk{0.85f, 0.87f, 0.90f, 1};
constexpr material::Color kChip{0.88f, 0.53f, 0.34f, 1};
constexpr material::Color kBand{0.15f, 0.20f, 0.25f, 1};

weave::Type voice() {
  return {.face = sketch::kit::houseFace(sketch::kit::Voice::Interface),
          .size = 14,
          .color = material::skia::toSkColor(kInk),
          .track = 0};
}

Element slotted(SkSize extent, float drop) {
  return text(weave::rich()
                  .add(u8"Place ")
                  .slot("chip", extent, drop)
                  .add(u8" into the line. It moves with the words and keeps "
                       u8"its full width when the paragraph wraps."))
      .font(voice())
      .width(kMeasure)
      .children({box().key("chip").fill(Fill::color(kChip))});
}

Element reserved(weave::ReservedBand band) {
  return text(
             "The same words occupy the same measure. Reserve room before a "
             "line for a reading, or after it for an annotation.")
      .font(voice())
      .width(kMeasure)
      .textLineMargin(band)
      .fill(Fill::color(kBand));
}

Element plate(Element paragraph, float height) {
  return sketch::kit::well({.width = 328, .height = height, .padding = 20})
      .children({std::move(paragraph)});
}
}  // namespace

struct RichSlotReserve {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 800}, .captureAt = 0.05});
    const auto depth = [&](weave::ReservedBand band) {
      return kit::formatted(
          "Measured block depth: %.1f px",
          ctx.measure(box().children({reserved(band)})).height());
    };
    ctx.composer.render(sketch::kit::page(
        {.title = "Making room in running text",
         .subtitle = "A 288 px measure at 14 px · the orange object moves "
                     "inside the line; the blue band belongs to every line",
         .footer = "Slots match this text leaf's keyed children. The "
                   "measurements below come from the laid-out paragraphs."},
        box().column().gap(26).children(
            {document::eyebrow("01 · AN OBJECT IN THE LINE"),
             sketch::kit::comparison(
                 {.cases = {{.title = "ON THE BASELINE",
                             .control = "34 × 16 px · drop 0",
                             .figure = plate(slotted({34, 16}, 0), 164),
                             .note =
                                 "The box's bottom sits on the text baseline."},
                            {.title = "LOWERED INTO THE LINE",
                             .control = "34 × 16 px · drop 4",
                             .figure = plate(slotted({34, 16}, 4), 164),
                             .note = "The same box extends four pixels below "
                                     "the baseline."},
                            {.title = "TALLER THAN THE TYPE",
                             .control = "40 × 32 px · drop 4",
                             .figure = plate(slotted({40, 32}, 4), 164),
                             .note = "Every line opens to the taller strut, "
                                     "including lines without the box."}},
                  .measure = 1020,
                  .gap = 18}),
             document::eyebrow("02 · ROOM BESIDE EVERY LINE"),
             sketch::kit::comparison(
                 {.cases = {{.title = "NO RESERVED BAND",
                             .control = "before 0 · after 0",
                             .figure = plate(reserved({}), 188),
                             .note = depth({})},
                            {.title = "ROOM ABOVE",
                             .control = "before 14 · after 0",
                             .figure = plate(reserved({.before = 14}), 188),
                             .note = depth({.before = 14})},
                            {.title = "ROOM BELOW",
                             .control = "before 0 · after 14",
                             .figure = plate(reserved({.after = 14}), 188),
                             .note = depth({.after = 14})}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(
    RichSlotReserve, "Kit · API",
    "an inline object at two baseline drops and a taller extent, beside "
    "measured paragraphs with room reserved above or below every line")
