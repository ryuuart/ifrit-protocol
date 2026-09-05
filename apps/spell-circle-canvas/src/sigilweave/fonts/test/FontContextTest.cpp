/** @file
 * The font service on its own: the fallback memo keyed by language, the
 * transient varied clone that is built and never retained, and the pair a
 * shaper sets by measuring outlines rather than reading metrics.
 */

#include <gtest/gtest.h>
#include <include/core/SkFontMgr.h>
#include <include/core/SkTypeface.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "support/Faces.h"
using namespace sigil::weave;
using namespace sigil::weave::test;

TEST(FallbackMemo, AResolvedFallbackIsKeyedByTheLanguageItWasAskedFor) {
  // Two regional faces over one shared character is the shape of the
  // question, and two instruments are what make it askable: a Latin-only
  // primary that answers nothing for the character, and two faces that do
  // under two family names, so a resolver told a different language has a
  // different face to hand back and the memo has two answers to keep apart.
  sk_sp<SkFontMgr> fontManager = ports::systemFontManager();
  sk_sp<SkTypeface> primary = sigil::test::instrument::sans();
  sk_sp<SkTypeface> simplified = sigil::test::instrument::hanSans();
  sk_sp<SkTypeface> traditional = sigil::test::instrument::hanSerif();
  constexpr SkUnichar kSharedHanCharacter = 0x4E2D;  // 中
  ASSERT_TRUE(primary && simplified && traditional);
  ASSERT_EQ(primary->unicharToGlyph(kSharedHanCharacter), 0);
  ASSERT_NE(simplified->unicharToGlyph(kSharedHanCharacter), 0);
  ASSERT_NE(traditional->unicharToGlyph(kSharedHanCharacter), 0);
  ASSERT_NE(simplified->uniqueID(), traditional->uniqueID());

  int resolverCalls = 0;
  FontContext fontContext(std::move(fontManager), nullptr,
                          [&](SkFontMgr&, const SkTypeface&, int32_t,
                              std::string_view languageTag) {
                            resolverCalls++;
                            return languageTag == "zh-Hant" ? traditional
                                                            : simplified;
                          });

  sk_sp<SkTypeface> hans =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hans");
  sk_sp<SkTypeface> hant =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hant");
  ASSERT_TRUE(hans);
  ASSERT_TRUE(hant);
  EXPECT_EQ(hans->uniqueID(), simplified->uniqueID());
  EXPECT_EQ(hant->uniqueID(), traditional->uniqueID());
  EXPECT_EQ(resolverCalls, 2);

  // Both exact language keys are now warm.
  sk_sp<SkTypeface> warmHans =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hans");
  sk_sp<SkTypeface> warmHant =
      fontContext.resolveTypeface(primary, kSharedHanCharacter, "zh-Hant");
  EXPECT_EQ(warmHans->uniqueID(), simplified->uniqueID());
  EXPECT_EQ(warmHant->uniqueID(), traditional->uniqueID());
  EXPECT_EQ(resolverCalls, 2);
}

