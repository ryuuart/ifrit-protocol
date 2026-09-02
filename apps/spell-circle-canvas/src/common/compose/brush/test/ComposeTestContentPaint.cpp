// The paint binary's share of ComposeTestContent.cpp: the suites whose subjects
// are paint-tier values, cut from that file so each test binary links only the
// target it exercises.

#include <include/core/SkBBHFactory.h>
#include <include/core/SkFont.h>
#include <include/core/SkPictureRecorder.h>
#include <sigilcompose/core/Feed.h>

#include <numeric>

#include "support/PaintTestSupport.h"

#ifdef SIGILMATERIAL_ENABLE_OCIO
#include <sigilmaterial/ocio/Ocio.h>

TEST(ComposeColor, OcioViewTransformsOutputAndClears) {
  // The OCIO output stage end-to-end: an exponent transform baked to a LUT
  // darkens mid-gray (0.5^2.2 ≈ 0.218); clearing the view restores
  // pass-through. Exercises bake → SkImage LUT → SkSL trilinear → saveLayer.
  ASSERT_TRUE(sigil::material::ocio::available());
  Host host;
  host.composer.setView(sigil::material::ocio::exponent(2.2f));
  host.composer.render(box().child(
      box().width(60).height(60).fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))));
  host.frame();
  const uint32_t dark = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(dark, 30u);  // ≈ 56 (LUT-quantized)
  EXPECT_LT(dark, 80u);
  host.composer.setView({});  // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u);  // ≈ 128
  EXPECT_LT(plain, 138u);
}

#endif  // SIGILMATERIAL_ENABLE_OCIO
