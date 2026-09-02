/** @file
 * The style vocabulary as plain values: the fluent variation sugar, the
 * paint-layer presets and their order, the StyleSet registry's lookup,
 * replacement and equality, and the designated-init `Type` a call site
 * names a style's numbers in. The feature preset tags are settled by the
 * compiler where they are declared, so nothing here asks about them.
 */

#include <gtest/gtest.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkPaint.h>
#include <sigilweave/style/Style.h>

#include <memory>
#include <type_traits>

using namespace sigil::weave;

// The umbrella still spells every subject.
static_assert(std::is_same_v<StyleSet::Entry::second_type, TextStyle>);

TEST(TextStyleVariations, TextStyleFluentSugarStaysOrderStable) {
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

TEST(PaintStyle, PaintLayersExposeCompletePaintAndExplicitOrder) {
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

TEST(StyleSet, LookupAnswersEveryNameAndTheBaseAnswersTheUnknownOnes) {
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

TEST(StyleSet, SetReplacesInPlaceAndEqualityIsExactAndOrdered) {
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

// A pass names its material by pointer: two passes sharing one instance are
// one pass, and a pass with a material is not the pass without it, so a
// restyle that attaches a material is seen by the draw-time comparison.
TEST(PaintStyle, PaintLayerMaterialComparesByIdentity) {
  const auto shared = std::shared_ptr<const sigil::material::Material>();
  PaintLayer plain(SK_ColorRED);
  PaintLayer withMaterial(SK_ColorRED);
  EXPECT_EQ(plain, withMaterial);
  withMaterial.material = std::shared_ptr<const sigil::material::Material>(
      shared, reinterpret_cast<const sigil::material::Material*>(&plain));
  EXPECT_NE(plain, withMaterial);
  PaintLayer same = withMaterial;
  EXPECT_EQ(same, withMaterial);
}

// ---------------------------------------------------------------------------
// Type — the designated-init aggregate a call site names a style's numbers
// in, and the TextStyle it builds.

TEST(Type, TheAggregatesNumbersLandOnTheStylesTwoHalves) {
  const TextStyle s = type({.size = 10.5f,
                            .color = {1, 0, 0, 1},
                            .track = 1.2f,
                            .condense = 0.8f,
                            .aliased = true});
  EXPECT_FLOAT_EQ(s.shaping.fontSize, 10.5f);
  EXPECT_FLOAT_EQ(s.shaping.letterSpacing, 1.2f);
  EXPECT_FLOAT_EQ(s.shaping.scaleX, 0.8f);
  EXPECT_TRUE(s.shaping.aliased);
  EXPECT_EQ(s.paint.foreground.getColor4f(), (SkColor4f{1, 0, 0, 1}));
}

TEST(Type, WeightAndSlantBecomeAxesAndTheExtraVariationsFollowThem) {
  Type t;
  t.weight = 700.0f;
  t.slant = -8.0f;
  t.variations = {FontVariation("wdth", 75.0f)};
  const TextStyle s = type(t);
  ASSERT_EQ(s.shaping.variations.size(), 3u);
  EXPECT_EQ(s.shaping.variations[0], FontVariation("wght", 700));
  EXPECT_EQ(s.shaping.variations[1], FontVariation("slnt", -8));
  EXPECT_EQ(s.shaping.variations[2], FontVariation("wdth", 75));
}

TEST(Type, TheEightBitLadderQuantisesWhereTheFloatOneDoesNot) {
  const SkColor4f c{0.4f, 0.4f, 0.4f, 1};
  EXPECT_EQ(type({.color = c}).paint.foreground.getColor4f(), c);
  EXPECT_NE(type({.color = c, .color8 = true}).paint.foreground.getColor4f(),
            c);
}
