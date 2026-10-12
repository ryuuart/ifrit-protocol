/** @file
 * A PAGE PAINTED ON METAL AND WORN BY A BODY ON THE DEVICE: Graphite on a
 * Metal queue paints a page into a texture both APIs name, the device's
 * queue waits on the GPU for the painting, and a lit card in a frame
 * wears the texture where it stands. Two ways for the texture to come to
 * be: the device makes it and Metal paints what the driver exports, or
 * Metal makes it and the device names what the driver imports. Either
 * frame is the frame the same page painted on the CPU and uploaded makes.
 *
 * And the painting order a page needs, held on Metal with a plain canvas:
 * a draw whose blend reads the destination neither buries what was
 * described after it nor loses what was drawn before it.
 *
 * Every case SKIPS where there is no Vulkan runtime, no Metal device to
 * stand a second queue on, or a driver that cannot hand objects across.
 * A Metal queue exists on Apple alone, so elsewhere the file holds none.
 */

#ifdef __APPLE__

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/surface/Surface.h>
#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/core/Frame.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/Readback.h>
#include <sigilskia/graphite/TextureImage.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/scene/Scene.h>
#include <src/gpu/graphite/Caps.h>
#include <src/gpu/graphite/ContextPriv.h>
#include <src/gpu/graphite/ResourceTypes.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include "DeviceSeams.h"

using namespace sigil;
using namespace sigil::world;

namespace {

namespace hardware = ::sigil::core::hardware;

/** A frame wide enough that the card wears the page larger than the page
 *  is: a magnified page reads level zero alone, so a page uploaded with a
 *  chain and a page wrapped without one are read alike. */
constexpr glm::ivec2 kExtent{320, 240};
/** The page's side, in pixels. */
constexpr int kPage = 64;

/** THE PAGE: a flat ground, an antialiased disc on it, a multiply over
 *  half of both — a blend the hardware cannot express, so the backend
 *  reads the destination — and an opaque square described after the
 *  multiply, which a read that lost the painting order would bury. */
void paintPage(SkCanvas& canvas) {
  canvas.clear(SkColorSetRGB(240, 220, 180));
  SkPaint disc;
  disc.setAntiAlias(true);
  disc.setColor(SkColorSetRGB(40, 80, 200));
  canvas.drawCircle(24, 32, 18, disc);
  SkPaint shade;
  shade.setColor(SkColorSetRGB(128, 128, 128));
  shade.setBlendMode(SkBlendMode::kMultiply);
  canvas.drawRect(SkRect::MakeXYWH(0, 0, 32, 64), shade);
  SkPaint square;
  square.setColor(SkColorSetRGB(30, 160, 60));
  canvas.drawRect(SkRect::MakeXYWH(40, 8, 16, 16), square);
}

/** The page painted on the CPU, as an image a texture uploads. */
sk_sp<SkImage> pageOnTheCpu() {
  sk_sp<SkSurface> raster =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kPage, kPage));
  paintPage(*raster->getCanvas());
  return raster->makeImageSnapshot();
}

/** A SOURCE WHOSE PIXELS STAND ON A DEVICE AND NOWHERE ELSE: its frame
 *  carries no image and no binding, so a frame that shows the page shows
 *  the texture it names and could not have uploaded anything. */
struct StandingSource {
  material::DeviceImage where;
  media::Frame frameAt(std::chrono::duration<double>) const {
    media::Frame frame;
    frame.device.kind = media::DeviceFrame::Kind::Texture;
    frame.device.device = where.device;
    frame.device.handle = where.handle;
    frame.device.format = where.format;
    frame.device.layout = where.layout;
    frame.device.width = where.width;
    frame.device.height = where.height;
    return frame;
  }
  bool isRunning() const { return false; }
  bool operator==(const StandingSource& other) const {
    return where == other.where;
  }
};

/** A white lit surface wearing @p map on a card, lit by one sun. */
Frame wearing(material::Texture map) {
  material::Material surface =
      material::surface::program({.baseColor = {1, 1, 1, 1}});
  surface.slot(material::surface::kBaseColorSlot, std::move(map));
  return diligent::card(surface, kExtent);
}

/** One frame rendered and read back, and what the frame counted. */
struct Photographed {
  SkBitmap pixels;
  SceneStats stats;
};

