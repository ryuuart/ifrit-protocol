/** @file
 * PAINT THAT FOLLOWS THE TYPE. An ink's unit square is stretched over the
 * passage it paints: across the widest line, from the first line's cap top
 * to the last line's baseline. So a chrome horizon crosses a wordmark's
 * capitals at every size, the same ramp spreads thin down a paragraph, and
 * a long run cropped to its top shows only the top slice of its field.
 * Eight inks are then proved on a display word and on body type at once:
 * each is one class of the sheet, stated once on its well and inherited by
 * both passages, each stretched over its own box.
 */
// TAGS: Typography/Effects, Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace paint = sigil::material::skia;
using namespace sigil::compose;
using namespace sigil::weave::literals;

namespace {

/** The moment every procedural field is held at. */
constexpr float kMoment = 6.4f;
/** The height of the window the long run is cropped to. */
constexpr float kCrop = 170;

/** One ink of the proof: the class the sheet paints it under, and the
 *  words its cell is captioned with. */
struct Ink {
  std::string name;
  std::string title;
  std::string control;
  std::string note;
  paint::Paint paint;
  /** The box the paint is stretched over; the passage's own text box
   *  unless the ink says otherwise. */
  PaintBox box = PaintBox::Element;
};

paint::Paint field(material::Material recipe) {
  return paint::Paint::recipe(std::move(recipe));
}

std::vector<Ink> inks() {
  // An ink on the default box hands its paint the unit square, so the
  // fields are authored over it rather than over any run's pixels.
  const SkRect unit = SkRect::MakeWH(1, 1);
  namespace fields = material::kit;
  return {
      {"water", "WATER", "water(unit, t)",
       "Fine highlights in a blue field; at body size they read as grain.",
       field(fields::water(unit, kMoment))},
      {"mesh", "MESH", "meshGradient(unit, t)",
       "Four colour regions, crossed by the word and by the column alike.",
       field(fields::meshGradient(unit, kMoment))},
      // `material::kit::sparkle` sizes its cells in the pixels it is
      // sampled in and never reads the run's extent, so on the unit
      // square of the default box a whole passage falls inside one cell.
      // The rule states the ink on the passage itself, so the Subtree box
      // is the passage's own box, resolved in its pixels, where a cell is
      // the 22 px it was drawn at; the bounds place only the origin.
      {"sparkle", "SPARKLE OVER A BASE", "sparkle(px, t) · plus · Subtree",
       "Stated over the passage's pixels, where its cells keep their size.",
       paint::Paint::blend(
           {{paint::Paint::solid({0.23f, 0.30f, 0.46f, 1}),
             SkBlendMode::kSrcOver},
            {field(fields::sparkle(SkRect::MakeWH(220, 70), kMoment)),
             SkBlendMode::kPlus}}),
       PaintBox::Subtree},
      {"star-nest", "STAR NEST", "starNest(unit, t)",
       "Dense light inside the letterforms; small type keeps its warmth.",
       field(fields::starNest(unit, kMoment))},
      {"clouds", "CLOUDS", "clouds(unit, t)",
       "Broad, soft changes of value that hold an even grey.",
       field(fields::clouds(unit, kMoment))},
      {"tunnel", "TUNNEL", "tunnel(unit, t)",
       "A high-contrast field; its dark rings eat whole words.",
       field(fields::tunnel(unit, kMoment))},
      {"sunset", "SUNSET CHROME", "sunsetChromeType()",
       "A hard horizon at half cap height; a column takes one band of it.",
       kit::sunsetChromeType()},
      {"silver", "SILVER CHROME", "silverChromeType()",
       "The same mapping in a colder ramp.", kit::silverChromeType()},
  };
}

/** Liang's English patterns, loaded once and shared by every paragraph. */
std::shared_ptr<const weave::Hyphenator> hyphenator() {
  static const std::shared_ptr<const weave::Hyphenator> table =
      std::make_shared<const weave::kit::PatternHyphenator>(
          "en", weave::kit::englishHyphenationPatterns());
  return table;
}

/** HOW EVERYTHING HERE IS SET: the wordmark in a heavy grotesque, the
 *  page in a book face justified with hyphens, and each ink a class whose
 *  paint reaches the passages of the well that names it — never the
 *  eyebrows and captions standing among them. */
StyleSheet sheet(const std::vector<Ink>& all) {
  StyleSheet inked;
  for (const Ink& ink : all)
    inked = inked + StyleSheet{rule("." + ink.name +
                                    " :is(wordmark, word, paragraph)")
                                   .ink(ink.paint, ink.box)};
  return StyleSheet{
             rule("wordmark")
                 .fontFamily(
                     "Avenir Next, Helvetica Neue, Arial Black, sans-serif")
                 .fontWeight(800)
                 .letterSpacing(0.025_em),
             rule("word, paragraph")
                 .fontFamily("Hoefler Text, Baskerville, serif")
                 .font({.language = "en-US"}),
             // The optimizing breaker fetches the line after a block's last
             // before the next block opens, so every block after the first
             // takes its first-line indent one line late.
             rule("paragraph")
                 .width(pct(100))
                 .lineHeight(weave::Leading::multiple(1.32f))
                 .textIndent(1.6_em)
                 .textAlign(weave::TextAlignment::kJustify)
                 .textWrap(TextWrap::Pretty)
                 .hyphens({.patterns = hyphenator()}),
         } +
         inked;
}

Element wordmark(float size) {
  return text(u8"SIGIL").role("wordmark").fontSize(size);
}

/** The words the page proofs and the ink cells are set in. */
struct Passages {
  std::u8string paragraph;
  std::u8string longRun;
  std::u8string proof;
};

/** One ink across three run lengths: a word, a complete paragraph, and
 *  the top of a run far longer than the window it is cropped to. */
Element pageProof(const std::string& ink, const Passages& words,
                  float completeDepth) {
  return sketch::kit::well({.width = 501, .height = 440, .padding = 24})
      .styleClass(ink)
      .column()
      .gap(14)
      .children(
          {document::eyebrow("01 · A WORD / 52 PX"),
           text(u8"PAGE").role("word").fontSize(52),
           document::eyebrow("02 · A COMPLETE PARAGRAPH / 13 PX"),
           document::paragraph(words.paragraph).fontSize(13),
           document::eyebrow("03 · THE TOP OF A LONG RUN / 9 PX"),
           box().height(kCrop).overflow(Overflow::Clip).children(
               {document::paragraph(words.longRun).fontSize(9)}),
           document::caption(kit::formatted(
               "%.0f px window · %.0f px complete run", kCrop,
               completeDepth))});
}

/** One ink at a common proof size: the display word over body type. */
Element inkProof(const Ink& ink, const Passages& words) {
  return sketch::kit::well({.width = 241.5f, .height = 172, .padding = 16})
      .styleClass(ink.name)
      .column()
      .gap(12)
      .alignItems(Align::Center)
      .children({wordmark(49), document::paragraph(words.proof).fontSize(9)});
}

sketch::kit::ComparisonCase inkCase(const Ink& ink, const Passages& words) {
  return {.title = ink.title,
          .control = ink.control,
          .figure = inkProof(ink, words),
          .note = ink.note};
}

}  // namespace

