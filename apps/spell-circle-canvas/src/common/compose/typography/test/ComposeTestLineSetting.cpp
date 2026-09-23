// The two longhands that say how a paragraph's lines are set — `textWrap`
// for the breaker and `textJustify` for where a justified line spends its
// slack — each laying a known passage exactly as the raw field of
// `paragraph()` it writes does, stated on the leaf, on an ancestor and in
// a rule.

#include <vector>

#include "GlyphCanvas.h"
#include "support/TextTestSupport.h"

namespace {

using sigil::weave::JustificationMethod;
using sigil::weave::LineBreakStrategy;
using sigil::weave::ParagraphBlock;
using sigil::weave::TextAlignment;

/** A passage that wraps to five lines at the measure below: at 12 px in
 *  the instrument face its greedy lines are 140.4, 144, 177.6, 97.2 and
 *  171.6 px wide. */
const char* kPassage =
    "Justification spends interword gaps before letterspacing, and "
    "reaches for horizontal glyph-scaling last of all.";
constexpr float kMeasure = 180.0f;

/** Where each line of the passage begins and ends, set under whatever
 *  @p state says on the box above the leaf. */
template <class State>
std::vector<SkRect> linesUnder(State state) {
  Host host(400, 400);
  host.composer.render(state(box()).children(
      {text(kPassage, whiteStyle(12)).key("t").width(kMeasure)}));
  host.frame();
  std::vector<SkRect> lines;
  for (const TextUnit& line : host.composer.units(
           "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
           sigil::weave::Unit::Line))
    lines.push_back(line.rect);
  return lines;
}

/** Whether two settings put every line in the same place. */
bool sameLines(const std::vector<SkRect>& one,
               const std::vector<SkRect>& other) {
  if (one.size() != other.size()) return false;
  for (size_t index = 0; index < one.size(); ++index)
    if (std::abs(one[index].left() - other[index].left()) > 0.01f ||
        std::abs(one[index].right() - other[index].right()) > 0.01f ||
        std::abs(one[index].top() - other[index].top()) > 0.01f)
      return false;
  return true;
}

/** The glyphs the passage hands the canvas, justified under @p method. */
std::vector<sigil::test::GlyphDraw> glyphsUnder(TextJustify method) {
  Host host(400, 400);
  host.composer.render(
      box()
          .textAlign(TextAlignment::kJustify)
          .textJustify(method)
          .children({text(kPassage, whiteStyle(12)).key("t").width(kMeasure)}));
  sigil::test::GlyphCanvas canvas(400, 400);
  host.composer.draw(canvas);
  return canvas.glyphs;
}

}  // namespace

TEST(ComposeLineSetting, EachTextWrapLaysThePassageAsTheRawFieldDoes) {
  struct Case {
    TextWrap wrap;
    ParagraphBlock raw;
  };
  const std::vector<Case> cases = {
      {TextWrap::Auto, {.lineBreak = LineBreakStrategy::kGreedy}},
      {TextWrap::Stable, {.lineBreak = LineBreakStrategy::kGreedy}},
      {TextWrap::Pretty, {.lineBreak = LineBreakStrategy::kKnuthPlass}},
      {TextWrap::Balance,
       {.balanceRaggedLines = true,
        .lineBreak = LineBreakStrategy::kKnuthPlass}},
  };
  for (const Case& each : cases) {
    const std::vector<SkRect> longhand =
        linesUnder([&](Element node) { return node.textWrap(each.wrap); });
    const std::vector<SkRect> raw =
        linesUnder([&](Element node) { return node.paragraph(each.raw); });
    ASSERT_GE(longhand.size(), 2u);
    EXPECT_TRUE(sameLines(longhand, raw)) << int(each.wrap);
  }
  // Balance evens the rag: no line is wider than the widest filled in turn.
  const auto widest = [](const std::vector<SkRect>& lines) {
    float width = 0;
    for (const SkRect& line : lines) width = std::max(width, line.width());
    return width;
  };
  EXPECT_LE(widest(linesUnder(
                [](Element node) { return node.textWrap(TextWrap::Balance); })),
            widest(linesUnder(
                [](Element node) { return node.textWrap(TextWrap::Auto); })) +
                0.5f);
}

