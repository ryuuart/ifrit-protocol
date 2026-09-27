// The lighting a lit surface is shaded under: stated once on a parent and
// inherited, it turns the relief of a normal-mapped fill toward the light;
// a surface with none in force is painted flat; a bound light moves the
// relief frame by frame while the colours beneath are read once; and a
// line of type and a stroke whose material states a surface are lit too.

#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilmaterial/core/Lighting.h>
#include <sigilmaterial/texture/Image.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>

#include <chrono>
#include <functional>
#include <memory>

#include "support/CoreTestSupport.h"

namespace {

/** A normal map whose left half faces left and whose right half faces
 *  right, both tilted 45 degrees out of the page. */
sk_sp<SkImage> twoFacedNormals(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      *bitmap.getAddr32(x, y) =
          SkPreMultiplyARGB(255, x < width / 2 ? 37 : 218, 128, 218);
  bitmap.setImmutable();
  return bitmap.asImage();
}

material::Material relief(material::Material colours) {
  return material::from(std::move(colours))
      .surface({.roughness = 0.8f,
                .normal = material::image(twoFacedNormals(64, 32))});
}

float brightness(SkColor colour) {
  return (float)(SkColorGetR(colour) + SkColorGetG(colour) +
                 SkColorGetB(colour));
}

/** A picture that counts how often it is read: the colours beneath a lit
 *  surface, which a moving light must not read again. */
struct CountedPicture {
  std::shared_ptr<int> reads;
  sk_sp<SkImage> picture;
  sigil::media::Frame frameAt(std::chrono::duration<double>) const {
    ++*reads;
    sigil::media::Frame frame;
    frame.image = picture;
    return frame;
  }
  bool isRunning() const { return false; }
  SkISize size() const { return picture->dimensions(); }
  bool operator==(const CountedPicture& other) const {
    return reads == other.reads;
  }
};

sk_sp<SkImage> grey(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height, true);
  bitmap.eraseColor(SkColorSetRGB(150, 150, 150));
  bitmap.setImmutable();
  return bitmap.asImage();
}

}  // namespace

TEST(ComposeLighting, AnInheritedLightTurnsTheReliefOfANormalMappedFill) {
  const material::Material surface = relief(material::Color{0.6f, 0.6f, 0.6f, 1});
  const auto scene = [&](float direction) {
    return stack().width(200).height(200).lighting(
        material::studio({.direction = direction, .elevation = 30.0f}))
        .children({box().width(64).height(32).fill(surface)});
  };
  Host host;
  host.composer.render(scene(180.0f));
  host.frame();
  const float leftLitFromLeft = brightness(host.pixel(12, 16));
  const float rightLitFromLeft = brightness(host.pixel(52, 16));
  host.composer.render(scene(0.0f));
  host.frame();
  const float leftLitFromRight = brightness(host.pixel(12, 16));
  const float rightLitFromRight = brightness(host.pixel(52, 16));
  EXPECT_GT(leftLitFromLeft, rightLitFromLeft + 60)
      << "lit from the left, the half whose normal faces left is brighter";
  EXPECT_GT(rightLitFromRight, leftLitFromRight + 60)
      << "lit from the right, the half whose normal faces right is brighter";
}

TEST(ComposeLighting, ASurfaceWithNoLightingInForceIsPaintedFlat) {
  Host host;
  host.composer.render(stack().width(200).height(200).children(
      {box().width(64).height(32).fill(
          relief(material::Color{0.6f, 0.6f, 0.6f, 1}))}));
  host.frame();
  EXPECT_EQ(host.pixel(12, 16), host.pixel(52, 16));
  // `initial` ends an inherited lighting below where it was stated.
  host.composer.render(
      stack().width(200).height(200).lighting(material::studio()).children(
          {box().width(64).height(32).initial(Property::Lighting).fill(
              relief(material::Color{0.6f, 0.6f, 0.6f, 1}))}));
  host.frame();
  EXPECT_EQ(host.pixel(12, 16), host.pixel(52, 16));
}

TEST(ComposeLighting, ABoundLightMovesTheReliefAndReadsTheColoursOnce) {
  auto reads = std::make_shared<int>(0);
  const material::Material surface = relief(
      material::image(sigil::media::PixelSource(CountedPicture{reads, grey(64, 32)})));
  sigil::motion::Animatable<float> sun = sigil::motion::animatable(180.0f);
  Host host;
  host.composer.render(stack().width(200).height(200)
                           .lighting(material::studio(
                               {.direction = sun, .elevation = 30.0f}))
                           .children({box().width(64).height(32).fill(surface)}));
  host.frame();
  const int readsOnce = *reads;
  EXPECT_GT(readsOnce, 0);
  const float leftBefore = brightness(host.pixel(12, 16));
  const float rightBefore = brightness(host.pixel(52, 16));
  sun = 0.0f;  // no render: the bound light alone moves
  host.frame();
  const float leftAfter = brightness(host.pixel(12, 16));
  const float rightAfter = brightness(host.pixel(52, 16));
  EXPECT_GT(leftBefore, rightBefore + 60);
  EXPECT_GT(rightAfter, leftAfter + 60) << "the relief turned with the light";
  sun = 90.0f;
  host.frame();
  EXPECT_EQ(readsOnce, *reads)
      << "only the lighting pass re-ran; the colours beneath were not read "
         "again";
}

namespace {

/** How many sampled pixels differ between @p element lit from the left
 *  and lit from the right. */
int turnedBy(const std::function<Element()>& element) {
  Host host;
  const auto scene = [&](float direction) {
    return stack().width(200).height(200).lighting(
        material::studio({.direction = direction, .elevation = 30.0f}))
        .children({element()});
  };
  host.composer.render(scene(180.0f));
  host.frame();
  SkBitmap fromLeft;
  fromLeft.allocN32Pixels(200, 200);
  host.surface->readPixels(fromLeft.pixmap(), 0, 0);
  host.composer.render(scene(0.0f));
  host.frame();
  SkBitmap fromRight;
  fromRight.allocN32Pixels(200, 200);
  host.surface->readPixels(fromRight.pixmap(), 0, 0);
  int moved = 0;
  for (int y = 0; y < 200; y += 2)
    for (int x = 0; x < 200; x += 2)
      moved += fromLeft.getColor(x, y) != fromRight.getColor(x, y);
  return moved;
}

}  // namespace

TEST(ComposeLighting, AStrokeAndALineOfTypeAreLitToo) {
  const material::Material surface = relief(material::Color{0.6f, 0.6f, 0.6f, 1});
  EXPECT_GT(turnedBy([&] {
              return box().width(64).height(32).stroke(surface, {.width = 8});
            }),
            20)
      << "the stroke turned with the light";
  EXPECT_GT(turnedBy([&] {
              return stack().children(
                  {text(u8"MMMM").font({.size = 48}).ink(surface)});
            }),
            20)
      << "the glyphs turned with the light";
}
