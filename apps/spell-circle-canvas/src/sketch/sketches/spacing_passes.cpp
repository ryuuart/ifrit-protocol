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

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1100, 400};
constexpr float kCell = 200;
constexpr float kPicture = 200;

constexpr float kMeasure = 130;          // every cell is set in this measure
constexpr float kWordSpacing = 2.0f;     // the multiple a gap is AIMED at
constexpr float kLetterSpacing = 0.05f;  // the em fraction the second pass adds
constexpr float kGlyphScale = 0.92f;     // what the third pass scales across

constexpr SkColor4f kGround{0.07f, 0.07f, 0.085f, 1};
constexpr SkColor4f kCellGround{0.105f, 0.11f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};
constexpr SkColor4f kBody{0.86f, 0.87f, 0.90f, 1};

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

weave::TextStyle body() {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"Iowan Old Style", "Georgia", "Times New Roman", "serif"});
  return weave::textStyle({.face = face, .size = 12, .color = kBody});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(10.5f, kInk),
          .note = label(10, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kCell};
}

/** Long words in a narrow measure: a fit the word gaps alone cannot make
 *  without opening holes, which is what gives the later passes anything
 *  to do. */
const char* kPassage =
    "Justification spends interword gaps before letterspacing, and "
    "reaches for horizontal glyph-scaling last of all.";

Element passage(weave::JustificationOptions options) {
  return text(toU8(kPassage), body())
      .width(Dim(kMeasure))
      .textAlign(weave::TextAlignment::kJustify)
      .lineBreak(weave::LineBreakStrategy::kKnuthPlass)
      .justification(options);
}

Element cell(const char* call, const char* note, Element body) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   kit::well({.width = kCell,
                              .height = kPicture,
                              .ground = Fill::color(kCellGround),
                              .padding = 12})
                       .child(std::move(body)));
}

}  // namespace

struct SpacingPasses final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // nothing moves; the sheet is complete at once

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
    weave::JustificationOptions lastWord = gaps;
    lastWord.justifyLastLine = true;
    lastWord.singleWord = weave::JustificationOptions::SingleWord::kJustify;

    ctx.composer.render(
        kit::sheet(
            {.title = toU8("THE THREE PASSES \xc2\xb7 JustificationOptions "
                           "word gaps, letter spacing, glyph scale"),
             .subtitle = toU8("dials \xc2\xb7 one measure (130 px) for every "
                              "cell \xc2\xb7 the multiple a gap is aimed at "
                              "(2.0) \xc2\xb7 "
                              "the em fraction the letter pass adds "
                              "(0.05) \xc2\xb7 the glyph scale (0.92)"),
             .footer = toU8("each pass spends only what the one before it "
                            "could not, and a pass whose limits equal its "
                            "desired value contributes nothing and costs "
                            "nothing \xe2\x80\x94 which is why a caller who "
                            "sets none of them gets word spacing alone"),
             .titleStyle = label(14, kInk, 2.4f),
             .subtitleStyle = label(11.5f, kAsh, 0.8f),
             .footerStyle = label(11, kAsh, 0.4f),
             .marginX = 24,
             .marginTop = 20,
             .marginBottom = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells(
                {.cells =
                     {cell("justification({})",
                           "the word gaps alone \xc2\xb7 the two later "
                           "passes have limits equal to their desired "
                           "values and do not run, so nothing bounds the "
                           "gaps and they take the whole fit",
                           passage(gaps)),
                      cell("wordSpacing = 2.0",
                           "the FIRST pass aimed at twice the shaped space "
                           "\xc2\xb7 the elasticity is measured from this, "
                           "not from the space the face cut",
                           passage(wider)),
                      cell("letterSpacing = 0.05",
                           "the SECOND pass, in em fractions, applied to "
                           "every justified line whatever its fit \xc2\xb7 "
                           "a pass past the gaps is open, so the gaps hold "
                           "at their stretch limit and the letters carry "
                           "the rest",
                           passage(letters)),
                      cell("glyphScale = 0.92",
                           "the THIRD pass, which scales the letters "
                           "themselves across \xc2\xb7 the last thing a page "
                           "should do: every justified line is set at 92 "
                           "per cent of its shaped width",
                           passage(glyphs)),
                      cell("singleWord = kJustify",
                           "a line holding ONE word has no gaps to "
                           "spend \xc2\xb7 stretched across the measure by "
                           "letter spacing alone, with justifyLastLine "
                           "setting the closing line too",
                           passage(lastWord))},
                 .gap = 12}))
            .absolute()
            .inset(0));
  }
};

SIGIL_SKETCH(SpacingPasses, "Kit \xc2\xb7 API",
             "one justified passage in one measure with the three fitting "
             "passes opened one at a time, and a last line of one word "
             "stretched by letter spacing alone")
