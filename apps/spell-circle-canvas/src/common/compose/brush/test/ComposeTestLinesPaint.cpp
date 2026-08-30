// The paint binary's share of ComposeTestLines.cpp: the suites whose subjects
// are paint-tier values, cut from that file so each test binary links only the
// target it exercises.

#include "support/PaintTestSupport.h"

TEST(ComposeStyles, PresetBundlesRenderAndPrune) {
  Host host(300, 120);
  auto tree = [] {
    return box()
        .row()
        .gap(20)
        .padding(20)
        .child(
            box().width(120).height(44).corners({22}).style(styles::aquaGel()))
        .child(box().width(120).height(44).corners({8}).style(
            styles::y2kChrome()));
  };
  host.composer.render(tree());
  host.frame();
  // Aqua pill — the gloss reads as: bright lens at the top, the
  // saturated dark band just under it, and the LIGHT-FROM-BELOW glow at
  // the bottom (both ends beat the midband).
  const SkColor aquaTop = host.pixel(80, 28);
  const SkColor aquaMid = host.pixel(80, 47);
  const SkColor aquaBottom = host.pixel(80, 60);
  EXPECT_NE(aquaTop, SK_ColorBLACK);
  EXPECT_NE(aquaMid, SK_ColorBLACK);
  auto lum = [](SkColor c) {
    return SkColorGetR(c) + SkColorGetG(c) + SkColorGetB(c);
  };
  EXPECT_GT(lum(aquaTop), lum(aquaMid));
  EXPECT_GT(lum(aquaBottom), lum(aquaMid));
  // Chrome bar: the ramp has a hard horizon, brighter above than below.
  const SkColor chromeTop = host.pixel(220, 28);
  const SkColor chromeMid = host.pixel(220, 42);
  EXPECT_GT(lum(chromeTop), lum(chromeMid) + 100);
  // Both bundles are value decorations: identical re-describe prunes.
  host.composer.render(tree());
  EXPECT_EQ(host.composer.stats().patchedNodes, 0u);
}

TEST(ComposePatterns, HalftoneRampSwellsDownward) {
  Host host(100, 100);
  host.composer.render(box().child(box().width(100).height(100).fill(
      patterns::halftoneRamp(10, 1.0f, 4.0f, {1, 1, 1, 1}))));
  host.frame();
  int top = 0, bottom = 0;
  for (int y = 0; y < 20; ++y)
    for (int x = 0; x < 100; x += 1) top += host.pixel(x, y) != SK_ColorBLACK;
  for (int y = 80; y < 100; ++y)
    for (int x = 0; x < 100; x += 1)
      bottom += host.pixel(x, y) != SK_ColorBLACK;
  EXPECT_GT(bottom, top * 2);  // dots swell toward the bottom
}

// A note for anyone tempted to check the closed-contour wrap seam by
// stitching the window with SkContourMeasure and asserting on the result:
// that tests Skia's getSegment, not the renderer. A total failure of
// spans::wrap would leave such a test green. Read pixels instead — see
// ComposeMask.ClosedContourWrapSeamIsOnePiece.

TEST(ComposePatterns, HalftoneRampBandRemaps) {
  // rampFrom/rampTo confine the swell: with the band pushed to the bottom
  // half, the top half stays at rMin everywhere.
  Host host(100, 100);
  host.composer.render(box().child(box().width(100).height(100).fill(
      patterns::halftoneRamp(10, 0.8f, 4.0f, {1, 1, 1, 1}, 0.0f, 0.5f, 1.0f))));
  host.frame();
  int band20 = 0, band45 = 0;
  for (int y = 10; y < 20; ++y)
    for (int x = 0; x < 100; ++x) band20 += host.pixel(x, y) != SK_ColorBLACK;
  for (int y = 38; y < 48; ++y)
    for (int x = 0; x < 100; ++x) band45 += host.pixel(x, y) != SK_ColorBLACK;
  // Both bands sit above the ramp start → same tiny dots, no swell yet.
  const int slack = band20 / 2 + 12;
  EXPECT_NEAR(band20, band45, slack);
  int bandBottom = 0;
  for (int y = 88; y < 98; ++y)
    for (int x = 0; x < 100; ++x)
      bandBottom += host.pixel(x, y) != SK_ColorBLACK;
  EXPECT_GT(bandBottom, band20 * 2);  // full swell at the bottom
}
