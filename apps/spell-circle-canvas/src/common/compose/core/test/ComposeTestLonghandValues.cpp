// The three longhands that take CSS's value spaces: `fontFamily` over a
// family's name, `fontStyle` over upright, italic and an oblique angle, and
// `textIndent` over any length. The family and style cases ask the machine's
// own families, so their suites carry the `fonts` label; the indent is
// measured in the instrument face and needs nothing.

#include <include/core/SkBitmap.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/FontStyle.h>
#include <sigilcompose/core/SpanDeclarations.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilweave/query/Selector.h>

#include <string>

#include "support/CoreTestSupport.h"

namespace {

using sigil::compose::FontStyle;

/** Words with a figure of every height, so two faces never draw them
 *  alike. */
constexpr const char8_t* kWords = u8"Hamburgefonstiv";

/** The whole surface, read back, for comparing two renders pixel for
 *  pixel. */
std::vector<uint32_t> pixelsOf(Host& host) {
  host.frame();
  SkBitmap bitmap;
  bitmap.allocPixels(host.surface->imageInfo());
  host.surface->readPixels(bitmap.pixmap(), 0, 0);
  const auto* first = static_cast<const uint32_t*>(bitmap.getPixels());
  return {first, first + bitmap.width() * bitmap.height()};
}

/** @p page rendered on a host of its own and read back. */
std::vector<uint32_t> render(const Element& page) {
  Host host(320, 80);
  host.composer.render(page);
  return pixelsOf(host);
}

/** The leaf every face case sets: white words at a size where two faces
 *  differ by whole pixels. */
Text words() { return text(kWords).fontSize(28).ink({1, 1, 1, 1}).key("t"); }

/** A family's face at a style, as the font context finds it. */
sk_sp<SkTypeface> faceOf(const char* family,
                         SkFontStyle style = SkFontStyle::Normal()) {
  return fonts().familyTypeface(family, style);
}

}  // namespace

// ---------------------------------------------------------------------------
// fontFamily

TEST(ComposeFontFamily, ANameIsTheFaceTheContextFindsForIt) {
  ASSERT_TRUE(faceOf("Georgia"));
  const auto named = render(box().fontFamily("Georgia").children({words()}));
  EXPECT_EQ(
      named,
      render(box().font({.face = faceOf("Georgia")}).children({words()})));
  EXPECT_NE(named,
            render(box().font({.face = faceOf("Impact")}).children({words()})));
}

TEST(ComposeFontFamily, AFamilyNobodyInstalledLeavesTheInheritedFace) {
  const Element inherited = box().font({.face = faceOf("Georgia")});
  ::testing::internal::CaptureStderr();
  const auto missing = render(Element(inherited).children(
      {box().fontFamily("Nobody Installed This Family").children({words()})}));
  const std::string said = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(missing, render(Element(inherited).children({words()})));
  EXPECT_NE(said.find("Nobody Installed This Family"), std::string::npos)
      << said;
}

TEST(ComposeFontFamily, ARuleAndASpanNameAFamilyAsTheElementDoes) {
  const auto named = render(box().fontFamily("Georgia").children({words()}));
  const StyleSheet sheet{sigil::compose::rule(".t").fontFamily("Georgia")};
  EXPECT_EQ(
      named,
      render(box().applyStyleSheet(sheet).children({words().styleClass("t")})));
  EXPECT_EQ(named, render(box().children({words().span(
                       sigil::weave::selectors::range(
                           {0, (uint32_t)std::u8string_view(kWords).size()}),
                       SpanDeclarations().fontFamily("Georgia"))})));
}

TEST(ComposeFontStatements, TheLaterOfAFamilyAndAFaceStands) {
  // One property, two spellings: the face of `font()` and the family by
  // name replace each other, so whichever was written last is all the
  // node says.
  const sk_sp<SkTypeface> face = sigil::test::instrument::sans();
  EXPECT_TRUE(sameDescription(box().fontFamily("Georgia").font({.face = face}),
                              box().font({.face = face})));
  EXPECT_TRUE(sameDescription(box().font({.face = face}).fontFamily("Georgia"),
                              box().fontFamily("Georgia")));
  // An empty name is the default family, which the partial can say.
  EXPECT_TRUE(sameDescription(box().fontFamily(""),
                              box().font({.face = weave::defaultFace()})));
}

// ---------------------------------------------------------------------------
// fontStyle

