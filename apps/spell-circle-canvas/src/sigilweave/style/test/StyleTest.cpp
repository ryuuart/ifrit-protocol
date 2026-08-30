/** @file
 * The style vocabulary as plain values: the fluent variation sugar, the
 * paint-layer presets and their order, the feature preset tags, and the
 * StyleSet registry's lookup, replacement and equality.
 */

#include <gtest/gtest.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkPaint.h>
#include <sigilweave/style/Features.h>
#include <sigilweave/style/Style.h>

using namespace sigil::weave;

TEST(ShaperVariations, TextStyleFluentSugarStaysOrderStable) {
  // weight()/opticalSize()/variation() replace in place when the axis is
  // already present — repeated fluent chains keep one order (one memoized
  // varied-typeface identity), never accumulate duplicates.
  TextStyle style;
  style.weight(500).opticalSize(36).weight(700);
  ASSERT_EQ(style.shaping.variations.size(), 2u);
  EXPECT_EQ(style.shaping.variations[0], FontVariation("wght", 700));
  EXPECT_EQ(style.shaping.variations[1], FontVariation("opsz", 36));
  style.variation("GRAD", 80);
  ASSERT_EQ(style.shaping.variations.size(), 3u);
  EXPECT_EQ(style.shaping.variations[2], FontVariation("GRAD", 80));

  // A chain that ends at the same design position compares EQUAL — the
  // in-place replace keeps first-mention order, so both styles carry
  // [wght, opsz, GRAD] and share one shape-cache identity.
  TextStyle same;
  same.weight(700).opticalSize(36).variation("GRAD", 80);
  EXPECT_TRUE(style == same);
}

TEST(Typography, PaintLayersExposeCompletePaintAndExplicitOrder) {
  PaintStyle style(SK_ColorWHITE);
  style.addUnderlay(PaintLayer::dropShadow(0x66000000, {3, 4}, 2.0f))
      .addUnderlay(PaintLayer::glow(0x550000FF, 5.0f))
      .addUnderlay(PaintLayer::outline(SK_ColorBLACK, 3.0f));

  SkPaint customOverlay;
  customOverlay.setAntiAlias(true);
  customOverlay.setColor(SK_ColorGREEN);
  customOverlay.setStyle(SkPaint::kStroke_Style);
  customOverlay.setStrokeWidth(1.0f);
  customOverlay.setBlendMode(SkBlendMode::kScreen);
  style.addOverlay(PaintLayer(customOverlay, {-1, -1}));

  ASSERT_EQ(style.underlays.size(), 3u);
  EXPECT_EQ(style.underlays[0].offset, (SkVector{3, 4}));
  EXPECT_NE(style.underlays[0].paint.getMaskFilter(), nullptr);
  EXPECT_NE(style.underlays[1].paint.getMaskFilter(), nullptr);
  EXPECT_EQ(style.underlays[2].paint.getStyle(), SkPaint::kStroke_Style);
  EXPECT_FLOAT_EQ(style.underlays[2].paint.getStrokeWidth(), 3.0f);
  ASSERT_EQ(style.overlays.size(), 1u);
  EXPECT_EQ(style.overlays[0].paint.getBlendMode_or(SkBlendMode::kSrcOver),
            SkBlendMode::kScreen);

  PaintStyle identical = style;
  EXPECT_EQ(identical, style);
  identical.overlays[0].offset.set(0, 0);
  EXPECT_FALSE(identical == style);
}

TEST(FeaturePresets, TagsAreConstexprAndWellFormed) {
  static_assert(Features::tabularNumbers == FontFeature{"tnum", 1});
  static_assert(Features::standardLigaturesOff == FontFeature{"liga", 0});
  static_assert(Features::smallCaps == FontFeature{"smcp", 1});
  static_assert(Features::stylisticSet(1) == FontFeature{"ss01", 1});
  static_assert(Features::stylisticSet(20) == FontFeature{"ss20", 1});
  static_assert(Features::stylisticSet(7) == FontFeature{"ss07", 1});
  // Out-of-range indices clamp instead of producing bogus tags.
  static_assert(Features::stylisticSet(0) == FontFeature{"ss01", 1});
  static_assert(Features::stylisticSet(99) == FontFeature{"ss20", 1});
  SUCCEED();
}

TEST(StyleSetTest, LookupAnswersEveryNameAndTheBaseAnswersTheUnknownOnes) {
  TextStyle base;
  base.shaping.fontSize = 12.0f;
  TextStyle alert;
  alert.shaping.fontSize = 12.0f;
  alert.paint.foreground.setColor(SK_ColorRED);

  StyleSet styles(base);
  styles.set("alert", alert);

  EXPECT_TRUE(styles["alert"] == alert);
  EXPECT_TRUE(styles.contains("alert"));
  ASSERT_NE(styles.find("alert"), nullptr);

  // The unknown-name contract: a lookup ALWAYS returns a style, and the one
  // it returns for a name nobody registered is the base. A misspelling is
  // therefore visible as base-styled text, never as text that vanished.
  EXPECT_TRUE(styles["alrt"] == base) << "an unknown name must fall back";
  EXPECT_TRUE(styles[""] == base) << "the empty name is an unknown name";
  EXPECT_FALSE(styles.contains("alrt"));
  EXPECT_EQ(styles.find("alrt"), nullptr) << "find() reports absence";
  EXPECT_EQ(styles.size(), 1u) << "a failed lookup must not register a name";

  // A default-constructed set still answers: the base is a default style.
  EXPECT_TRUE(StyleSet{}["anything"] == TextStyle{});
}

TEST(StyleSetTest, SetReplacesInPlaceAndEqualityIsExactAndOrdered) {
  TextStyle small;
  small.shaping.fontSize = 9.0f;
  TextStyle large;
  large.shaping.fontSize = 24.0f;

  StyleSet a;
  a.set("head", small).set("body", large);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a.entries()[0].first, "head") << "entries keep insertion order";

  // Re-setting a registered name replaces it where it already sits.
  a.set("head", large);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a.entries()[0].first, "head");
  EXPECT_TRUE(a["head"] == large);

  // Equality is what lets a StyleSet ride inside a larger comparable value:
  // same base, same entries, same order.
  StyleSet b;
  b.set("head", large).set("body", large);
  EXPECT_TRUE(a == b);
  b.base(small);
  EXPECT_FALSE(a == b) << "the base participates in equality";

  StyleSet reordered;
  reordered.set("body", large).set("head", large);
  EXPECT_FALSE(a == reordered) << "equality is order-sensitive";

  StyleSet extra = a;
  extra.set("note", small);
  EXPECT_FALSE(a == extra);
}
