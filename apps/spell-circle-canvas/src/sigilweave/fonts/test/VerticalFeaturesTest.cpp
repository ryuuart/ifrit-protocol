/** @file
 * The vertical OpenType features a column asks a face for: which ones
 * top-to-bottom shaping applies on its own, and which ones a style has to
 * name.
 *
 * Every assertion reads a shaped run off an instrument face
 * (test/assets/VerticalFeatures.ttf, built by the script beside it) whose
 * features share no consequence: one substitutes a letter, one substitutes
 * a different letter, one moves ink without moving the pen, and three move
 * the pen. So a shaped run names the feature that ran, which no real CJK
 * face allows — there the same substitution hangs off several tags at once.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Features.h>

#include <string>
#include <vector>

#include "support/Fonts.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

namespace {

/// 'Latn' in HarfBuzz's tag encoding — the instrument's letters are Latin
/// code points, so the column here is the one a rotated Latin run rides.
constexpr ScriptTag kLatin = 0x4C61746Eu;

sk_sp<SkTypeface> instrument() {
  static sk_sp<SkTypeface> face = ports::systemFontManager()->makeFromFile(
      SIGILWEAVE_TEST_ASSET_DIR "/VerticalFeatures.ttf");
  return face;
}

/// One run of @p text shaped down a column with @p features asked for.
ShapedWordRef column(const std::vector<FontFeature>& features,
                     std::u16string_view text) {
  ShapingStyle style;
  style.typeface = instrument();
  style.fontSize = 100.0f;  // 1000 upem: one font unit is a tenth of a px
  style.fontFeatures = features;
  return shapeWord(sharedContext(), style, style.typeface, text, kLatin,
                   /*rightToLeft=*/false, /*vertical=*/true);
}

/// The same run shaped along a line.
ShapedWordRef line(const std::vector<FontFeature>& features,
                   std::u16string_view text) {
  ShapingStyle style;
  style.typeface = instrument();
  style.fontSize = 100.0f;
  style.fontFeatures = features;
  return shapeWord(sharedContext(), style, style.typeface, text, kLatin,
                   /*rightToLeft=*/false, /*vertical=*/false);
}

}  // namespace

class VerticalFeatures : public ::testing::Test {
 protected:
  void SetUp() override {
    if (!instrument())
      GTEST_SKIP() << "test asset VerticalFeatures.ttf failed to load";
  }
};

TEST_F(VerticalFeatures, ColumnShapingTakesTheVerticalFormsUnasked) {
  // 'vert' is the one vertical feature nothing has to ask for: shaping a
  // run top-to-bottom applies it, which is what stands a bracket on its
  // vertical shape without any style saying so.
  const ShapedWordRef down = column({}, u"A");
  const ShapedWordRef across = line({}, u"A");
  ASSERT_EQ(down->glyphs.size(), 1u);
  ASSERT_EQ(across->glyphs.size(), 1u);
  EXPECT_NE(down->glyphs[0], across->glyphs[0])
      << "a column drew the horizontal form of a letter the face carries a "
         "vertical form for";
}

TEST_F(VerticalFeatures, TheVerticalFormsCanBeDeclined) {
  const ShapedWordRef plain = column({}, u"A");
  const ShapedWordRef declined = column({Features::verticalFormsOff}, u"A");
  const ShapedWordRef across = line({}, u"A");
  ASSERT_EQ(declined->glyphs.size(), 1u);
  EXPECT_NE(declined->glyphs[0], plain->glyphs[0]);
  EXPECT_EQ(declined->glyphs[0], across->glyphs[0])
      << "declining the vertical forms must leave the letter as a line sets it";
}

TEST_F(VerticalFeatures, TheRotationSetReachesWhatTheFormsDoNot) {
  // 'vrt2' is the wider set: on this face it covers a letter 'vert' leaves
  // alone, the way a real face's rotation set reaches proportional forms
  // its vertical forms do not. Column shaping does NOT apply it — a style
  // that wants it names it.
  const ShapedWordRef unasked = column({}, u"B");
  const ShapedWordRef asked = column({Features::verticalRotatedForms}, u"B");
  const ShapedWordRef across = line({}, u"B");
  ASSERT_EQ(unasked->glyphs.size(), 1u);
  ASSERT_EQ(asked->glyphs.size(), 1u);
  EXPECT_EQ(unasked->glyphs[0], across->glyphs[0])
      << "the rotation set must not run unasked";
  EXPECT_NE(asked->glyphs[0], unasked->glyphs[0])
      << "asking for the rotation set turned nothing";
}

