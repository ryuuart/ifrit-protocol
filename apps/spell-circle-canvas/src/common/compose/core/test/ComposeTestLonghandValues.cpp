// The face a family's name and a style choose: `fontFamily` over a
// family's name and `fontStyle` over upright, italic and an oblique angle.
// The cases that ask the machine's own families carry the `fonts` label
// through their suites; the lean and the italic axis are asked of the
// instrument faces and need nothing.

#include <include/core/SkBitmap.h>
#include <include/core/SkFontStyle.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/FontStyle.h>
#include <sigilcompose/core/SpanDeclarations.h>
#include <sigilcompose/core/StyleSheet.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/query/Selector.h>

#include <string>
#include <string_view>

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

/** Every character of the words, for a span over all of them. */
sigil::weave::Selector allOfTheWords() {
  return sigil::weave::selectors::range(
      {0, (uint32_t)std::u8string_view(kWords).size()});
}

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
  EXPECT_EQ(named,
            render(box().children({words().span(
                allOfTheWords(), SpanDeclarations().fontFamily("Georgia"))})));
}

TEST(ComposeFontFamily, ARunARuleNamesTakesItsFamily) {
  // A run named so is a virtual child of the leaf, and a rule about the
  // name finds its face as it would a child's.
  const StyleSheet sheet{sigil::compose::rule(".g").fontFamily("Georgia")};
  EXPECT_EQ(render(box().applyStyleSheet(sheet).children(
                {text(sigil::weave::rich().add(std::u8string_view(kWords), "g"))
                     .fontSize(28)
                     .ink({1, 1, 1, 1})})),
            render(box().fontFamily("Georgia").children({words()})));
}

TEST(ComposeFontFamily, AWeightBelowAFamilyFindsTheFamilysFaceAtIt) {
  // CSS matches a face per element from the family, the weight and the
  // style in force there, so a weight stated below the family is the same
  // statement as one stated beside it.
  ASSERT_TRUE(faceOf("Georgia", SkFontStyle::Bold()));
  const auto bold =
      render(box().fontFamily("Georgia").fontWeight(700).children({words()}));
  EXPECT_NE(bold, render(box().fontFamily("Georgia").children({words()})));
  EXPECT_EQ(
      render(box().fontFamily("Georgia").children({words().fontWeight(700)})),
      bold);
  EXPECT_EQ(render(box().fontFamily("Georgia").children({words().span(
                allOfTheWords(), SpanDeclarations().fontWeight(700))})),
            bold);
}

