// The pixel styles: the bevel pair's tones land on the edges it names and
// swap when it is sunken, the brackets stand off the box at the corners
// they were asked for, the tick rail's ladder walks one edge with every
// n-th mark long, and the scanlines are rows inside the outline. Every
// claim is about WHERE INK LANDS, so every case reads pixels back.

#include <sigilcompose/brush/PixelStyles.h>

#include "support/PaintTestSupport.h"

namespace {

Fill grey() { return Fill::color({0.5f, 0.5f, 0.5f, 1}); }

/** Red, green and blue of a read-back pixel, for a claim about a tone. */
struct Rgb {
  int r, g, b;
};
Rgb rgb(SkColor c) {
  return {(int)SkColorGetR(c), (int)SkColorGetG(c), (int)SkColorGetB(c)};
}

}  // namespace

TEST(ComposePixelStyles, TheBevelPairLightsTheNearEdgesAndShadesTheFar) {
  Host host(100, 100);
  const styles::BevelPair raised =
      styles::bevelPair(SkColor4f{1, 1, 1, 1}, SkColor4f{0, 0, 0, 1}, 2);
  host.composer.render(box().child(
      box().left(10).top(10).width(40).height(30).fill(grey()).overlay(raised)));
  host.frame();
  // Two pixels in from each edge: light top and left, dark bottom and
  // right, and the grey face between them.
  EXPECT_EQ(host.pixel(11, 11), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(11, 25), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(30, 11), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(48, 25), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(30, 38), SK_ColorBLACK);
  EXPECT_NEAR(rgb(host.pixel(30, 25)).r, 128, 1);
  // The corner step: the top edge owns the top-right corner and the
  // bottom edge the bottom-left, as a raised panel's always have.
  EXPECT_EQ(host.pixel(48, 11), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(11, 38), SK_ColorBLACK);

  // Sunken is the same value the other way up.
  const styles::BevelPair sunken = raised.inverted();
  EXPECT_TRUE(sunken.sunken);
  EXPECT_FALSE(sunken == raised);
  EXPECT_EQ(sunken.inverted(), raised);
  host.composer.render(box().child(
      box().left(10).top(10).width(40).height(30).fill(grey()).overlay(sunken)));
  host.frame();
  EXPECT_EQ(host.pixel(11, 11), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(48, 25), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(30, 38), SK_ColorWHITE);
}

TEST(ComposePixelStyles, TheDerivedPairLightensAndDropsTheFace) {
  const SkColor4f face{0.4f, 0.5f, 0.6f, 1};
  const styles::BevelPair pair = styles::bevelPair(face, 0.2f, 0.5f, 3.0f);
  EXPECT_EQ(pair.light, lighten(face, 0.2f));
  EXPECT_EQ(pair.dark, scaleRgb(face, 0.5f));
  EXPECT_FLOAT_EQ(pair.lightWidth, 3.0f);
  EXPECT_FLOAT_EQ(pair.darkWidth, 3.0f);
  EXPECT_FLOAT_EQ(pair.reach(), 3.0f);
  // The same recipe twice is one value: a panel wearing it prunes.
  EXPECT_EQ(pair, styles::bevelPair(face, 0.2f, 0.5f, 3.0f));
}

