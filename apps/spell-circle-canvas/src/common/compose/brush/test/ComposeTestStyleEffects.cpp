// The looks a style mark once drew, as a material's effects and layers:
// each pinned to the pixel the drawn mark put there, over an 80 px grey
// box standing 20 px in from the corner of a 120 px host.

#include "support/BrushTestSupport.h"

namespace {

const material::Color kGrey = {0.5f, 0.5f, 0.5f, 1};

/** The pixel at (@p x, @p y) of a host holding @p node as that box. */
SkColor pixelOf(Element node, int x, int y) {
  Host host(120, 120);
  host.composer.render(box().children(
      {std::move(node.absolute().left(20).top(20).width(80).height(80))}));
  host.frame();
  return host.pixel(x, y);
}

}  // namespace

TEST(ComposeStyleEffects, AnInnerShadowHugsTheEdgeItIsCastFrom) {
  // Cast downward, so the band lies along the TOP inner edge.
  EXPECT_EQ(pixelOf(box().fill(material::from(kGrey).effects(
                        material::Filter::shadow({0, 0, 0, 0.8f},
                                                 {.blur = 6,
                                                  .offset = {0, 4},
                                                  .inside = true}))),
                    60, 22),
            0xFF2A2A2Au);
}

TEST(ComposeStyleEffects, AnOuterGlowIsASpreadShadowWithNoOffset) {
  EXPECT_EQ(pixelOf(box().fill(material::from(kGrey).effects(
                        material::Filter::shadow({1, 1, 1, 0.8f},
                                                 {.blur = 8, .spread = 2}))),
                    60, 15),
            0xFF393939u);
}

TEST(ComposeStyleEffects, ABevelLightsOneEdgeAndShadesTheOther) {
  const Element bevelled =
      box().fill(material::from(kGrey).effects(material::Filter::bevel()));
  EXPECT_EQ(pixelOf(bevelled, 22, 60), 0xFFA3A3A3u);  // lit, upper left
  EXPECT_EQ(pixelOf(bevelled, 97, 60), 0xFF636363u);  // shaded
}