Photographed photograph(const Frame& frame, const Runtime& runtime) {
  motion::Engine engine;
  Scene scene(engine);
  Frame copy = frame;
  copy.runtime(runtime);
  engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
  scene.render(copy);
  Photographed out;
  out.pixels.allocPixels(SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y));
  SkCanvas canvas(out.pixels);
  canvas.clear(SK_ColorBLACK);
  draw(scene, canvas);
  out.stats = scene.stats();
  return out;
}

/** HOW A TEXTURE KEEPS ITS HAZARDS on Metal, read off the texture itself:
 *  1 untracked, 2 tracked. Graphite's Metal backend leaves the order
 *  between two passes over one texture to Metal's own tracking. */
unsigned long hazardTrackingOf(void* mtlTexture) {
  using Send = unsigned long (*)(void*, SEL);
  return reinterpret_cast<Send>(objc_msgSend)(
      mtlTexture, sel_registerName("hazardTrackingMode"));
}

/** The levels a Metal texture carries, read off the texture itself. */
unsigned long mipmapLevelsOf(void* mtlTexture) {
  using Send = unsigned long (*)(void*, SEL);
  return reinterpret_cast<Send>(objc_msgSend)(
      mtlTexture, sel_registerName("mipmapLevelCount"));
}

/** What a Graphite context does for a draw whose blend reads the
 *  destination, as its capabilities say. */
const char* destinationReadOf(const skia::GraphiteContext& context) {
  using skgpu::graphite::DstReadStrategy;
  switch (context.context()->priv().caps()->getDstReadStrategy()) {
    case DstReadStrategy::kNoneRequired:
      return "none required";
    case DstReadStrategy::kTextureCopy:
      return "texture copy";
    case DstReadStrategy::kTextureSample:
      return "texture sample";
    case DstReadStrategy::kReadFromInput:
      return "read from input attachment";
    case DstReadStrategy::kFramebufferFetch:
      return "framebuffer fetch";
  }
  return "unknown";
}

/** THE TWO QUEUES: the device's Vulkan one, which the frame is drawn on,
 *  and a Metal one on the device the driver runs over, with Graphite on
 *  it — which is what a host painting pages on Metal stands on. */
class PageAcrossQueues : public testing::Test {
 protected:
  void SetUp() override {
    on = diligent::onDevice();
    if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
    device = on.device->gpu();
    if (!device) GTEST_SKIP() << "the device was created but not adopted";
    std::string error;
    metal = hardware::GpuDevice::createOwned(&error);
    if (!metal || metal->backend() != hardware::Backend::Metal)
      GTEST_SKIP() << "no Metal device: " << error;
    painter = skia::GraphiteContext::create(*metal);
    if (!painter) GTEST_SKIP() << "no Graphite on the Metal device";
  }

  /** The frame the page painted on the CPU and uploaded makes. */
  Photographed uploaded() {
    return photograph(wearing(material::Texture(pageOnTheCpu())), on.runtime);
  }

  /** The texture @p image names on the device, as a map a body wears. */
  material::Texture standing(const hardware::NativeTexture& image) const {
    material::DeviceImage where;
    where.device = device;
    where.handle = image.vkImage;
    where.format = image.vkFormat;
    where.layout = image.vkLayout;
    where.width = image.width;
    where.height = image.height;
    return material::Texture(StandingSource{where});
  }

