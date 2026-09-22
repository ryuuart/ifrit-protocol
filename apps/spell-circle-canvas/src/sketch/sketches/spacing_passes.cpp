/** @file
 * spacing_passes — the three things a justified line may spend, in the
 * order it spends them.
 *
 * A justified line is fitted in THREE PASSES, each spending only what the
 * one before it could not. The WORD GAPS move first, from the width they
 * are aimed at towards the near limit; then LETTER SPACING is added
 * between the glyphs; then the glyphs themselves are SCALED across.
 * Shrinking runs the same order.
 *
 * A pass whose limits equal its desired value contributes nothing and
 * costs nothing, which is why a caller who sets none of them gets word
 * spacing alone — and why the passes are opened one at a time on this
 * sheet, over one passage in one measure, so each cell is the stock
 * settings plus exactly one field moved off them.
 *
 * `wordSpacing` is the width a gap is AIMED at, as a multiple of the
 * shaped space, and the elasticity is measured from it: a gap may run
 * from `wordSpacing · (1 − spaceShrink)` to `wordSpacing · (1 +
 * spaceStretch)`. The letter and glyph passes are in fractions of the em
 * and in scale respectively, and SCALING LETTERS IS THE LAST THING A PAGE
 * SHOULD DO — the defaults never do it.
 *
 * A line holding ONE WORD has no gaps to spend at all: `kAlign` leaves it
 * at the block's alignment and `kJustify` stretches it across the measure
 * with letter spacing alone.
 *
 * OPENING A PASS CHANGES THE PASS BEFORE IT. The word gaps run
 * unbounded while nothing follows them, which is the first cell; the
 * moment the letter or glyph limits leave room past what those passes
 * were asked for, the gaps stop at `wordSpacing · (1 + spaceStretch)`
 * and what they may not take is what the later passes spend. So the
 * third and fourth cells set the same words in tighter gaps than the
 * first, and the right margin of each is where that cell's limits ran
 * out.
 *
 * EDIT THESE FIRST
 *   kMeasure — the measure every cell is set in, px. A narrow one is what
 *     makes the later passes do anything at all.
 *   kWordSpacing — the multiple of the shaped space a gap is aimed at.
 *   kLetterSpacing — the em fraction the second pass adds.
 *   kGlyphScale — what the third pass scales the letters across by.
 */

// TAGS: Typography/Paragraph

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kCanvas = {1100, 490};
constexpr float kCell = 190;
constexpr float kPicture = 156;

constexpr float kMeasure = 130;          // every cell is set in this measure
constexpr float kWordSpacing = 2.0f;     // the multiple a gap is AIMED at
constexpr float kLetterSpacing = 0.05f;  // the em fraction the second pass adds
constexpr float kGlyphScale = 0.92f;     // what the third pass scales across

constexpr material::Color kBody{0.86f, 0.87f, 0.90f, 1};

weave::TextStyle body() {
  const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle(
      {.face = face, .size = 12, .color = material::skia::toSkColor(kBody)});
}

/** Long words in a narrow measure: a fit the word gaps alone cannot make
 *  without opening holes, which is what gives the later passes anything
 *  to do. */
const char* kPassage =
    "Justification spends interword gaps before letterspacing, and "
    "reaches for horizontal glyph-scaling last of all.";

Element passage(weave::JustificationOptions options) {
  return text(kPassage, body())
      .width(kMeasure)
      .block({.alignment = weave::TextAlignment::kJustify})
      .block({.lineBreak = weave::LineBreakStrategy::kKnuthPlass})
      .block({.justification = options});
}

Element singleWord(weave::JustificationOptions options) {
  const sketch::kit::Theme& sheet = sketch::kit::theme();
  const auto sample = [&](const char* label,
                          weave::JustificationOptions justification) {
    return box().column().gap(4).children(
        {text(label)
             .font(sheet.font({.size = 8.5f, .track = 0.5f, .mono = true}))
             .ink(sheet.palette.ash),
         text("Alone.", body())
             .width(kMeasure)
             .block({.alignment = weave::TextAlignment::kJustify,
                     .justification = justification})});
  };
  return box().column().gap(16).children(
      {sample("ALIGNED", {}), sample("JUSTIFIED", options)});
}

Element setting(Utf8 label, Utf8 control, Utf8 note, Element specimen) {
  const sketch::kit::Theme& sheet = sketch::kit::theme();
  return sketch::kit::caption(
      kCell, std::move(label), std::move(note),
      box().column().gap(10).children(
          {text(control)
               .font(sheet.font({.size = 10, .mono = true}))
               .ink(sheet.palette.figure),
           sketch::kit::well(
               {.width = kCell,
                .height = kPicture,
                .padding = 22,
                .content = sketch::kit::Well::Content{.across = Align::Center,
                                                      .down = Justify::Start}},
               std::move(specimen))}));
}

}  // namespace

struct SpacingPasses {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sketch::kit::studyTheme());
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});

    // Every value is the stock one with a single field moved, so a cell
    // reports what that field does and nothing else.
    weave::JustificationOptions gaps;
    weave::JustificationOptions wider = gaps;
    wider.wordSpacing = kWordSpacing;
    weave::JustificationOptions letters = gaps;
    letters.letterSpacing = kLetterSpacing;
    letters.letterSpacingMaximum = kLetterSpacing * 2;
    weave::JustificationOptions glyphs = gaps;
    glyphs.glyphScale = kGlyphScale;
    glyphs.glyphScaleMinimum = kGlyphScale;
    glyphs.glyphScaleMaximum = kGlyphScale;
    weave::JustificationOptions lastWord = gaps;
    lastWord.justifyLastLine = true;
    lastWord.singleWord = weave::JustificationOptions::SingleWord::kJustify;

    ctx.composer.render(sketch::kit::page(
        {.title = "Fitting a justified line",
         .subtitle = kit::formatted("One %.0f px measure · four paragraph "
                                    "settings and a single-word edge case",
                                    kMeasure),
         .footer = "Fitting order: word gaps → letter spacing → glyph scale. "
                   "A one-word line can spend only letter spacing."},
        kit::cells(
            {.cells =
                 {setting("WORD GAPS", "justification({})",
                          "Stock word gaps take the whole fit. "
                          "The later passes are closed.",
                          passage(gaps)),
                  setting("WIDER GAPS",
                          kit::formatted("wordSpacing = %.1f", kWordSpacing),
                          "Aim at twice the shaped space. Stretch and shrink "
                          "are measured from that target.",
                          passage(wider)),
                  setting(
                      "LETTER SPACING",
                      kit::formatted("letterSpacing = %.2f", kLetterSpacing),
                      "Open the second pass. Word gaps stop at their limit; "
                      "space between letters carries the rest.",
                      passage(letters)),
                  setting("GLYPH SCALE",
                          kit::formatted("glyphScale = %.2f", kGlyphScale),
                          "The final pass changes the width of the glyphs "
                          "themselves, here to 92 per cent.",
                          passage(glyphs)),
                  setting("ONE-WORD LINE", "singleWord = kJustify",
                          "A lone word, before and after. Letter spacing "
                          "fills the measure when justifyLastLine is on.",
                          singleWord(lastWord))},
             .gap = 16})));
  }
};

SIGIL_SKETCH(SpacingPasses, "Kit · API",
             "one justified passage in one measure with the three fitting "
             "passes opened one at a time, and a last line of one word "
             "stretched by letter spacing alone")