TEST(ComposeLineSetting, ALaterTextWrapUndoesAnEarlierBalance) {
  EXPECT_TRUE(sameLines(
      linesUnder([](Element node) {
        return node.textWrap(TextWrap::Balance).textWrap(TextWrap::Pretty);
      }),
      linesUnder(
          [](Element node) { return node.textWrap(TextWrap::Pretty); })));
}

TEST(ComposeLineSetting, EachTextJustifyLaysThePassageAsTheRawFieldDoes) {
  struct Case {
    TextJustify justify;
    JustificationMethod raw;
  };
  const std::vector<Case> cases = {
      {TextJustify::Auto, JustificationMethod::kAuto},
      {TextJustify::InterWord, JustificationMethod::kInterWord},
      {TextJustify::InterCharacter, JustificationMethod::kInterCharacter},
      {TextJustify::None, JustificationMethod::kNone},
  };
  for (const Case& each : cases) {
    const std::vector<SkRect> longhand = linesUnder([&](Element node) {
      return node.textAlign(TextAlignment::kJustify).textJustify(each.justify);
    });
    const std::vector<SkRect> raw = linesUnder([&](Element node) {
      return node.paragraph({.alignment = TextAlignment::kJustify,
                             .justificationMethod = each.raw});
    });
    ASSERT_GE(longhand.size(), 2u);
    EXPECT_TRUE(sameLines(longhand, raw)) << int(each.justify);
  }
  // None sets the justified passage as a ragged one; the rest reach the
  // measure on the first line.
  const std::vector<SkRect> none = linesUnder([](Element node) {
    return node.textAlign(TextAlignment::kJustify)
        .textJustify(TextJustify::None);
  });
  const std::vector<SkRect> ragged =
      linesUnder([](Element node) { return node; });
  EXPECT_TRUE(sameLines(none, ragged));
  const std::vector<SkRect> spread = linesUnder([](Element node) {
    return node.textAlign(TextAlignment::kJustify)
        .textJustify(TextJustify::InterCharacter);
  });
  ASSERT_FALSE(spread.empty());
  EXPECT_NEAR(spread.front().right(), kMeasure, 1.0f);
}

TEST(ComposeLineSetting, InterCharacterMovesTheLettersInterWordDoesNot) {
  // The same lines, justified two ways: between the words alone the
  // letters of a word keep their shaped spacing, between every cluster
  // they move apart.
  const auto words = glyphsUnder(TextJustify::InterWord);
  const auto clusters = glyphsUnder(TextJustify::InterCharacter);
  ASSERT_GE(words.size(), 3u);
  ASSERT_EQ(words.size(), clusters.size());
  EXPECT_FLOAT_EQ(words.front().position.x(), clusters.front().position.x());
  EXPECT_GT(clusters[2].position.x() - clusters[1].position.x(),
            words[2].position.x() - words[1].position.x() + 0.01f)
      << "the letters of the first word did not move apart";
}

TEST(ComposeLineSetting, BothLonghandsAreStatableInARule) {
  const StyleSheet sheet{rule(".set")
                             .textWrap(TextWrap::Balance)
                             .textAlign(TextAlignment::kJustify)
                             .textJustify(TextJustify::InterCharacter)};
  Host host(400, 400);
  host.composer.render(
      box().applyStyleSheet(sheet).children({text(kPassage, whiteStyle(12))
                                                 .key("t")
                                                 .width(kMeasure)
                                                 .styleClass("set")}));
  host.frame();
  std::vector<SkRect> ruled;
  for (const TextUnit& line : host.composer.units(
           "t", sigil::weave::selectors::each(sigil::weave::Unit::Line),
           sigil::weave::Unit::Line))
    ruled.push_back(line.rect);
  EXPECT_TRUE(sameLines(
      ruled, linesUnder([](Element node) {
        return node.paragraph(
            {.alignment = TextAlignment::kJustify,
             .balanceRaggedLines = true,
             .lineBreak = LineBreakStrategy::kKnuthPlass,
             .justificationMethod = JustificationMethod::kInterCharacter});
      })));
}