TEST(ComposeFontFamily, AFaceKeywordInAStrongerLayerStandsOverAFamily) {
  // The element's own `initial` on the face is stronger than the rule's
  // family, and `inherit` takes the parent's face over it.
  sigil::weave::Type initialFace, inheritedFace;
  initialFace.keywords.set(sigil::weave::TypeField::Face,
                           sigil::weave::Keyword::Initial);
  inheritedFace.keywords.set(sigil::weave::TypeField::Face,
                             sigil::weave::Keyword::Inherit);
  const StyleSheet sheet{sigil::compose::rule(".t").fontFamily("Georgia")};
  EXPECT_EQ(render(box().applyStyleSheet(sheet).children(
                {words().styleClass("t").font(initialFace)})),
            render(box().children({words()})));
  EXPECT_EQ(render(box().fontFamily("Impact").applyStyleSheet(sheet).children(
                {words().styleClass("t").font(inheritedFace)})),
            render(box().fontFamily("Impact").children({words()})));
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

// ---------------------------------------------------------------------------
// The lean and the italic axis, asked of the instruments

namespace {

/** One letter in @p face, large, white on black. */
Text letterIn(sk_sp<SkTypeface> face) {
  return text(u8"I")
      .font({.face = std::move(face), .size = 60})
      .ink({1, 1, 1, 1});
}

/** The leftmost inked column across the top and the bottom quarter of the
 *  ink's rows: a letter leaning right has the first further right. */
struct Lean {
  int top = 0;
  int bottom = 0;
};

Lean leanOf(const std::vector<uint32_t>& pixels, int width) {
  const int height = static_cast<int>(pixels.size()) / width;
  const auto inked = [&](int x, int y) {
    return (pixels[y * width + x] & 0x00FFFFFFu) != 0;
  };
  const auto leftmost = [&](int y) {
    for (int x = 0; x < width; ++x)
      if (inked(x, y)) return x;
    return width;
  };
  int first = -1, last = -1;
  for (int y = 0; y < height; ++y)
    if (leftmost(y) < width) {
      if (first < 0) first = y;
      last = y;
    }
  Lean lean{width, width};
  if (first < 0) return lean;
  const int quarter = std::max((last - first + 1) / 4, 1);
  for (int y = first; y < first + quarter; ++y)
    lean.top = std::min(lean.top, leftmost(y));
  for (int y = last - quarter + 1; y <= last; ++y)
    lean.bottom = std::min(lean.bottom, leftmost(y));
  return lean;
}

}  // namespace

TEST(ComposeOblique, APositiveAngleLeansRight) {
  // CSS's sign, written onto the face's slnt axis negated: the top of the
  // letter stands to the right of its foot.
  const Lean leaning =
      leanOf(render(box().children(
                 {letterIn(sigil::test::instrument::oblique()).fontStyle(12)})),
             320);
  EXPECT_GT(leaning.top, leaning.bottom + 3);
  const Lean upright = leanOf(
      render(box().children({letterIn(sigil::test::instrument::oblique())})),
      320);
  EXPECT_EQ(upright.top, upright.bottom);
}

TEST(ComposeOblique, AFamilyWithNoItalicLeansFourteenDegreesAndSaysSo) {
  // No italic face and no ital axis: the italic is the oblique CSS falls
  // back to, and the composer names the family that had neither.
  ::testing::internal::CaptureStderr();
  const auto italic =
      render(box().children({letterIn(sigil::test::instrument::oblique())
                                 .fontStyle(FontStyle::Italic)}));
  const std::string said = ::testing::internal::GetCapturedStderr();
  EXPECT_EQ(italic,
            render(box().children(
                {letterIn(sigil::test::instrument::oblique()).fontStyle(14)})));
  const Lean lean = leanOf(italic, 320);
  EXPECT_GT(lean.top, lean.bottom + 3);
  EXPECT_NE(said.find("Sigil Instrument Oblique"), std::string::npos) << said;
}

TEST(ComposeOblique, AnItalicAxisIsTheItalicOnANodeARuleAndASpan) {
  sigil::weave::Type onTheAxis;
  onTheAxis.face = sigil::test::instrument::italic();
  onTheAxis.variations.emplace_back("ital", 1.0f);
  const auto axis = render(box().children(
      {letterIn(sigil::test::instrument::italic()).font(onTheAxis)}));
  EXPECT_NE(
      axis,
      render(box().children({letterIn(sigil::test::instrument::italic())})));
  EXPECT_EQ(render(box().children({letterIn(sigil::test::instrument::italic())
                                       .fontStyle(FontStyle::Italic)})),
            axis);
  const StyleSheet sheet{
      sigil::compose::rule(".i").fontStyle(FontStyle::Italic)};
  EXPECT_EQ(render(box().applyStyleSheet(sheet).children(
                {letterIn(sigil::test::instrument::italic()).styleClass("i")})),
            axis);
  EXPECT_EQ(render(box().children(
                {letterIn(sigil::test::instrument::italic())
                     .span(sigil::weave::selectors::range({0, 1}),
                           SpanDeclarations().fontStyle(FontStyle::Italic))})),
            axis);
  // A run a rule makes italic by name is set on the axis too.
  EXPECT_EQ(
      render(box().applyStyleSheet(sheet).children(
          {text(sigil::weave::rich().add(std::u8string_view(u8"I"), "i"))
               .font({.face = sigil::test::instrument::italic(), .size = 60})
               .ink({1, 1, 1, 1})})),
      axis);
  // Normal under the italic sets the axis back.
  EXPECT_EQ(
      render(box()
                 .fontStyle(FontStyle::Italic)
                 .children({letterIn(sigil::test::instrument::italic())
                                .fontStyle(FontStyle::Normal)})),
      render(box().children({letterIn(sigil::test::instrument::italic())})));
}
