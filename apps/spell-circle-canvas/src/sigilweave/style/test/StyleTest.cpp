/** @file
 * The style vocabulary as plain values: the fluent variation sugar, the
 * paint-layer presets and their order, the StyleSheet's lookup, replacement
 * and equality, and the partial `Type` a call site names a style's numbers
 * in — what it overlays, what it leaves alone, and how a relative length
 * becomes pixels. The feature preset tags are settled by the compiler where
 * they are declared, so nothing here asks about them.
 */

#include <gtest/gtest.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkPaint.h>
#include <sigilweave/kit/PaintLayers.h>
#include <sigilweave/style/Style.h>

#include <memory>
#include <type_traits>

using namespace sigil::weave;

// The umbrella still spells every subject, and a sheet's entries are the
// PARTIALS a class is, not whole styles.
static_assert(std::is_same_v<StyleSheet::Entry::second_type, Type>);

TEST(TextStyle, TheFluentSugarAppendsInTheOrderItWasCalled) {
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
  style.addUnderlay(sigil::weave::kit::dropShadow(0x66000000, {3, 4}, 2.0f))
      .addUnderlay(sigil::weave::kit::glow(0x550000FF, 5.0f))
      .addUnderlay(sigil::weave::kit::outline(SK_ColorBLACK, 3.0f));

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

TEST(StyleSheet, AClassResolvesThroughTheSheetAndAnAbsentNameAnswersTheBase) {
  TextStyle base;
  base.shaping.fontSize = 12.0f;
  base.paint.foreground.setColor4f({1, 1, 1, 1}, nullptr);

  StyleSheet styles(base);
  // The class states only the colour — the size is the base's, which is the
  // whole point of an entry being a partial.
  styles.set("alert", Type{.color = SkColor4f{1, 0, 0, 1}});

  const TextStyle alert = styles["alert"];
  EXPECT_FLOAT_EQ(alert.shaping.fontSize, 12.0f)
      << "a field the class is silent about is the base's";
  EXPECT_EQ(alert.paint.foreground.getColor4f(), (SkColor4f{1, 0, 0, 1}));
  EXPECT_TRUE(styles.contains("alert"));
  ASSERT_NE(styles.find("alert"), nullptr);
  EXPECT_EQ(styles.find("alert")->color, (SkColor4f{1, 0, 0, 1}))
      << "find() hands back the partial, not the style it resolves to";

  // The unknown-name contract: a lookup ALWAYS returns a style, and the one
  // it returns for a name nobody registered is the base alone. A misspelling
  // is therefore visible as base-styled text, never as text that vanished.
  EXPECT_TRUE(styles["alrt"] == base) << "an unknown name must fall back";
  EXPECT_TRUE(styles[""] == base) << "the empty name is an unknown name";
  EXPECT_FALSE(styles.contains("alrt"));
  EXPECT_EQ(styles.find("alrt"), nullptr) << "find() reports absence";
  EXPECT_EQ(styles.size(), 1u) << "a failed lookup must not register a name";

  // A default-constructed sheet still answers: the base is a default style.
  EXPECT_TRUE(StyleSheet{}["anything"] == TextStyle{});
}

TEST(StyleSheet, SetReplacesInPlaceAndEqualityIsExactAndOrdered) {
  const Type small{.size = 9.0f};
  const Type large{.size = 24.0f};

  StyleSheet a;
  a.set("head", small).set("body", large);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a.entries()[0].first, "head") << "entries keep insertion order";

  // Re-setting a registered name replaces it where it already sits.
  a.set("head", large);
  EXPECT_EQ(a.size(), 2u);
  EXPECT_EQ(a.entries()[0].first, "head");
  EXPECT_FLOAT_EQ(a["head"].shaping.fontSize, 24.0f);

  // Equality is what lets a StyleSheet ride inside a larger comparable
  // value: same base, same entries, same order.
  StyleSheet b;
  b.set("head", large).set("body", large);
  EXPECT_TRUE(a == b);
  TextStyle other;
  other.shaping.fontSize = 9.0f;
  b.base(other);
  EXPECT_FALSE(a == b) << "the base participates in equality";

  StyleSheet reordered;
  reordered.set("body", large).set("head", large);
  EXPECT_FALSE(a == reordered) << "equality is order-sensitive";

  StyleSheet extra = a;
  extra.set("note", small);
  EXPECT_FALSE(a == extra);
}