TEST(VariedTypeface, TheTransientCloneIsNeverPutInThePermanentMemo) {
  // The memo has no cap and no eviction, so a coordinate that varies
  // continuously must not go into it: one retained clone per frame, forever,
  // is a leak whether or not the value ever repeats. The transient entry
  // point is what such a caller asks for, and the property it promises is
  // that the retained population does not move.
  FontContext fontContext(ports::systemFontManager());
  sk_sp<SkTypeface> base = sigil::test::instrument::variable();
  ASSERT_TRUE(base);

  EXPECT_EQ(fontContext.variedTypefaceCount(), 0u);
  for (int step = 0; step < 300; ++step) {
    const std::vector<FontVariation> axes = {
        {"wght", 100.0f + 0.0031f * (float)step}};  // never the same twice
    ASSERT_TRUE(fontContext.variedTypefaceTransient(base, axes));
  }
  EXPECT_EQ(fontContext.variedTypefaceCount(), 0u)
      << "the transient path put clones in the permanent memo";

  // Repeating one coordinate does not resurrect it into the memo either.
  // (Whether the two calls hand back the same object is Skia's business —
  // its own typeface cache may satisfy a clone — and is exactly why the
  // transient face promises no stable identity to key anything on.)
  const std::vector<FontVariation> fixed = {{"wght", 640.0f}};
  ASSERT_TRUE(fontContext.variedTypefaceTransient(base, fixed));
  ASSERT_TRUE(fontContext.variedTypefaceTransient(base, fixed));
  EXPECT_EQ(fontContext.variedTypefaceCount(), 0u);

  // …and the retaining entry point still retains, one per distinct
  // coordinate, which is what a bounded ladder of coordinates relies on.
  EXPECT_TRUE(fontContext.variedTypeface(base, fixed));
  EXPECT_EQ(fontContext.variedTypefaceCount(), 1u);
  EXPECT_TRUE(fontContext.variedTypeface(base, fixed));
  EXPECT_EQ(fontContext.variedTypefaceCount(), 1u);

  // An empty variation list is the base either way, and retains nothing.
  EXPECT_EQ(fontContext.variedTypefaceTransient(base, {}).get(), base.get());
  EXPECT_EQ(fontContext.variedTypefaceCount(), 1u);
}

// ── Optical kerning ───────────────────────────────────────────────────────

/// A face whose outlines are what optical kerning measures. The instrument
/// is the one asked for because its A and V lean into each other far enough
/// for a measured pair to differ from a metric one, and by exactly as much
/// on every machine.
class OpticalKerning : public ::testing::Test {
 protected:
  void SetUp() override {
    m_typeface = sigil::test::instrument::optical();
    ASSERT_TRUE(m_typeface);
  }

  FontContext m_fontContext{ports::systemFontManager()};
  sk_sp<SkTypeface> m_typeface;
};

TEST_F(OpticalKerning, APairIsSetByWhatItsOutlinesLeaveBetweenThem) {
  ShapingStyle metric;
  metric.typeface = m_typeface;
  metric.fontSize = 64.0f;
  ShapingStyle optical = metric;
  optical.opticalKerning = true;

  // A pair whose outlines lean into each other: the diagonal of the V
  // stands clear of the A's own diagonal for most of their height, so a
  // measurement of the two edges closes them further than a face's even
  // pair sits.
  const ShapedWordRef byMetrics =
      shapeWord(m_fontContext, metric, m_typeface, u"AV", 0, false, false);
  const ShapedWordRef byOutlines =
      shapeWord(m_fontContext, optical, m_typeface, u"AV", 0, false, false);
  ASSERT_TRUE(byMetrics);
  ASSERT_TRUE(byOutlines);
  ASSERT_EQ(byMetrics->glyphs.size(), 2u);
  ASSERT_EQ(byOutlines->glyphs.size(), 2u);
  EXPECT_LT(byOutlines->advance, byMetrics->advance);
  // The second glyph moved with the advance: a kerned pair is two glyphs
  // closer together, not one advance quietly disagreeing with a position.
  EXPECT_NEAR(byOutlines->positions[1].x() - byMetrics->positions[1].x(),
              byOutlines->advance - byMetrics->advance, 0.01f);

  // The two answers are two cache entries, not one: a word shaped under one
  // must never be handed back for the other.
  EXPECT_NE(byMetrics.get(), byOutlines.get());
  EXPECT_EQ(
      shapeWord(m_fontContext, optical, m_typeface, u"AV", 0, false, false)
          .get(),
      byOutlines.get());
}

TEST_F(OpticalKerning, AGlyphWithNoNeighbourIsLeftAlone) {
  ShapingStyle style;
  style.typeface = m_typeface;
  style.fontSize = 32.0f;
  ShapingStyle optical = style;
  optical.opticalKerning = true;
  const ShapedWordRef plain =
      shapeWord(m_fontContext, style, m_typeface, u"o", 0, false, false);
  const ShapedWordRef kerned =
      shapeWord(m_fontContext, optical, m_typeface, u"o", 0, false, false);
  ASSERT_TRUE(plain);
  ASSERT_TRUE(kerned);
  EXPECT_FLOAT_EQ(kerned->advance, plain->advance);
}
