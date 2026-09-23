/** @file
 * One article in two voices. Semantic roles establish the hierarchy;
 * each reading column supplies its own stylesheet. Named classes and
 * an inline run keep their deliberate emphasis within either document.
 */
// TAGS: Typography/Documents, Typography/Styles, Runtime/Composition

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilcompose/kit/Document.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
using namespace sigil::compose;

namespace {
constexpr float kColumn = 501;
constexpr float kInset = 28;
constexpr float kMeasure = kColumn - kInset * 2;
constexpr material::Color kWarm = hexColor(0x9e4c31);
constexpr material::Color kCool = hexColor(0x246579);
constexpr material::Color kInline = hexColor(0x8c4561);

Element article() {
  const weave::RichText passage =
      weave::rich()
          .add(u8"A paragraph is one shaped passage. ")
          .add(u8"This phrase keeps its own ink",
               weave::Type{.color = material::skia::toSkColor(kInline)})
          .add(
              u8", while the surrounding words follow the document. "
              u8"Changing the sheet changes its voice without rebuilding "
              u8"the content.");
  return document::article(
             {
                 document::eyebrow("FIELD NOTES / 01"),
                 document::h1("The shape of a page"),
                 document::lead("A few familiar elements give a thought its "
                                "structure, before a design gives it a voice."),
                 document::rule().opacity(0.25f),
                 document::paragraph(passage),
                 document::h2("Room for a thought"),
                 document::quote({
                     document::paragraph(
                         "A useful default should leave the "
                         "author room to make a different page."),
                     document::caption(
                         "A margin note, carried with its passage."),
                 }),
                 document::list({
                     document::item("Headings name the structure."),
                     document::item("Paragraphs share the document's voice."),
                     document::item("Lists keep each marker with its text."),
                 }),
                 document::caption("This note wears the author class “accent”.")
                     .styleClass("accent"),
                 document::footer("One content tree. Two independent sheets."),
             })
      .var(document::measure, Dimension(kMeasure))
      .var(document::gap, Dimension(12))
      .var(document::listGap, Dimension(7));
}

sigil::compose::StyleSheet voice(bool editorial) {
  const auto body =
      editorial ? weave::ports::face({"Iowan Old Style", "Georgia", "serif"})
                : weave::ports::face({"Helvetica Neue", "Arial", "sans-serif"});
  const auto mono = weave::ports::face({"Menlo", "Consolas", "monospace"});
  const material::Color ink =
      editorial ? hexColor(0x352f29) : hexColor(0x22333c);
  const material::Color muted =
      editorial ? hexColor(0x776858) : hexColor(0x647984);
  const material::Color accent = editorial ? kWarm : kCool;
  sigil::compose::StyleSheet sheet{
      sigil::compose::rule("article")
          .font({.face = body,
                 .size = 17,
                 .color = material::skia::toSkColor(ink),
                 .track = 0})
          .block({.leading = weave::Leading::multiple(1.4f)}),
      sigil::compose::rule("h1").font(
          {.face = body,
           .size = editorial ? 32.0f : 30.0f,
           .color = material::skia::toSkColor(accent),
           .track = 0}),
      sigil::compose::rule("h2").font({.face = body,
                                       .size = 22,
                                       .color = material::skia::toSkColor(ink),
                                       .track = 0}),
      sigil::compose::rule("lead").font(
          {.face = body,
           .size = 19,
           .color = material::skia::toSkColor(muted),
           .track = 0}),
      sigil::compose::rule("eyebrow").font(
          {.face = mono,
           .size = 10,
           .color = material::skia::toSkColor(accent),
           .track = 1.1f}),
      sigil::compose::rule("caption, .caption")
          .font({.face = body,
                 .size = 13,
                 .color = material::skia::toSkColor(muted),
                 .track = 0}),
      sigil::compose::rule("footer").font(
          {.face = mono,
           .size = 10,
           .color = material::skia::toSkColor(muted),
           .track = 0}),
      sigil::compose::rule("quote, .quote")
          .font({.color = material::skia::toSkColor(accent)}),
      sigil::compose::rule(".accent").font(
          {.color = material::skia::toSkColor(accent)})};
  return sheet;
}

Element panel(bool editorial) {
  return sketch::kit::well(
             {.width = kColumn,
              .height = 768,
              .ground = Fill::color(editorial ? hexColor(0xf3ebdd)
                                              : hexColor(0xe8eff0)),
              .padding = kInset,
              .clip = false},
             box().children({article()}))
      .applyStyleSheet(voice(editorial));
}

struct DocumentStyles {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = {1100, 1010}, .captureAt = 0.05});
    ctx.composer.render(sketch::kit::page(
        {.title = "One document, two voices",
         .subtitle = "The same article uses headings, paragraphs, a quotation "
                     "and a list. Each column supplies a different role sheet.",
         .footer = "Role defaults → document stylesheet → author class → "
                   "explicit font or inline style."},
        sketch::kit::comparison({
            .cases = {{.title = "01 / EDITORIAL",
                       .control = "Book face · warm paper",
                       .figure = panel(true)},
                      {.title = "02 / TECHNICAL",
                       .control = "Interface face · cool paper",
                       .figure = panel(false)}},
            .measure = 1020,
            .gap = 18,
        })));
  }
};
}  // namespace

SIGIL_SKETCH(DocumentStyles, "Compose · Typography",
             "one article in two semantic stylesheets, with class and inline "
             "overrides")
