// textIndent over any length: pixels, a length in the font resolved where
// it is stated and inherited as pixels, a percentage of each passage's own
// measure, a custom property or a calc(), on an element, a rule and a
// named paragraph alike. Every passage is set in the instrument face, so an
// indent moves its first line by exactly the pixels asked for and nothing
// depends on the machine.

#include <include/core/SkBitmap.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/StyleSheet.h>

#include <array>
#include <string_view>
#include <vector>

#include "support/CoreTestSupport.h"

namespace {

/** The whole surface, read back after one frame. */
std::vector<uint32_t> render(const Element& page) {
  Host host(320, 80);
  host.composer.render(page);
  host.frame();
  SkBitmap bitmap;
  bitmap.allocPixels(host.surface->imageInfo());
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  const auto* first = static_cast<const uint32_t*>(bitmap.getPixels());
  return {first, first + bitmap.width() * bitmap.height()};
}

/** A passage long enough to wrap, set in the instrument at @p size. */
Text passage(float width, float size = 10) {
  return text(u8"AAAA AAAA AAAA AAAA AAAA AAAA")
      .font({.face = sigil::test::instrument::sans(), .size = size})
      .ink({1, 1, 1, 1})
      .width(width);
}

/** The first-line indent written as a keyword in a paragraph partial. */
sigil::weave::ParagraphBlock indentAs(sigil::weave::Keyword keyword) {
  sigil::weave::ParagraphBlock block;
  block.keywords.set(sigil::weave::ParagraphField::FirstLineIndent, keyword);
  return block;
}

}  // namespace

TEST(ComposeTextIndent, ALengthInTheFontIsResolvedWhereItIsStated) {
  // 2 em at the box's 10 px is 20 px, and the passage under it, set at 20
  // px, inherits the twenty pixels — not two of its own em, which would be
  // forty.
  const auto twenty = render(box().textIndent(20).children({passage(160, 20)}));
  EXPECT_EQ(render(box()
                       .fontSize(10)
                       .textIndent(sigil::weave::em(2))
                       .children({passage(160, 20)})),
            twenty);
  EXPECT_NE(render(box().textIndent(40).children({passage(160, 20)})), twenty);
  EXPECT_EQ(
      render(
          box().textIndent(sigil::weave::pt(15)).children({passage(160, 20)})),
      twenty);
  EXPECT_NE(render(box().children({passage(160, 20)})), twenty);
}

TEST(ComposeTextIndent, APercentageIsOneOfEachPassagesOwnMeasure) {
  // Inherited as the percentage: the same ten percent is 16 px of one
  // passage and 24 px of the other.
  EXPECT_NE(render(box().textIndent(pct(10)).children({passage(160)})),
            render(box().children({passage(160)})));
  EXPECT_EQ(render(box().textIndent(pct(10)).children({passage(160)})),
            render(box().textIndent(16).children({passage(160)})));
  EXPECT_EQ(render(box().textIndent(pct(10)).children(
                {box().children({passage(240)})})),
            render(box().textIndent(24).children({passage(240)})));
}

TEST(ComposeTextIndent, APropertyAndACalcResolveAsTheLengthTheyHold) {
  const auto twenty = render(box().textIndent(20).children({passage(160)}));
  EXPECT_EQ(render(box()
                       .fontSize(10)
                       .var("indent", Dimension(sigil::weave::em(2)))
                       .textIndent(var("indent"))
                       .children({passage(160)})),
            twenty);
  EXPECT_EQ(render(box()
                       .fontSize(10)
                       .textIndent(*parseDimension("calc(1em + 10px)"))
                       .children({passage(160)})),
            twenty);
}

TEST(ComposeTextIndent, ARuleStatesEveryUnitAsAnElementDoes) {
  const auto indented = [](const StyleSheet& sheet) {
    return render(
        box().applyStyleSheet(sheet).children({passage(160).styleClass("p")}));
  };
  // Two em of the matched passage's own 10 px, and ten percent of its 160.
  EXPECT_EQ(indented(StyleSheet{rule(".p").textIndent(sigil::weave::em(2))}),
            render(box().textIndent(20).children({passage(160)})));
  EXPECT_EQ(indented(StyleSheet{rule(".p").textIndent(pct(10))}),
            render(box().textIndent(16).children({passage(160)})));
}

TEST(ComposeTextIndent, ANamedParagraphTakesAnIndentInAnyUnit) {
  const std::array<std::string_view, 1> names{"lead"};
  const auto named = [&](const StyleSheet& sheet) {
    return render(box().applyStyleSheet(sheet).children(
        {passage(160).paragraphStyles(names)}));
  };
  EXPECT_EQ(named(StyleSheet{rule(".lead").textIndent(pct(10))}),
            render(box().textIndent(16).children({passage(160)})));
  EXPECT_EQ(named(StyleSheet{rule(".lead").textIndent(sigil::weave::em(2))}),
            render(box().textIndent(20).children({passage(160)})));
}

TEST(ComposeTextIndent, AKeywordInAStrongerLayerStandsOverAWeakerIndent) {
  const auto none = render(box().children({passage(160)}));
  // `initial` on the passage stops the percentage its parent states…
  EXPECT_EQ(render(box().textIndent(pct(10)).children({passage(160).paragraph(
                indentAs(sigil::weave::Keyword::Initial))})),
            none);
  // …and on the element stops the one a rule matching it states.
  EXPECT_EQ(
      render(box()
                 .applyStyleSheet(StyleSheet{rule(".p").textIndent(pct(10))})
                 .children({passage(160).styleClass("p").paragraph(
                     indentAs(sigil::weave::Keyword::Initial))})),
      none);
  // `inherit` takes the parent's percentage over the rule's pixels.
  EXPECT_EQ(render(box()
                       .textIndent(pct(10))
                       .applyStyleSheet(StyleSheet{rule(".p").textIndent(40)})
                       .children({passage(160).styleClass("p").paragraph(
                           indentAs(sigil::weave::Keyword::Inherit))})),
            render(box().textIndent(16).children({passage(160)})));
}

TEST(ComposeTextIndent, AutoIsNoIndentAndIsRefused) {
  ::testing::internal::CaptureStderr();
  const Element refused = box().textIndent(autoDimension());
  const std::string said = ::testing::internal::GetCapturedStderr();
  EXPECT_TRUE(sameDescription(refused, box()));
  EXPECT_NE(said.find("textIndent"), std::string::npos) << said;
}

TEST(ComposeTextIndent, TheLaterOfPixelsAndAnotherUnitStands) {
  EXPECT_TRUE(
      sameDescription(box().textIndent(sigil::weave::em(2)).textIndent(8),
                      box().textIndent(8)));
  EXPECT_TRUE(sameDescription(box().textIndent(8).textIndent(pct(5)),
                              box().textIndent(pct(5))));
  EXPECT_TRUE(sameDescription(
      box().textIndent(pct(5)).paragraph({.firstLineIndent = 8}),
      box().textIndent(8)));
  // A length written after the keyword is the later statement.
  EXPECT_TRUE(
      sameDescription(box()
                          .paragraph(indentAs(sigil::weave::Keyword::Initial))
                          .textIndent(pct(5)),
                      box().textIndent(pct(5))));
}