TEST(ComposePixelStyles, BracketsStandOffTheBoxAtTheCornersAsked) {
  Host host(120, 80);
  host.composer.render(box().child(
      box()
          .width(100)
          .height(60)
          .fill(Fill::color({0, 0, 0, 1}))
          .foreground(styles::brackets(
              {1, 0, 0, 1}, 10, 2, 4,
              geometry::shapes::Corner::TopLeft |
                  geometry::shapes::Corner::BottomRight))));
  host.frame();
  // Top-left: a 2 px L whose outer edge stands 4 px in, arms 10 px long.
  EXPECT_EQ(host.pixel(4, 10), SK_ColorRED);
  EXPECT_EQ(host.pixel(10, 4), SK_ColorRED);
  EXPECT_EQ(host.pixel(10, 10), SK_ColorBLACK);  // inside the L
  EXPECT_EQ(host.pixel(30, 4), SK_ColorBLACK);   // past the arm
  EXPECT_EQ(host.pixel(2, 10), SK_ColorBLACK);   // in the gap
  // Bottom-right, mirrored.
  EXPECT_EQ(host.pixel(95, 50), SK_ColorRED);
  EXPECT_EQ(host.pixel(88, 55), SK_ColorRED);
  // The two corners not asked for stay bare.
  EXPECT_EQ(host.pixel(95, 4), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(4, 55), SK_ColorBLACK);
}

TEST(ComposePixelStyles, TheTickRailWalksOneEdgeWithEveryNthMarkLong) {
  Host host(120, 40);
  host.composer.render(box().child(
      box()
          .width(100)
          .height(20)
          .fill(Fill::color({0, 0, 0, 1}))
          .foreground(styles::tickRail({1, 0, 0, 1}, 10, 3, 6, 4))));
  host.frame();
  // Marks at 5, 15, 25, …: the first and every fourth are 6 px, the rest
  // 3 px, each one pixel wide.
  EXPECT_EQ(host.pixel(5, 1), SK_ColorRED);
  EXPECT_EQ(host.pixel(5, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 1), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 5), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(45, 5), SK_ColorRED);
  EXPECT_EQ(host.pixel(10, 1), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(6, 1), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(5, 18), SK_ColorBLACK);  // nothing on the far edge

  // The same rail hung off the bottom, from the far side.
  styles::TickRail bottom = styles::tickRail({1, 0, 0, 1}, 10, 3, 6, 4,
                                             geometry::path::Edge::Bottom);
  host.composer.render(box().child(box()
                                       .width(100)
                                       .height(20)
                                       .fill(Fill::color({0, 0, 0, 1}))
                                       .foreground(bottom)));
  host.frame();
  EXPECT_EQ(host.pixel(5, 18), SK_ColorRED);
  EXPECT_EQ(host.pixel(5, 14), SK_ColorRED);
  EXPECT_EQ(host.pixel(15, 14), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(5, 1), SK_ColorBLACK);
}

TEST(ComposePixelStyles, ScanlinesAreRowsInsideTheOutline) {
  Host host(40, 40);
  host.composer.render(box().child(
      box()
          .width(20)
          .height(20)
          .fill(Fill::color({1, 1, 1, 1}))
          .overlay(styles::scanlines({0, 0, 0, 1}, 4, 2))));
  host.frame();
  EXPECT_EQ(host.pixel(10, 0), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(10, 1), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(10, 2), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(10, 3), SK_ColorWHITE);
  EXPECT_EQ(host.pixel(10, 4), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(10, 17), SK_ColorBLACK);
  // Clipped to the shape: nothing lands outside the box.
  EXPECT_EQ(host.pixel(25, 0), SK_ColorBLACK);  // the host's own ground
  EXPECT_EQ(host.pixel(25, 2), SK_ColorBLACK);

  // A phase slides the rows, and a plus blend adds a tint to the ground.
  styles::Scanlines shifted = styles::scanlines({1, 0, 0, 1}, 4, 2,
                                                SkBlendMode::kPlus);
  shifted.phase = 2.0f;
  host.composer.render(box().child(box()
                                       .width(20)
                                       .height(20)
                                       .fill(Fill::color({0, 0, 0, 1}))
                                       .overlay(shifted)));
  host.frame();
  EXPECT_EQ(host.pixel(10, 0), SK_ColorBLACK);
  EXPECT_EQ(host.pixel(10, 2), SK_ColorRED);
  EXPECT_EQ(host.pixel(10, 3), SK_ColorRED);
  EXPECT_EQ(host.pixel(10, 4), SK_ColorBLACK);
}
