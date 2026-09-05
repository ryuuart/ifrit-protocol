// Colour management as a paint-tier value: the view transform a composer
// carries, end to end from the bake to the host's eight-bit surface.

#include "support/PaintTestSupport.h"

TEST(ComposeColor, OcioViewTransformsOutputAndClears) {
  // An exponent transform darkens mid-grey (0.5^2.2 ~ 0.218); clearing the
  // view restores pass-through. Exercises bake, response row, the
  // lowering the host's eight-bit surface asks for, and saveLayer.
#ifndef SIGILMATERIAL_ENABLE_OCIO
  GTEST_SKIP() << "built without OpenColorIO, so a view transform is a "
                  "no-op and this proves nothing";
#else
  ASSERT_TRUE(sigil::material::ocio::available());
  Host host;
  host.composer.setView(sigil::material::ocio::exponent(2.2f));
  host.composer.render(box().child(
      box().width(60).height(60).fill(Fill::color({0.5f, 0.5f, 0.5f, 1}))));
  host.frame();
  const uint32_t dark = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(dark, 30u);
  EXPECT_LT(dark, 80u);
  host.composer.setView({});  // pass-through again
  host.frame();
  const uint32_t plain = SkColorGetR(host.pixel(30, 30));
  EXPECT_GT(plain, 118u);
  EXPECT_LT(plain, 138u);
#endif
}