  /** THE CROSSING, IN ORDER: the device's queue is told to wait for the
   *  page before anything else is queued there, and a signal of its own
   *  is queued behind the wait — which must stand still, since nothing
   *  has painted yet. Then the page is painted on Metal through
   *  @p surface, the Metal queue raises @p painted behind it, and the
   *  wait lets the device's queue through. False when the driver cannot
   *  bring the event over. */
  bool paintAcross(skia::OffscreenSurface& surface,
                   hardware::FenceHandle painted) {
    // The Metal queue's fence, brought over to the device's queue once.
    const hardware::FenceHandle brought =
        device->importNative(metal->exportNative(painted));
    if (!brought) return false;
    const hardware::FenceHandle behind = device->createFence();
    // A fence's first signal raises it to one, so that is the value the
    // page will stand at.
    constexpr hardware::FenceValue kPainted = 1;
    hardware::FenceValue queued = hardware::kFenceInitialValue;
    {
      const geometry::device::Device::QueueLock lock(*on.device);
      device->waitGpu(brought, kPainted);
      queued = device->signal(behind);
    }
    EXPECT_EQ(device->waitCpu(behind, queued, std::chrono::milliseconds(50)),
              hardware::FenceWait::TimedOut)
        << "the device's queue ran before the page was painted";

    // A PLAIN CANVAS: the surface's own, with nothing standing between
    // the page and Graphite.
    paintPage(*surface.surface()->getCanvas());
    EXPECT_EQ(surface.submit(*metal, painted), kPainted);
    EXPECT_EQ(device->waitCpu(behind, queued), hardware::FenceWait::Reached);
    // The wait has run, so nothing waits on the semaphore any more.
    device->destroyFence(behind);
    device->destroyFence(brought);
    return true;
  }

  diligent::OnDevice on;
  hardware::GpuDevice* device = nullptr;
  std::unique_ptr<hardware::GpuDevice> metal;
  std::unique_ptr<skia::GraphiteContext> painter;
};

/** HOW FAR A FRAME WEARING THE PAGE MAY STAND from the frame wearing the
 *  CPU's page: the two rasterisers agree on every flat colour and differ
 *  along the disc's antialiased edge alone, which the card magnifies, so
 *  a few pixels are a few levels apart and none is a colour apart. */
constexpr int kEdgeLevels = 16;
constexpr int kPixelsPastTwoLevels = 400;

void expectTheUploadedFrame(const Photographed& worn,
                            const Photographed& reference) {
  const int worst = diligent::worstChannel(worn.pixels, reference.pixels);
  const int apart = diligent::pixelsApart(
      worn.pixels, reference.pixels, SkIRect::MakeWH(kExtent.x, kExtent.y), 2);
  std::printf(
      "[ probe    ] worst channel %d, pixels more than two levels "
      "apart %d of %d\n",
      worst, apart, kExtent.x * kExtent.y);
  EXPECT_LE(worst, kEdgeLevels);
  EXPECT_LE(apart, kPixelsPastTwoLevels);
  // …and a page rather than a cleared card: the square described after
  // the multiply stands green in the middle of the card's right half.
  const SkColor4f middle = worn.pixels.getColor4f(kExtent.x / 2, kExtent.y / 2);
  EXPECT_GT(middle.fR + middle.fG + middle.fB, 0.3f);
}

}  // namespace

TEST_F(PageAcrossQueues, APagePaintedOnMetalIsWornByABodyUnderAFence) {
  // THE DEVICE MAKES THE TEXTURE, as a texture scene does today, and
  // Metal paints the object the driver exports for it.
  hardware::TextureDescription description;
  description.width = description.height = kPage;
  description.format = hardware::TextureFormat::RGBA8Unorm;
  const hardware::TextureHandle page = device->createTexture(description);
  ASSERT_TRUE(device->isValid(page));
  void* texture = device->exportNative(page).mtlTexture;
  if (!texture) {
    device->destroy(page);
    GTEST_SKIP() << "this driver does not name the Metal objects behind its "
                    "own";
  }
  // ONE DEVICE UNDER BOTH QUEUES: the Metal context draws on the device
  // the exported texture stands on.
  ASSERT_TRUE(skia::drawsOnDeviceOf(*painter->recorder(), texture));
  std::printf(
      "[ probe    ] exported texture hazard tracking %lu "
      "(1 untracked, 2 tracked)\n",
      hazardTrackingOf(texture));
  EXPECT_EQ(hazardTrackingOf(texture), 2u);

  const hardware::FenceHandle painted = metal->createFence();
  {
    skia::OffscreenSurface surface(*painter, texture, kPage, kPage);
    ASSERT_NE(surface.surface(), nullptr);
    if (!paintAcross(surface, painted)) {
      metal->destroyFence(painted);
      device->destroy(page);
      GTEST_SKIP() << "this driver does not bring a Metal event over";
    }
  }

  const Photographed worn =
      photograph(wearing(standing(device->exportNative(page))), on.runtime);
  // ONE CROSSING BACK, and it is the photograph's own: the page was
  // sampled where it stands, since the source carries no image at all.
  EXPECT_EQ(worn.stats.readbacks, 1);
  expectTheUploadedFrame(worn, uploaded());

  metal->destroyFence(painted);
  device->destroy(page);
}

