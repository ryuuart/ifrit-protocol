/** @file
 * A text paint is mapped through the run's metrics. The chrome horizon
 * therefore follows cap height at every size. A compact proof shelf keeps
 * the six procedural fields and both ramps at one word, size and moment.
 */
// TAGS: Typography/Effects

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace material = sigil::material;
namespace paint = sigil::material::skia;
using namespace sigil::compose;

namespace {
constexpr float kMoment = 6.4f;

Element word(float size, paint::Paint fill) {
  return text("SIGIL")
      .font({.face = sigil::weave::ports::face(
                 {"Avenir Next Heavy", "Helvetica Neue Bold", "Arial Black"}),
             .size = size,
             .track = size * 0.025f})
      .textFill(std::move(fill));
}

paint::Paint field(material::Material value) {
  return paint::Paint::recipe(std::move(value));
}

// Sparkle uses pixel-sized cells. Map a virtual field into the unit square
// that textFill stretches over the run's metrics.
paint::Paint sparkle() {
  const auto shader =
      field(material::kit::sparkle(SkRect::MakeWH(220, 70), kMoment))
          .asShader();
  return paint::Paint::shader(
      shader->makeWithLocalMatrix(SkMatrix::Scale(1.0f / 220, 1.0f / 70)));
}

Element swatch(paint::Paint fill, bool overlay = false) {
  Element sample = box().width(217.5f).height(96);
  if (overlay)
    sample.children(
        {kit::centred(word(49, paint::Paint::solid({0.23f, 0.30f, 0.46f, 1})))
             .cover()});
  sample.children({kit::centred(word(49, std::move(fill))).cover()});
  return sketch::kit::well({.width = 241.5f,
                            .height = 120,
                            .padding = 12,
                            .content = sketch::kit::Well::Content{}})
      .children({std::move(sample)});
}
}  // namespace

struct TextPaints {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 940}, .captureAt = 0.05});
    const SkRect unit = SkRect::MakeWH(1, 1);
    Element hero =
        sketch::kit::well({.width = 501, .height = 236, .padding = 26})
            .column()
            .gap(20)
            .children(
                {document::eyebrow("ONE RAMP · 104 PX"),
                 word(104, kit::sunsetChromeType()),
                 document::caption("The hard horizon crosses the capitals.")});
    Element scale =
        sketch::kit::well({.width = 501, .height = 236, .padding = 26})
            .column()
            .gap(12)
            .children({document::eyebrow("THE SAME RAMP · 28 / 48 / 72 PX"),
                       word(28, kit::sunsetChromeType()),
                       word(48, kit::sunsetChromeType()),
                       word(72, kit::sunsetChromeType())});
    ctx.composer.render(sketch::kit::page(
        {.title = "Paint that follows the type",
         .subtitle = "The material's unit square runs from cap top to baseline "
                     "· change the size and the horizon follows",
         .footer =
             "All proofs use the same word and 49 px face. Procedural fields "
             "are held at 6.4 s; the two chrome ramps do not move."},
        box().column().gap(28).children(
            {sketch::kit::comparison(
                 {.cases = {{.title = "A WORDMARK", .figure = std::move(hero)},
                            {.title = "ONE COORDINATE SYSTEM, THREE SIZES",
                             .figure = std::move(scale)}},
                  .measure = 1020,
                  .gap = 18}),
             document::eyebrow("EIGHT INKS · a common proof size"),
             sketch::kit::comparison(
                 {.cases = {{.title = "WATER",
                             .control = "water(unit, t)",
                             .figure = swatch(
                                 field(material::kit::water(unit, kMoment))),
                             .note = "Fine highlights in a blue field."},
                            {.title = "MESH",
                             .control = "meshGradient(unit, t)",
                             .figure = swatch(field(
                                 material::kit::meshGradient(unit, kMoment))),
                             .note = "Four color regions across the word."},
                            {.title = "SPARKLE OVER A BASE",
                             .control = "220 × 70 px → unit space",
                             .figure = swatch(sparkle(), true),
                             .note =
                                 "A pixel-grid field mapped over blue ink."},
                            {.title = "STAR NEST",
                             .control = "starNest(unit, t)",
                             .figure = swatch(
                                 field(material::kit::starNest(unit, kMoment))),
                             .note = "Dense light inside the letterforms."}},
                  .measure = 1020,
                  .gap = 18}),
             sketch::kit::comparison(
                 {.cases = {{.title = "CLOUDS",
                             .control = "clouds(unit, t)",
                             .figure = swatch(
                                 field(material::kit::clouds(unit, kMoment))),
                             .note = "Broad, soft changes of value."},
                            {.title = "TUNNEL",
                             .control = "tunnel(unit, t)",
                             .figure = swatch(
                                 field(material::kit::tunnel(unit, kMoment))),
                             .note = "A high-contrast moving field."},
                            {.title = "SUNSET CHROME",
                             .control = "sunsetChromeType()",
                             .figure = swatch(kit::sunsetChromeType()),
                             .note = "A hard horizon at half cap height."},
                            {.title = "SILVER CHROME",
                             .control = "silverChromeType()",
                             .figure = swatch(kit::silverChromeType()),
                             .note = "The same mapping with a colder ramp."}},
                  .measure = 1020,
                  .gap = 18})})));
  }
};

SIGIL_SKETCH(TextPaints, "Specimen",
             "a chrome wordmark across four sizes and an aligned proof of six "
             "procedural text paints and two chrome ramps")
