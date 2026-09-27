// The ripple a node's layer is resampled through, and the halftone ramps.

#include "support/BrushTestSupport.h"

TEST(ComposePatterns, HalftoneRampSwellsDownward) {
  Host host(100, 100);
  host.composer.render(box().children(
      {box().width(100).height(100).fill(
          material::field::halftoneRamp(10, 1.0f, 4.0f, {1, 1, 1, 1}))}));
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
// stitching the window with a contour measure and asserting on the result:
// that tests the measure, not the renderer. A total failure of
// spans::wrap would leave such a test green. Read pixels instead — see
// ComposeMask.ClosedContourWrapSeamIsOnePiece.
TEST(ComposePatterns, HalftoneRampBandRemaps) {
  // rampFrom/rampTo confine the swell: with the band pushed to the bottom
  // half, the top half stays at rMin everywhere.
  Host host(100, 100);
  host.composer.render(box().children({box().width(100).height(100).fill(
      material::field::halftoneRamp(
          10, 0.8f, 4.0f, {1, 1, 1, 1}, 0.0f, 0.5f, 1.0f))}));
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

TEST(ComposeFields, RippleDisplacesTheLayer) {
  // A thin horizontal red bar warped by a strong ripple: pixels appear
  // off-axis where the flat version has none.
  auto bar = [](bool warped) {
    Element e = box()
                    .absolute()
                    .inset(96, 20)
                    .fill(Fill::color({1, 0, 0, 1}));
    if (warped) e.filter(material::Filter::of(material::field::ripple(10, 60)));
    return box().children({std::move(e)});
  };
  Host flat, warped;
  flat.composer.render(bar(false));
  warped.composer.render(bar(true));
  flat.frame();
  warped.frame();
  int off = 0, offFlat = 0;
  for (int x = 30; x < 170; x += 2)
    for (int dy : {-7, 7}) {
      off += warped.pixel(x, 100 + dy) != SK_ColorBLACK;
      offFlat += flat.pixel(x, 100 + dy) != SK_ColorBLACK;
    }
  EXPECT_GT(off, 15);
  EXPECT_EQ(offFlat, 0);
}