TEST_F(VerticalFeatures, KanaFormsAreSubstitutedWhenAsked) {
  const ShapedWordRef unasked = column({}, u"S");
  const ShapedWordRef asked = column({Features::verticalKana}, u"S");
  ASSERT_EQ(asked->glyphs.size(), 1u);
  EXPECT_NE(asked->glyphs[0], unasked->glyphs[0]);
  EXPECT_FLOAT_EQ(asked->advance, unasked->advance)
      << "a kana form is a different shape in the same column step";
}

TEST_F(VerticalFeatures, AlternatesMoveTheInkAndNotThePen) {
  // 'valt' is the face recentring punctuation on the column axis: the
  // glyph moves along the column, and the step to the next glyph does not.
  const ShapedWordRef unasked = column({}, u"P");
  const ShapedWordRef asked = column({Features::verticalAlternates}, u"P");
  ASSERT_EQ(asked->glyphs.size(), 1u);
  EXPECT_EQ(asked->glyphs[0], unasked->glyphs[0]) << "an alternate is a POSE";
  EXPECT_FLOAT_EQ(asked->advance, unasked->advance)
      << "an alternate must not change the column step";
  // The instrument shifts the ink 200 font units — a fifth of the em, so
  // 20 px at this size — down the column.
  EXPECT_NEAR(asked->positions[0].y() - unasked->positions[0].y(), 20.0f, 0.5f)
      << "the alternate did not move the glyph down its column";
  EXPECT_FLOAT_EQ(asked->positions[0].x(), unasked->positions[0].x());
}

TEST_F(VerticalFeatures, ProportionalMetricsTightenTheColumnStep) {
  const ShapedWordRef unasked = column({}, u"Q");
  const ShapedWordRef asked =
      column({Features::proportionalVerticalMetrics}, u"Q");
  EXPECT_LT(asked->advance, unasked->advance)
      << "proportional vertical metrics must shorten a full-em step";
  // 300 font units off a 1000-unit em, at 100 px.
  EXPECT_NEAR(unasked->advance - asked->advance, 30.0f, 0.5f);
}

TEST_F(VerticalFeatures, HalfWidthMetricsHalveTheColumnStep) {
  const ShapedWordRef unasked = column({}, u"T");
  const ShapedWordRef asked =
      column({Features::halfWidthVerticalMetrics}, u"T");
  EXPECT_NEAR(asked->advance, unasked->advance * 0.5f, 0.5f);
}

TEST_F(VerticalFeatures, VerticalKerningIsAskedForLikeAnyOtherFeature) {
  // Horizontal kerning runs unasked; the vertical pair table does not, so
  // a column that wants it names it.
  const ShapedWordRef unasked = column({}, u"RR");
  const ShapedWordRef asked = column({Features::verticalKerning}, u"RR");
  ASSERT_EQ(asked->advances.size(), 2u);
  EXPECT_FLOAT_EQ(unasked->advances[0], unasked->advances[1])
      << "the pair kerned without being asked to";
  EXPECT_NEAR(unasked->advances[0] - asked->advances[0], 20.0f, 0.5f)
      << "the vertical pair adjustment did not reach the pen";
  EXPECT_FLOAT_EQ(asked->advances[1], unasked->advances[1])
      << "only the FIRST glyph of the pair carries the adjustment";
}

TEST_F(VerticalFeatures, AnAskedFeatureRunsInEitherDirection) {
  // A named feature is the style's instruction, not the direction's: the
  // shaper runs the lookups a style asks for whichever way the run is set.
  // So a style carrying column features and set along a line takes them
  // there too — the rotated forms substitute, and an alternate that moves
  // ink down a column moves it off a baseline.
  const std::vector<FontFeature> all = {Features::verticalRotatedForms,
                                        Features::verticalKana};
  const ShapedWordRef plain = line({}, u"BS");
  const ShapedWordRef dressed = line(all, u"BS");
  ASSERT_EQ(dressed->glyphs.size(), plain->glyphs.size());
  EXPECT_NE(dressed->glyphs, plain->glyphs)
      << "a feature the style named was gated on the writing direction — "
         "which would make the same style mean two things";
  // Only the vertical FORMS are the shaper's own, and those it takes for a
  // column alone.
  EXPECT_EQ(line({}, u"A")->glyphs,
            line({Features::verticalFormsOff}, u"A")->glyphs)
      << "the vertical forms never ran along a line, so declining them "
         "cannot change one";
}

TEST_F(VerticalFeatures, TheFeatureListIsPartOfShapingIdentity) {
  // Two runs differing only in a vertical feature must not share a cache
  // entry: the feature list is in the shape key, and a hit that ignored it
  // would draw the first caller's forms for the second.
  const ShapedWordRef plain = column({}, u"S");
  const ShapedWordRef kana = column({Features::verticalKana}, u"S");
  EXPECT_NE(plain.get(), kana.get());
  EXPECT_EQ(column({}, u"S").get(), plain.get()) << "the cache stopped hitting";
}
