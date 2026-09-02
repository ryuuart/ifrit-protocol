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

TEST(ComposeStyles, AquaGelEdgesRunFromNoneToTheDeepCut) {
  // The two edges the gel's default softens, each read off a rack of
  // 90x44 pills at y 20..64 that differ in one option and nothing else.
  const styles::AquaGelOptions preset;
  auto pill = [](styles::AquaGelOptions opts) {
    return box().width(90).height(44).corners({22}).style(
        styles::aquaGel({0.118f, 0.561f, 1.0f, 1.0f}, opts));
  };
  auto withTopBand = [&](float v) {
    styles::AquaGelOptions o;
    o.topBand = v;
    return pill(o);
  };
  styles::AquaGelOptions noLens;
  noLens.lensAlphaTop = 0.0f;
  styles::AquaGelOptions lensToItsOutline;
  lensToItsOutline.lensFadeEnd = 1.0f;

  Host host(500, 90);
  host.composer.render(box()
                           .row()
                           .gap(8)
                           .padding(8, 20)
                           .child(withTopBand(0.0f))
                           .child(withTopBand(preset.topBand))
                           .child(withTopBand(1.0f))
                           .child(pill(noLens))
                           .child(pill(lensToItsOutline)));
  host.frame();
  auto lum = [&](int x, int y) {
    const SkColor c = host.pixel(x, y);
    return SkColorGetR(c) + SkColorGetG(c) + SkColorGetB(c);
  };
  const int kNoBand = 53, kBand = 151, kDeepBand = 249;  // pill centres
  const int kNoLens = 347, kLensToOutline = 445;

  // topBand is the recess under the top edge, and the default is a real
  // recess that is not the deep cut: at a fifth of the height, no band is
  // the lightest and the deep cut the darkest.
  EXPECT_GT(lum(kNoBand, 29), lum(kBand, 29));
  EXPECT_GT(lum(kBand, 29), lum(kDeepBand, 29));

  // lensFadeEnd is where the highlight has finished. Under the default it
  // is finished BEFORE the lens's own lower arc, so the last rows of the
  // lens box carry no light at all and the arc cannot draw an edge — the
  // pill with no lens matches there. At 1 the ramp runs to the arc and
  // there IS light for the arc to cut off.
  EXPECT_NEAR(lum(kBand, 41), lum(kNoLens, 41), 4);
  EXPECT_GT(lum(kLensToOutline, 41), lum(kNoLens, 41) + 12);
  // …and the row is inside the lens, not below it.
  EXPECT_GT(lum(kBand, 30), lum(kNoLens, 30) + 100);
}

TEST(ComposePatterns, HalftoneRampSwellsDownward) {
  Host host(100, 100);
  host.composer.render(
      box().child(box().width(100).height(100).fill(Material::recipe(
          material::field::halftoneRamp(10, 1.0f, 4.0f, {1, 1, 1, 1})))));
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
      Material::recipe(material::field::halftoneRamp(
          10, 0.8f, 4.0f, {1, 1, 1, 1}, 0.0f, 0.5f, 1.0f)))));
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
