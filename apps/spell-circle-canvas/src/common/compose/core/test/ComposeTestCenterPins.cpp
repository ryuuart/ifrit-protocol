// A centre pin settles on Yoga's pixel grid and stays there when an
// unrelated slot changes, so the texture holding its artwork is reused.

#include "support/CoreTestSupport.h"

namespace {

class CenterPinCaching : public ::testing::TestWithParam<int> {};

Element pinnedArt(int kind) {
  if (kind == 0)
    return box()
        .key("pin")
        .width(50)
        .height(3)
        .centerAt({320, 315})
        .rotate(37)
        .fill(red());
  if (kind == 1)
    return box()
        .key("pin")
        .width(395.2f)
        .height(395.2f)
        .centerAt({740, 470})
        .fill(green());
  return box()
      .key("pin")
      .width(40.3f)
      .height(4.7f)
      .centerAt({300.2f, 300.4f})
      .fill(blue());
}

}  // namespace

TEST_P(CenterPinCaching, ASiblingSlotChangeKeepsThePinnedArtAndItsBake) {
  Host host(1100, 800);
  host.composer.setAutoTexturePromotion(false);
  host.composer.render(box().children({box()
                                           .key("art")
                                           .width(1000)
                                           .height(720)
                                           .cache(Cache::Texture)
                                           .children({pinnedArt(GetParam())}),
                                       slot("s").width(10).height(10)}));
  host.composer.renderSlot("s", box().fill(red()));
  host.frame();
  ASSERT_EQ(host.composer.stats().texturesBaked, 1u);
  const SkRect was = require(host.composer.bounds("pin"));
  for (int frame = 0; frame < 8; ++frame) {
    host.composer.renderSlot("s", box().fill(frame % 2 ? red() : green()));
    host.frame();
    EXPECT_EQ(require(host.composer.bounds("pin")), was);
    EXPECT_EQ(host.composer.stats().texturesBaked, 0u)
        << "slot update " << frame;
  }
}

INSTANTIATE_TEST_SUITE_P(ComposeCaching, CenterPinCaching,
                         ::testing::Values(0, 1, 2));
