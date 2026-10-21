/** @file
 * A texture as a mesh samples it: where it is read, whether it repeats
 * and how it is filtered, answered from the size its source states — so
 * a page standing on a device is placed exactly as the same picture in
 * host memory is, and is never read back to be placed.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkImage.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmedia/core/PixelSource.h>
#include <sigilworld/advanced/View.h>

#include <chrono>
#include <glm/mat3x3.hpp>
#include <memory>

using namespace sigil;

namespace {

/** A BINDING THAT COUNTS how many times the frame was asked for as an
 *  image — what reading a page back to host memory goes through. */
class CountingBinding final : public media::DeviceBinding {
 public:
  explicit CountingBinding(int& asked) : m_asked(asked) {}
  sk_sp<SkImage> image(skgpu::graphite::Recorder*) override {
    ++m_asked;
    return nullptr;
  }

 private:
  int& m_asked;
};

/** A SOURCE WHOSE PIXELS STAND ON A DEVICE AND NOWHERE ELSE: its frame
 *  names a texture of the stated size and carries no image. */
struct StandingSource {
  std::shared_ptr<media::DeviceBinding> binding;
  int width = 0;
  int height = 0;
  media::Frame frameAt(std::chrono::duration<double>) const {
    media::Frame frame;
    frame.device.kind = media::DeviceFrame::Kind::Texture;
    frame.device.width = width;
    frame.device.height = height;
    frame.device.binding = binding;
    return frame;
  }
  bool isRunning() const { return false; }
  bool operator==(const StandingSource& other) const {
    return binding == other.binding && width == other.width &&
           height == other.height;
  }
};

/** A picture of @p width by @p height in host memory. */
media::Picture inHostMemory(int width, int height) {
  SkBitmap pixels;
  pixels.allocN32Pixels(width, height);
  pixels.eraseColor(SK_ColorWHITE);
  pixels.setImmutable();
  return media::fromSk(pixels.asImage());
}

/** One placement, a scale and an offset, that only lands the same on
 *  two pictures when both are taken at the same size. */
material::Texture placed(media::PixelSource source) {
  glm::mat3 uv(1.0f);
  uv[0][0] = 2.0f;
  uv[1][1] = 0.5f;
  uv[2][0] = 12.0f;
  uv[2][1] = 3.0f;
  material::Texture texture(std::move(source));
  texture.uv(uv).region({4, 2, 40, 20}).tile(material::Repeat::Repeat);
  return texture;
}

}  // namespace

TEST(WorldSampling, APageOnADeviceIsPlacedWithoutBeingReadBack) {
  int asked = 0;
  const material::Texture onDevice = placed(media::PixelSource(
      StandingSource{std::make_shared<CountingBinding>(asked), 64, 32}));
  const material::Texture inMemory =
      placed(media::PixelSource(inHostMemory(64, 32)));

  const world::Sampling page = world::samplingOf(onDevice);
  const world::Sampling picture = world::samplingOf(inMemory);

  EXPECT_EQ(asked, 0) << "the page was read back to learn where it is read";
  EXPECT_EQ(page.uv, picture.uv);
  EXPECT_EQ(page.tile, picture.tile);
  EXPECT_TRUE(page.tile);
  EXPECT_EQ(page.filter, picture.filter);
  // …and the placement is the region's, not the whole page's.
  EXPECT_NE(page.uv, glm::mat3(1.0f));
}

TEST(WorldSampling, ATextureWithNothingToSampleAnswersAnEmptySampling) {
  const world::Sampling none = world::samplingOf(material::Texture{});
  EXPECT_EQ(none.uv, glm::mat3(1.0f));
  EXPECT_FALSE(none.tile);
}
