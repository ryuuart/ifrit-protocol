/** @file
 * The colour bridge: what crosses between SkColor4f and this library's
 * colour leaf, and what a palette of them becomes.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/skia/Color.h>

#include <vector>

using namespace sigil::material;

TEST(SkiaColor, ChannelsAndAlphaSurviveTheRoundTrip) {
  constexpr SkColor4f in{0.25f, 0.5f, 0.75f, 0.125f};
  constexpr Color mid = skia::toColor(in);
  EXPECT_FLOAT_EQ(mid.r, 0.25f);
  EXPECT_FLOAT_EQ(mid.g, 0.5f);
  EXPECT_FLOAT_EQ(mid.b, 0.75f);
  EXPECT_FLOAT_EQ(mid.a, 0.125f);
  constexpr SkColor4f back = skia::toSkColor(mid);
  EXPECT_EQ(back, in);
}

TEST(SkiaColor, AChannelAboveOneIsCarriedRatherThanClamped) {
  // Straight copy, no transfer function: a wide-gamut channel survives.
  const Color c = skia::toColor(SkColor4f{1.5f, -0.25f, 0, 1});
  EXPECT_FLOAT_EQ(c.r, 1.5f);
  EXPECT_FLOAT_EQ(c.g, -0.25f);
}

TEST(SkiaColor, APaletteConvertsInOrder) {
  const std::vector<SkColor4f> palette{{1, 0, 0, 1}, {0, 1, 0, 1}};
  const std::vector<Color> out = skia::toColors(palette);
  ASSERT_EQ(out.size(), 2u);
  EXPECT_EQ(out[0], (Color{1, 0, 0, 1}));
  EXPECT_EQ(out[1], (Color{0, 1, 0, 1}));
}
