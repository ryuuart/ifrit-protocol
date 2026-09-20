// kit/Ground.h — the two fills a ground is dressed with: the vignette
// that holds the middle and meets all four corners at one value, and the
// grain that breaks a flat tone without moving the neutral it is measured
// from.

#include <sigilcompose/kit/Ground.h>

#include "support/ShapeTestSupport.h"

namespace kit = sigil::compose::kit;

/** A vignette holds the middle at nothing and ramps to the edge colour at
 *  the CORNER, which is what makes it meet all four corners at the same
 *  value on a surface that is not square. */
TEST(KitGround, AVignetteShadesTheCornersAndHoldsTheMiddle) {
  Host host(300, 200);
  host.composer.render(box().absolute().inset(0).fill(green()).children(
      {box().absolute().inset(0).fill(
          kit::vignette({300, 200}, {0, 0, 0, 1}))}));
  host.frame();
  EXPECT_EQ(host.pixel(150, 100), SK_ColorGREEN);
  const SkColor topLeft = host.pixel(1, 1);
  const SkColor topRight = host.pixel(298, 1);
  EXPECT_LT(SkColorGetG(topLeft), SkColorGetG(SK_ColorGREEN));
  EXPECT_NEAR(SkColorGetG(topLeft), SkColorGetG(topRight), 2);
}

/** A grain at zero IS the colour — the neutral the strength is measured
 *  from — and a grain above zero moves pixels a flat fill leaves
 *  identical. Measured on a MID TONE: soft light holds black and white
 *  wherever it finds them, so a grain over either is correctly nothing. */
TEST(KitGround, AGrainIsNeutralAtZeroAndBrokenAboveIt) {
  const SkColor4f mid{0.5f, 0.5f, 0.5f, 1};
  Host plain(120, 40);
  plain.composer.render(box().absolute().inset(0).fill(Fill::color(mid)));
  plain.frame();
  const SkColor flatColor = plain.pixel(60, 20);

  Host none(120, 40);
  none.composer.render(box().absolute().inset(0).fill(kit::grained(mid, 0.0f)));
  none.frame();
  EXPECT_EQ(none.pixel(60, 20), flatColor);

  Host rough(120, 40);
  rough.composer.render(
      box().absolute().inset(0).fill(kit::grained(mid, 0.9f)));
  rough.frame();
  bool moved = false;
  const SkColor first = rough.pixel(0, 20);
  for (int x = 1; x < 120 && !moved; ++x) moved = rough.pixel(x, 20) != first;
  EXPECT_TRUE(moved);
}
