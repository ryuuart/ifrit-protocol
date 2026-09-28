/** @file
 * text_wrap — one passage set four ways by `textWrap`, and four ways by
 * `textJustify` under a justified alignment, each in the same measure.
 */

// THE TWO LONGHANDS OF HOW A PARAGRAPH'S LINES ARE SET. Every specimen is
// the same text leaf at the same measure; the only statement that differs
// between two columns is the one named above it.
//
//   · textWrap — Auto and Stable fill each line in turn, Pretty weighs
//     the whole paragraph at once, Balance weighs it and then sets it in
//     the narrowest measure that keeps its line count. The tint behind
//     each passage is the measure, so the rag is read against it.
//   · textJustify — the same passage justified: Auto spends the word
//     gaps, InterWord the word gaps alone, InterCharacter every letter
//     and gap alike up to a cap, None nothing at all.
//
// EDIT THESE FIRST
//   kMeasure  — the width every passage is set in; narrow it and the
//               four breakers part further.
//   kPassage  — the text every specimen sets.

// TAGS: Typography/Paragraph

#include <sigilweave/style/Face.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/LineSetting.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;
namespace weave = sigil::weave;

using namespace sigil::compose;

namespace {

constexpr SkSize kSceneSize{1040, 720};
constexpr float kMeasure = 200;

constexpr const char8_t* kPassage =
    u8"A line breaker decides where every line ends. Filled in turn, each "
    u8"line takes all it can hold; weighed together, the paragraph gives a "
    u8"little from one line to spare the next a ragged end.";

const material::Color kPaper{0.965f, 0.957f, 0.937f, 1};
const material::Color kInk{0.114f, 0.106f, 0.098f, 1};
const material::Color kMeasureTint{0.78f, 0.30f, 0.20f, 0.08f};

sigil::weave::Face serif() {
  return weave::ports::face(
      {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"});
}

/// The feature sheet leaves the passages at their authored measure.
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look =
      sketch::kit::featureTheme(sketch::kit::Density::Spacious);
  look.spacing.marginX = 48;
  look.spacing.marginTop = 44;
  look.spacing.marginBottom = 28;
  return look;
}

/// One specimen: the call that set it, over the passage in its measure.
Element specimen(const char* call, Text passage) {
  Element paper = sketch::kit::well(
      {.ground = Fill::color(kPaper), .padding = 10},
      box().children({std::move(passage)
                          .font({.face = serif(), .size = 14, .track = 0.2f})
                          .ink(kInk)
                          .width(kMeasure)
                          .fill(Fill::color(kMeasureTint))}));
  return box().column().gap(8).children(
      {document::code(call), std::move(paper)});
}

/// A row of specimens under the name of the longhand they vary.
Element row(const char* name, std::vector<Element> specimens) {
  return sketch::kit::section(
      {.label = name}, box().row().gap(20).children(std::move(specimens)));
}

}  // namespace

struct TextWrapSpecimen {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = kSceneSize, .captureAt = 0.2});
    ctx.composer.render(describe());
  }

  Element wrapRow() {
    return row(
        "TEXT-WRAP",
        {specimen("textWrap(Auto)", text(kPassage).textWrap(TextWrap::Auto)),
         specimen("textWrap(Stable)",
                  text(kPassage).textWrap(TextWrap::Stable)),
         specimen("textWrap(Pretty)",
                  text(kPassage).textWrap(TextWrap::Pretty)),
         specimen("textWrap(Balance)",
                  text(kPassage).textWrap(TextWrap::Balance))});
  }

  Element justifyRow() {
    const auto justified = [](TextJustify method) {
      return text(kPassage)
          .textWrap(TextWrap::Pretty)
          .textAlign(weave::TextAlignment::kJustify)
          .textJustify(method);
    };
    return row(
        "TEXT-JUSTIFY, JUSTIFIED",
        {specimen("textJustify(Auto)", justified(TextJustify::Auto)),
         specimen("textJustify(InterWord)", justified(TextJustify::InterWord)),
         specimen("textJustify(InterCharacter)",
                  justified(TextJustify::InterCharacter)),
         specimen("textJustify(None)", justified(TextJustify::None))});
  }

  Element describe() {
    const sketch::kit::Provide look(sheetTheme());
    return sketch::kit::page(
        {.title = u8"How the lines are set",
         .subtitle = u8"One passage, one measure: the breaker, then where a "
                     u8"justified line spends its slack",
         .footer = u8"Each longhand is one field of paragraph(), and inherits "
                   u8"as the paragraph setting does."},
        box().column().gap(44).children({wrapRow(), justifyRow()}));
  }
};

SIGIL_SKETCH_AS(TextWrapSpecimen, "text_wrap", "Specimen",
                "textWrap and textJustify — the four breakers and the four "
                "justification methods on one passage")