TEST(ComposeFontStatements, ANumberIsAnObliqueLeaningRightWhenPositive) {
  // CSS's sign: a positive angle leans right, which is a NEGATIVE value on
  // the face's slnt axis, and a bare number is the oblique of that angle.
  EXPECT_TRUE(sameDescription(box().fontStyle(12), box().font({.slant = -12})));
  EXPECT_TRUE(sameDescription(box().fontStyle(FontStyle::oblique(12)),
                              box().fontStyle(12)));
  EXPECT_TRUE(sameDescription(box().fontStyle(FontStyle::oblique()),
                              box().fontStyle(14)));
  EXPECT_TRUE(sameDescription(box().fontStyle(FontStyle::Normal),
                              box().font({.slant = 0})));
  EXPECT_FALSE(sameDescription(box().fontStyle(FontStyle::Italic),
                               box().fontStyle(FontStyle::Normal)));
  // An italic is a style of its own, and a lean written after it replaces
  // it.
  EXPECT_TRUE(sameDescription(box().fontStyle(FontStyle::Italic).fontStyle(8),
                              box().fontStyle(8)));
}

TEST(ComposeFontStyle, AnItalicIsTheFamilysItalicFace) {
  const sk_sp<SkTypeface> italicFace = faceOf("Georgia", SkFontStyle::Italic());
  ASSERT_TRUE(italicFace);
  ASSERT_NE(italicFace->fontStyle().slant(), SkFontStyle::kUpright_Slant);
  const auto italic = render(box()
                                 .fontFamily("Georgia")
                                 .fontStyle(FontStyle::Italic)
                                 .children({words()}));
  EXPECT_EQ(italic,
            render(box().font({.face = italicFace}).children({words()})));
  EXPECT_NE(italic, render(box().fontFamily("Georgia").children({words()})));
}

TEST(ComposeFontStyle, AnItalicInheritsIntoAFamilyNamedUnderIt) {
  // The style is inherited beside the face, so a family named further
  // down is found at the italic in force, as CSS finds it.
  const sk_sp<SkTypeface> courierItalic =
      faceOf("Courier New", SkFontStyle::Italic());
  ASSERT_TRUE(courierItalic);
  ASSERT_NE(courierItalic->fontStyle().slant(), SkFontStyle::kUpright_Slant);
  EXPECT_EQ(
      render(
          box()
              .fontFamily("Georgia")
              .fontStyle(FontStyle::Italic)
              .children({box().fontFamily("Courier New").children({words()})})),
      render(box().font({.face = courierItalic}).children({words()})));
}

TEST(ComposeFontStyle, NormalUnderAnItalicStandsUprightAgain) {
  EXPECT_EQ(
      render(box()
                 .fontFamily("Georgia")
                 .fontStyle(FontStyle::Italic)
                 .children(
                     {box().fontStyle(FontStyle::Normal).children({words()})})),
      render(box().fontFamily("Georgia").children({words()})));
}

TEST(ComposeFontStyle, AFamilyWithNoItalicLeansFourteenDegreesAndSaysSo) {
  // Impact has one face and no axis: its italic is the oblique CSS falls
  // back to, and the composer says which family had none.
  const sk_sp<SkTypeface> impact = faceOf("Impact");
  ASSERT_TRUE(impact);
  ::testing::internal::CaptureStderr();
  const auto italic = render(box()
                                 .fontFamily("Impact")
                                 .fontStyle(FontStyle::Italic)
                                 .children({words()}));
  const std::string said = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(
      italic,
      render(box().font({.face = impact}).fontStyle(14).children({words()})));
  EXPECT_NE(said.find("Impact"), std::string::npos) << said;
}

// ---------------------------------------------------------------------------
// textIndent

namespace {

/** A passage long enough to wrap, set in the instrument so an indent
 *  moves its first line by exactly the pixels asked for. */
Text passage(float width) {
  return text(u8"AAAA AAAA AAAA AAAA AAAA AAAA")
      .font({.face = sigil::test::instrument::sans(), .size = 10})
      .ink({1, 1, 1, 1})
      .width(width);
}

}  // namespace

TEST(ComposeTextIndent, ALengthInTheFontIsResolvedWhereItIsStated) {
  // 2 em at 10 px is 20 px, and the leaf under a larger size inherits the
  // twenty pixels, not the two em.
  const auto pixels = render(box().textIndent(20).children({passage(160)}));
  EXPECT_EQ(render(box()
                       .fontSize(10)
                       .textIndent(sigil::weave::em(2))
                       .children({passage(160)})),
            pixels);
  EXPECT_EQ(
      render(box().textIndent(sigil::weave::pt(15)).children({passage(160)})),
      pixels);
  EXPECT_NE(render(box().children({passage(160)})), pixels);
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
}