struct TextPaints {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(
        sketch::kit::featureTheme(sketch::kit::Density::Spacious));
    sketch::kit::stage(ctx, {.size = {1100, 1620}, .captureAt = 0.05});
    const std::vector<Ink> all = inks();
    const StyleSheet setting = sheet(all);
    const Passages words{
        .paragraph = sketch::kit::passage(ctx, "data/paragraph.txt"),
        .longRun = sketch::kit::passage(ctx, "data/long_run.txt"),
        .proof = sketch::kit::passage(ctx, "data/proof.txt")};
    // The depth the whole long run sets to at the window's measure, which
    // is the box its ink is stretched over.
    const float completeDepth =
        ctx.measure(box().width(453).applyStyleSheet(setting).children(
                        {document::paragraph(words.longRun).fontSize(9)}))
            .height();
    const auto comparison = [](std::vector<sketch::kit::ComparisonCase> cases) {
      return sketch::kit::comparison(
          {.cases = std::move(cases), .measure = 1020, .gap = 18});
    };
    ctx.composer.render(sketch::kit::page(
        {.title = "Paint that follows the type",
         .subtitle = "An ink's unit square runs from the first cap top to the "
                     "last baseline · the horizon follows the size, and a "
                     "column stretches the field with it",
         .footer = "Procedural fields are held at 6.4 s; the two chrome "
                   "ramps do not move. Every ink is one class of the sheet, "
                   "stated once per well."},
        // Nothing on the sheet moves, so the whole of it is held as one
        // texture.
        box()
            .column()
            .gap(28)
            .applyStyleSheet(setting)
            .cache(Cache::Texture)
            .key("sheet")
            .children(
            {comparison(
                 {{.title = "A WORDMARK",
                   .figure =
                       sketch::kit::well(
                           {.width = 501, .height = 236, .padding = 26})
                           .styleClass("sunset")
                           .column()
                           .gap(20)
                           .children({document::eyebrow("ONE RAMP · 104 PX"),
                                      wordmark(104),
                                      document::caption("The hard horizon "
                                                        "crosses the "
                                                        "capitals.")})},
                  {.title = "ONE COORDINATE SYSTEM, THREE SIZES",
                   .figure = sketch::kit::well(
                                 {.width = 501, .height = 236, .padding = 26})
                                 .styleClass("sunset")
                                 .column()
                                 .gap(12)
                                 .children({document::eyebrow(
                                                "THE SAME RAMP · 28 / 48 / 72 "
                                                "PX"),
                                            wordmark(28), wordmark(48),
                                            wordmark(72)})}}),
             document::heading(2, "A WORD IS NOT A PAGE"),
             comparison(
                 {{.title = "A HARD HORIZON",
                   .control = "sunsetChromeType()",
                   .figure = pageProof("sunset", words, completeDepth),
                   .note = "The word shows the whole horizon. A paragraph "
                           "spreads it across its lines; the long run's top "
                           "slice never reaches it."},
                  {.title = "A BROAD COLOUR FIELD",
                   .control = "meshGradient(unit, t)",
                   .figure = pageProof("mesh", words, completeDepth),
                   .note = "The same four regions cover every run. A long "
                           "passage samples them far more slowly down the "
                           "page."}}),
             document::heading(2, "EIGHT INKS · A WORD AND BODY TYPE"),
             comparison({inkCase(all[0], words), inkCase(all[1], words),
                         inkCase(all[2], words), inkCase(all[3], words)}),
             comparison({inkCase(all[4], words), inkCase(all[5], words),
                         inkCase(all[6], words), inkCase(all[7], words)})})));
  }
};

SIGIL_SKETCH(TextPaints, "Specimen",
             "a chrome wordmark across four sizes, one ink across a word, a "
             "paragraph and a cropped long run, and eight inks proved on "
             "display and body type from one sheet")
