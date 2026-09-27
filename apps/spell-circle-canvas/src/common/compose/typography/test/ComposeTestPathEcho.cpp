// A layer style's echo on a run laid along a path: the echo re-stamps the
// placed glyphs at its offset beneath the real pass, on the curve, exactly
// as it does for a run set on a straight line.

#include <include/core/SkBitmap.h>

#include <cmath>
#include <vector>

#include "support/TextTestSupport.h"

namespace {

constexpr int kField = 240;
constexpr float kType = 16.0f;
constexpr SkVector kOffset = {3, 3};

/** Pure green is the echo; the real pass is white and covers the echo
 *  wherever the two overlap. */
bool isEcho(SkColor colour) {
  return SkColorGetG(colour) > 150 && SkColorGetR(colour) < 90 &&
         SkColorGetB(colour) < 90;
}

bool isRealPass(SkColor colour) {
  return SkColorGetR(colour) > 150 && SkColorGetB(colour) > 150;
}

}  // namespace

TEST(ComposeTextPathEcho, AnEchoRidesThePathTheRunIsLaidAlong) {
  Host host(kField, kField);
  host.composer.render(box().children(
      {text(u8"ECHO ECHO ECHO ECHO ECHO", whiteStyle(kType))
           .width(kField)
           .height(kField)
           .absolute()
           .left(0)
           .top(0)
           .textOnPath({.path = geometry::shapes::circle(),
                        .align = TextPath::Align::Center})
           .ink(material::from(material::Color{1, 1, 1, 1})
                    .effects(material::Filter::shadow(
                        {0, 1, 0, 1}, {.offset = {kOffset.fX, kOffset.fY}})))}));
  host.frame();
  SkBitmap pixels;
  pixels.allocPixels(SkImageInfo::MakeN32Premul(kField, kField));
  host.surface->readPixels(pixels.pixmap(), 0, 0);

  std::vector<SkIPoint> echo, real;
  for (int y = 0; y < kField; ++y)
    for (int x = 0; x < kField; ++x) {
      const SkColor colour = pixels.getColor(x, y);
      if (isEcho(colour)) echo.push_back({x, y});
      if (isRealPass(colour)) real.push_back({x, y});
    }
  ASSERT_FALSE(echo.empty());
  ASSERT_FALSE(real.empty());

  // A straight run at the box's origin would stand in its top-left corner,
  // which the ring never reaches.
  for (const SkIPoint& at : echo)
    EXPECT_FALSE(at.x() < 20 && at.y() < 20)
        << "echo in the corner at " << at.x() << "," << at.y();

  // Every echo pixel is within the offset and one glyph of the real ink.
  const float reach = std::hypot(kOffset.x(), kOffset.y()) + kType;
  int stray = 0;
  for (const SkIPoint& at : echo) {
    bool near = false;
    for (const SkIPoint& ink : real)
      if (std::hypot((float)(at.x() - ink.x()), (float)(at.y() - ink.y())) <=
          reach) {
        near = true;
        break;
      }
    if (!near) ++stray;
  }
  EXPECT_EQ(stray, 0);
}