// A pass names its material by pointer: two passes sharing one instance are
// one pass, and a pass with a material is not the pass without it, so a
// restyle that attaches a material is seen by the draw-time comparison.
TEST(PaintStyle, PaintLayerMaterialComparesByIdentity) {
  // A pass never dereferences the material it names, and this feature does
  // not link the library that defines one, so an address that owns nothing
  // is all the comparison needs to be shown.
  static char address = 0;
  const auto named = std::shared_ptr<const sigil::material::Material>(
      std::shared_ptr<void>{}, static_cast<const sigil::material::Material*>(
                                   static_cast<const void*>(&address)));
  PaintLayer plain(SK_ColorRED);
  PaintLayer withMaterial(SK_ColorRED);
  EXPECT_EQ(plain, withMaterial);
  withMaterial.material = named;
  EXPECT_NE(plain, withMaterial);
  PaintLayer same = withMaterial;
  EXPECT_EQ(same, withMaterial);
}

// ---------------------------------------------------------------------------
// Type — the PARTIAL a call site names a style's numbers in, the merges that
// resolve one, and the TextStyle a total builds.

TEST(Type, TheAggregatesNumbersLandOnTheStylesTwoHalves) {
  const TextStyle s = textStyle({.size = 10.5f,
                                 .color = SkColor4f{1, 0, 0, 1},
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
  const TextStyle s = textStyle(t);
  ASSERT_EQ(s.shaping.variations.size(), 3u);
  EXPECT_EQ(s.shaping.variations[0], FontVariation("wght", 700));
  EXPECT_EQ(s.shaping.variations[1], FontVariation("slnt", -8));
  EXPECT_EQ(s.shaping.variations[2], FontVariation("wdth", 75));
}

TEST(Type, TheEightBitLadderQuantisesWhereTheFloatOneDoesNot) {
  const SkColor4f c{0.4f, 0.4f, 0.4f, 1};
  EXPECT_EQ(textStyle({.color = c}).paint.foreground.getColor4f(), c);
  EXPECT_NE(
      textStyle({.color = c, .color8 = true}).paint.foreground.getColor4f(), c);
}

TEST(Type, TextStyleOfATotalSpecIsTheStyleThatSpecHasAlwaysNamed) {
  // A spec that states every field lands exactly where it says, field by
  // field on the style's two halves. Optional fields are a way of saying
  // LESS, never a way of meaning something else: a call site that states a
  // style in full gets that style and no resolution happens to it.
  const TextStyle s = textStyle({.size = 13.0f,
                                 .color = SkColor4f{0.1f, 0.2f, 0.3f, 1},
                                 .track = 1.25f,
                                 .condense = 0.92f,
                                 .weight = 650.0f,
                                 .slant = -9.0f,
                                 .aliased = true,
                                 .antiAlias = false,
                                 .variations = {FontVariation("wdth", 75.0f)}});
  TextStyle expected;
  expected.shaping.fontSize = 13.0f;
  expected.shaping.letterSpacing = 1.25f;
  expected.shaping.scaleX = 0.92f;
  expected.shaping.aliased = true;
  expected.paint.foreground.setColor4f({0.1f, 0.2f, 0.3f, 1}, nullptr);
  expected.paint.foreground.setAntiAlias(false);
  expected.weight(650.0f).variation("slnt", -9.0f).variation("wdth", 75.0f);
  EXPECT_TRUE(s == expected);
}

TEST(Type, APartialOverlaysATotalFieldByField) {
  Type base = initialType();
  base.size = 20.0f;
  base.track = 2.0f;
  base.condense = 0.9f;
  base.color = SkColor4f{1, 1, 1, 1};

  const Type total =
      overlay(base, {.color = SkColor4f{1, 0, 0, 1}, .weight = 700.0f});
  EXPECT_EQ(total.color, (SkColor4f{1, 0, 0, 1})) << "a named field wins";
  EXPECT_EQ(total.weight, 700.0f);
  EXPECT_EQ(total.size, Length(20.0f)) << "a field it is silent about stands";
  EXPECT_EQ(total.track, 2.0f);
  EXPECT_EQ(total.condense, 0.9f);

  // The partial that names nothing overlays onto anything as itself.
  EXPECT_TRUE(Type{}.empty());
  EXPECT_FALSE(Type{.track = 0.0f}.empty())
      << "a field set to zero is a field that was set";
  EXPECT_TRUE(overlay(base, Type{}) == base);
}

TEST(Type, AnUnsetFieldInheritsRightThroughToTheStyle) {
  const sk_sp<SkTypeface> none;
  const Type heading{.size = 32.0f, .weight = 800.0f};
  const TextStyle inherited =
      toTextStyle(overlay(overlay(initialType(), {.track = 3.0f}), heading));
  EXPECT_FLOAT_EQ(inherited.shaping.fontSize, 32.0f);
  EXPECT_FLOAT_EQ(inherited.shaping.letterSpacing, 3.0f)
      << "the tracking nobody restated came down the cascade";
  EXPECT_EQ(inherited.shaping.typeface, none) << "and so did the null face";
  ASSERT_EQ(inherited.shaping.variations.size(), 1u);
  EXPECT_EQ(inherited.shaping.variations[0], FontVariation("wght", 800));

  // An axis already present is replaced where it stands, so a cascade never
  // accumulates two settings of one axis and the order stays stable.
  const Type varied =
      overlay(Type{.variations = {FontVariation("wght", 300.0f),
                                  FontVariation("opsz", 12.0f)}},
              Type{.variations = {FontVariation("wght", 900.0f)}});
  ASSERT_EQ(varied.variations.size(), 2u);
  EXPECT_EQ(varied.variations[0], FontVariation("wght", 900));
  EXPECT_EQ(varied.variations[1], FontVariation("opsz", 12));
}

TEST(Type, ARelativeSizeResolvesAgainstTheBase) {
  const Type base{.size = 20.0f};
  EXPECT_EQ(overlay(base, {.size = em(0.5f)}).size, Length(10.0f));
  EXPECT_EQ(overlay(base, {.size = 1.5_em}).size, Length(30.0f))
      << "the suffix and the function spell one length";
  EXPECT_EQ(overlay(base, {.size = 2_rem}, 10.0f).size, Length(20.0f))
      << "rem is the root's size, not the base's";
  EXPECT_EQ(overlay(base, {.size = 1_lh}, 16.0f, 28.0f).size, Length(28.0f));
  // No line height known: a single-spaced line is taken as 1.2 times the
  // size it is set in, so lh stays a number rather than becoming zero.
  EXPECT_FLOAT_EQ(overlay(base, {.size = 1_lh}).size->value, 24.0f);
  // A base that states no size leaves the initial 16 px to multiply.
  EXPECT_EQ(overlay(Type{}, {.size = 2_em}).size, Length(32.0f));
  // And the resolved size is a plain number: px, resolved once, never a
  // multiple carried forward to be applied twice.
  EXPECT_FALSE(overlay(base, {.size = 0.5_em}).size->relative());

  // The TextStyle form resolves against the size the style already carries.
  TextStyle built;
  built.shaping.fontSize = 24.0f;
  EXPECT_FLOAT_EQ(overlay(built, {.size = em(0.25f)}).shaping.fontSize, 6.0f);
}

TEST(Type, MergeCopiesFieldsAndLeavesARelativeSizeRelative) {
  // Two partials written about one element fold into one partial, which is
  // not yet resolved against anything: a relative size stays relative
  // because neither partial knows what it is relative to.
  Type folded{.size = 2_em,
              .track = 1.0f,
              .variations = {FontVariation("wght", 300.0f)}};
  merge(folded, {.color = SkColor4f{0, 1, 0, 1},
                 .track = 4.0f,
                 .variations = {FontVariation("wght", 700.0f),
                                FontVariation("wdth", 80.0f)}});
  EXPECT_EQ(folded.size, 2_em) << "a relative size is copied, not resolved";
  EXPECT_EQ(folded.track, 4.0f) << "a field the second names wins";
  EXPECT_EQ(folded.color, (SkColor4f{0, 1, 0, 1}));
  ASSERT_EQ(folded.variations.size(), 2u);
  EXPECT_EQ(folded.variations[0], FontVariation("wght", 700))
      << "an axis already present is replaced where it stands";
  EXPECT_EQ(folded.variations[1], FontVariation("wdth", 80));

  // An absolute size is copied the same way, and a partial that states
  // nothing changes nothing.
  Type keep{.size = 11.0f};
  EXPECT_TRUE(merge(keep, Type{}) == Type{.size = 11.0f});
}