TEST_F(PageAcrossQueues, AMetalTextureImportedIntoTheDeviceIsWornAsItStands) {
  // METAL MAKES THE TEXTURE, with the whole chain a map wants, and the
  // device names it over the same memory.
  hardware::TextureDescription description;
  description.width = description.height = kPage;
  description.format = hardware::TextureFormat::RGBA8Unorm;
  description.mipLevels = hardware::mipLevelsFor(kPage, kPage);
  const hardware::TextureHandle made = metal->createTexture(description);
  ASSERT_TRUE(metal->isValid(made));
  void* texture = metal->exportNative(made).mtlTexture;
  ASSERT_NE(texture, nullptr);
  hardware::NativeTexture given;
  given.backend = hardware::Backend::Metal;
  given.mtlTexture = texture;
  const hardware::TextureHandle page = device->importNative(given);
  if (!page) {
    metal->destroy(made);
    GTEST_SKIP() << "this driver does not import a Metal texture";
  }
  std::printf(
      "[ probe    ] Metal-made texture hazard tracking %lu "
      "(1 untracked, 2 tracked)\n",
      hazardTrackingOf(texture));

  // THE CHAIN IS PRESENT ON BOTH SIDES, level for level. Graphite paints
  // level zero; a body wearing the page reads the residency's chain, whose
  // level zero is copied from this one on the device.
  const hardware::NativeTexture image = device->exportNative(page);
  EXPECT_EQ(image.mipLevels, description.mipLevels);
  EXPECT_EQ(mipmapLevelsOf(texture), (unsigned long)description.mipLevels);

  const hardware::FenceHandle painted = metal->createFence();
  {
    skia::OffscreenSurface surface(*painter, *metal, made);
    ASSERT_NE(surface.surface(), nullptr);
    if (!paintAcross(surface, painted)) {
      metal->destroyFence(painted);
      device->destroy(page);
      metal->destroy(made);
      GTEST_SKIP() << "this driver does not bring a Metal event over";
    }
  }

  const Photographed worn = photograph(wearing(standing(image)), on.runtime);
  EXPECT_EQ(worn.stats.readbacks, 1);
  expectTheUploadedFrame(worn, uploaded());

  metal->destroyFence(painted);
  device->destroy(page);
  for (uint64_t frame = 0; frame < hardware::GpuDevice::kFramesInFlight;
       ++frame)
    device->beginFrame();
  metal->destroy(made);
}

