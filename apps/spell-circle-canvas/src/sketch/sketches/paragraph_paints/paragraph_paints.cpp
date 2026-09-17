/** @file
 * The same ink mapped over a word, a short paragraph and a long run.
 * A clipped viewport does not reset the mapping: a crop of a long column
 * receives only the corresponding slice of the material's unit square.
 */
// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace paint = sigil::material::skia;
using namespace sigil::compose;

namespace {
constexpr float kMoment = 6.4f;
constexpr const char8_t* kExcerpt =
    u8"A word is read by its outline; a page by its overall texture. "
    u8"The same ramp that crosses four capitals now crosses an entire "
    u8"column. Every line receives a small part of the field, and a crop "
    u8"shows only a fraction of its full extent.";
constexpr const char8_t* kProof =
    u8"Small type makes a field visible as texture. Eight inks share "
    u8"the same words, face, measure and size.";

const weave::kit::PatternHyphenator& hyphenator() {
  static const weave::kit::PatternHyphenator value(
      "en", weave::kit::englishHyphenationPatterns());
  return value;
}

Element passage(std::u8string_view words, float size, float width,
                paint::Paint fill) {
  weave::ParagraphStyle paragraph;
  paragraph.leading = weave::Leading::multiple(1.32f);
  paragraph.indent.firstLine = size * 1.6f;
  return text(words)
      .font({.face = sketch::kit::houseFace(sketch::kit::Voice::Book),
             .size = size,
             .track = 0,
             .language = "en-US"})
      .width(width)
      .paragraphs({paragraph})
      .block(
          {.alignment = weave::TextAlignment::kJustify,
           .hyphenation = weave::HyphenationOptions{.patterns = &hyphenator()},
           .lineBreak = weave::LineBreakStrategy::kKnuthPlass})
      .textFill(std::move(fill));
}

paint::Paint field(material::Material value) {
  return paint::Paint::recipe(std::move(value));
}

// Sparkle uses pixel-sized cells. Map a virtual field into the unit square
// that textFill stretches over the run's metrics.
paint::Paint sparkle() {
  const auto shader =
      field(material::kit::sparkle(SkRect::MakeWH(96, 256), kMoment))
          .asShader();
  return paint::Paint::shader(
      shader->makeWithLocalMatrix(SkMatrix::Scale(1.0f / 96, 1.0f / 256)));
}
}  // namespace

struct ParagraphPaints {
  std::u8string prose;

  Element proof(paint::Paint ink, float fullDepth) {
    return sketch::kit::well({.width = 501, .height = 484, .padding = 24})
        .column()
        .gap(14)
        .children(
            {text("01 · A WORD / 52 PX").styleClass("eyebrow"),
             text("PAGE")
                 .font(
                     {.face = sketch::kit::houseFace(sketch::kit::Voice::Book),
                      .size = 52,
                      .track = 0})
                 .textFill(ink),
             text("02 · A COMPLETE PARAGRAPH / 13 PX").styleClass("eyebrow"),
             passage(kExcerpt, 13, 453, ink),
             text("03 · THE TOP OF A LONG RUN / 9 PX").styleClass("eyebrow"),
             box().width(453).height(170).clip().children(
                 {passage(prose, 9, 453, ink)}),
             text(kit::formatted("170 px viewport · %.0f px complete run",
                                 fullDepth))
                 .styleClass("captionNote")});
  }

  Element inventory(paint::Paint ink, bool overlay = false) {
    Element well =
        sketch::kit::well({.width = 111.75f, .height = 116, .padding = 10});
    if (overlay)
      well.children({box().absolute().left(10).top(10).children({passage(
          kProof, 9, 91.75f, paint::Paint::solid({0.48f, 0.54f, 0.66f, 1}))})});
    return well.children({passage(kProof, 9, 91.75f, std::move(ink))});
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 960}, .captureAt = 0.05});
    prose = sketch::kit::passage(ctx, "data/paragraph_paints.txt");
    const float fullDepth =
        ctx.measure(box().children({passage(
                        prose, 9, 453, paint::Paint::solid(SkColors::kWhite))}))
            .height();
    const SkRect unit = SkRect::MakeWH(1, 1);
    ctx.composer.render(sketch::kit::page(
        {.title = "A word is not a page",
         .subtitle = "Two paints, three run lengths · the ink spans the "
                     "complete text, even when only the top is visible",
         .footer = "Proofs: 9 px body type at 6.4 s. Sparkle maps a 96 × 256 "
                   "px field into the run; the other paints use normalized "
                   "coordinates."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "A HARD HORIZON",
                             .control = "sunsetChromeType()",
                             .figure =
                                 proof(kit::sunsetChromeType(), fullDepth),
                             .note = "The word shows the whole horizon. A "
                                     "paragraph spreads it across lines; the "
                                     "long crop can miss it entirely."},
                            {.title = "A BROAD COLOR FIELD",
                             .control = "meshGradient(unit, t)",
                             .figure = proof(field(material::kit::meshGradient(
                                                 unit, kMoment)),
                                             fullDepth),
                             .note = "The same four regions cover every run. A "
                                     "long passage samples them much more "
                                     "slowly down the page."}},
                  .measure = 1020,
                  .gap = 18}),
             text("BODY-TYPE PROOFS · the complete paint inventory")
                 .styleClass("eyebrow"),
             sketch::kit::comparison(
                 {.cases = {{.title = "WATER",
                             .figure = inventory(
                                 field(material::kit::water(unit, kMoment)))},
                            {.title = "MESH",
                             .figure = inventory(field(
                                 material::kit::meshGradient(unit, kMoment)))},
                            {.title = "SPARKLE",
                             .figure = inventory(sparkle(), true)},
                            {.title = "STAR NEST",
                             .figure = inventory(field(
                                 material::kit::starNest(unit, kMoment)))},
                            {.title = "CLOUDS",
                             .figure = inventory(
                                 field(material::kit::clouds(unit, kMoment)))},
                            {.title = "TUNNEL",
                             .figure = inventory(
                                 field(material::kit::tunnel(unit, kMoment)))},
                            {.title = "SUNSET",
                             .figure = inventory(kit::sunsetChromeType())},
                            {.title = "SILVER",
                             .figure = inventory(kit::silverChromeType())}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(ParagraphPaints, "Specimen",
             "the same paint across a word, a complete paragraph and a cropped "
             "long run, with all eight paints proved at body size")