// THE TWO CLAIMS A FENCING CANVAS EXISTS FOR, on Metal with none: a draw
// whose blend reads the destination keeps the painting order — what was
// described after it is not buried — and a large path drawn after such a
// draw keeps what the pass before it drew. The scenes are the device
// test's own; here they are drawn through the surface's plain canvas.
TEST_F(PageAcrossQueues, PaintingOrderHoldsOnMetalWithNoFence) {
  std::printf("[ probe    ] destination reads: Metal %s",
              destinationReadOf(*painter));
  if (skia::GraphiteContext* own = on.device->graphite())
    std::printf(", the device's Vulkan %s", destinationReadOf(*own));
  std::printf("\n");

  const auto onBoth = [&](const SkImageInfo& info, const auto& describe,
                          SkBitmap* cpu, SkBitmap* device) {
    sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
    describe(*raster->getCanvas());
    cpu->allocPixels(info);
    ASSERT_TRUE(raster->readPixels(*cpu, 0, 0));
    sk_sp<SkSurface> target =
        SkSurfaces::RenderTarget(painter->recorder(), info);
    ASSERT_NE(target, nullptr);
    describe(*target->getCanvas());
    device->allocPixels(info);
    ASSERT_TRUE(skia::readbackPixels(*painter, *target, device->pixmap()));
    painter->context()->checkAsyncWorkCompletion();
  };

  // An opaque ground, a fill the multiply blends with, and an opaque box
  // described after both; then the same again through a layer whose own
  // paint is the multiply.
  const auto ordered = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setColor(SK_ColorRED);
    c.drawRect(SkRect::MakeXYWH(0, 0, 100, 100), fill);
    fill.setColor(SK_ColorMAGENTA);
    c.drawRect(SkRect::MakeXYWH(20, 20, 60, 60), fill);
    SkPaint shade;
    shade.setColor(SkColorSetARGB(255, 128, 128, 128));
    shade.setBlendMode(SkBlendMode::kMultiply);
    c.drawRect(SkRect::MakeXYWH(0, 0, 100, 100), shade);
    fill.setColor(SK_ColorBLUE);
    c.drawRect(SkRect::MakeXYWH(40, 40, 20, 20), fill);
    SkPaint layer;
    layer.setBlendMode(SkBlendMode::kMultiply);
    c.saveLayer(nullptr, &layer);
    SkPaint pale;
    pale.setColor(SkColorSetARGB(255, 200, 200, 255));
    c.drawRect(SkRect::MakeXYWH(0, 60, 100, 40), pale);
    c.restore();
    fill.setColor(SK_ColorGREEN);
    c.drawRect(SkRect::MakeXYWH(5, 70, 20, 20), fill);
  };
  SkBitmap cpu, gpu;
  onBoth(SkImageInfo::MakeN32Premul(100, 100), ordered, &cpu, &gpu);
  ASSERT_FALSE(gpu.drawsNothing());
  int differing = 0;
  for (int y = 0; y < 100; ++y)
    for (int x = 0; x < 100; ++x)
      if (cpu.getColor(x, y) != gpu.getColor(x, y)) ++differing;
  std::printf(
      "[ probe    ] painting order: %d of 10000 pixels differ from "
      "the CPU's\n",
      differing);
  EXPECT_EQ(differing, 0);

  // Two large stars — the shape Graphite gives its multisample renderer —
  // either side of a multiply over the white ground.
  const auto star = [](float cx, float cy, float outer, float inner) {
    SkPathBuilder path;
    for (int i = 0; i < 10; ++i) {
      const float angle = (float)i * 3.14159265f / 5.0f - 1.5707963f;
      const float r = (i % 2 == 0) ? outer : inner;
      const float x = cx + r * std::cos(angle);
      const float y = cy + r * std::sin(angle);
      if (i == 0)
        path.moveTo(x, y);
      else
        path.lineTo(x, y);
    }
    path.close();
    return path.detach();
  };
  const auto resumed = [&star](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 200, 170, 70), fill);
    SkPaint shade;
    shade.setColor(SkColorSetARGB(255, 128, 128, 128));
    shade.setBlendMode(SkBlendMode::kMultiply);
    c.drawRect(SkRect::MakeXYWH(0, 0, 120, 60), shade);
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(300, 460, 130, 55), fill);
  };
  onBoth(SkImageInfo::MakeN32Premul(600, 600), resumed, &cpu, &gpu);
  ASSERT_FALSE(gpu.drawsNothing());
  // Every pixel the CPU painted one of the scene's flat colours, Metal
  // painted the same colour, to within the code value or two a pixel
  // resolved through a coverage value may come back off it.
  const SkColor flat[] = {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE,
                          SkColorSetARGB(255, 128, 128, 128)};
  const auto gap = [](SkColor a, SkColor b) {
    const auto one = [](uint32_t l, uint32_t r) {
      return (int)(l > r ? l - r : r - l);
    };
    return std::max({one(SkColorGetR(a), SkColorGetR(b)),
                     one(SkColorGetG(a), SkColorGetG(b)),
                     one(SkColorGetB(a), SkColorGetB(b)),
                     one(SkColorGetA(a), SkColorGetA(b))});
  };
  int judged = 0, worst = 0;
  for (int y = 0; y < 600; ++y) {
    for (int x = 0; x < 600; ++x) {
      const SkColor want = cpu.getColor(x, y);
      if (std::find(std::begin(flat), std::end(flat), want) == std::end(flat))
        continue;
      ++judged;
      worst = std::max(worst, gap(want, gpu.getColor(x, y)));
    }
  }
  std::printf("[ probe    ] resumed pass: %d flat pixels judged, worst %d\n",
              judged, worst);
  EXPECT_GT(judged, 300000);
  EXPECT_LE(worst, 4);
}

#endif  // __APPLE__
