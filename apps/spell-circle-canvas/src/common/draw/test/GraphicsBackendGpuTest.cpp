/** @file
 * Where a persistent buffer stands: on the device of the host that first
 * begins it, re-formed beside itself while it stands where its host draws;
 * following a device host from raster and from another thread's recorder,
 * its marks kept; on raster where no host makes a surface; and
 * formed again on no frame that changes nothing; and a trail faded on
 * the device reaching its ground.
 */

#include <gtest/gtest.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <include/utils/SkNoDrawCanvas.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigildraw/Graphics.h>
#include <sigildraw/Pen.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace {

namespace draw = sigil::draw;
using sigil::skia::GraphiteContext;

constexpr SkISize kExtent{64, 32};

/** The buffer's pixels, read through a device surface where it stands on
 *  one and directly where it does not. */
std::vector<uint32_t> pixelsOf(const draw::Graphics& buffer,
                               GraphiteContext* context) {
  const sk_sp<SkImage> image = buffer.image();
  EXPECT_TRUE(image);
  if (!image) return {};
  const SkImageInfo info = SkImageInfo::MakeN32Premul(image->dimensions());
  std::vector<uint32_t> pixels(size_t(info.width()) * info.height());
  const SkPixmap pixmap(info, pixels.data(), info.minRowBytes());
  if (!image->isTextureBacked()) {
    EXPECT_TRUE(image->readPixels(nullptr, pixmap, 0, 0));
    return pixels;
  }
  if (!context) {
    ADD_FAILURE() << "a device image needs its context to be read";
    return {};
  }
  auto surface = SkSurfaces::RenderTarget(context->recorder(), info);
  EXPECT_TRUE(surface);
  if (!surface) return {};
  SkPaint copy;
  copy.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawImage(
      image, 0, 0, SkSamplingOptions(SkFilterMode::kNearest), &copy);
  EXPECT_TRUE(sigil::skia::readbackPixels(*context, *surface, pixmap));
  return pixels;
}

/** One frame of @p buffer under @p host, laying @p mark if it has one. */
void frame(draw::Graphics& buffer, SkCanvas& host, const SkRect* mark) {
  draw::Pen pen;
  pen.begin(host, {.width = float(kExtent.width()),
                   .height = float(kExtent.height())});
  draw::Pen& inner = buffer.begin(pen);
  if (mark) {
    inner.noStroke();
    inner.fill(255, 0, 0);
    inner.rect(mark->x(), mark->y(), mark->width(), mark->height());
  }
  buffer.end();
  pen.end();
}

SkColor at(const std::vector<uint32_t>& pixels, int x, int y) {
  const uint32_t value = pixels[size_t(y) * kExtent.width() + x];
  const SkPixmap one(SkImageInfo::MakeN32Premul(1, 1), &value, 4);
  return one.getColor(0, 0);
}

}  // namespace

TEST(GraphicsBackendGpu, ABufferFormsOnTheDeviceOfTheHostThatFirstBeginsIt) {
  auto device = sigil::core::hardware::GpuDevice::createOwned().value_or(nullptr);
  if (!device) GTEST_SKIP() << "no GPU device";
  auto context = GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context";
  auto deviceHost = SkSurfaces::RenderTarget(
      context->recorder(), SkImageInfo::MakeN32Premul(kExtent));
  ASSERT_TRUE(deviceHost);

  draw::Graphics buffer(float(kExtent.width()), float(kExtent.height()));
  const SkRect left = SkRect::MakeXYWH(4, 4, 12, 12);
  frame(buffer, *deviceHost->getCanvas(), &left);
  EXPECT_EQ(buffer.extent(), kExtent);
  ASSERT_TRUE(buffer.image());
  EXPECT_TRUE(buffer.image()->isTextureBacked());

  // A frame that changes nothing forms nothing: the held image stands.
  const uint32_t held = buffer.image()->uniqueID();
  frame(buffer, *deviceHost->getCanvas(), nullptr);
  EXPECT_EQ(buffer.image()->uniqueID(), held);

  // A host that draws nothing, as a headless step is, keeps it on the
  // device and forms nothing; its marks land there.
  SkNoDrawCanvas stepping(kExtent.width(), kExtent.height());
  const SkRect right = SkRect::MakeXYWH(40, 4, 12, 12);
  frame(buffer, stepping, &right);
  EXPECT_TRUE(buffer.image()->isTextureBacked());
  const auto marked = pixelsOf(buffer, context.get());
  ASSERT_FALSE(marked.empty());
  EXPECT_EQ(at(marked, 8, 8), SK_ColorRED);
  EXPECT_EQ(at(marked, 44, 8), SK_ColorRED);
  EXPECT_EQ(at(marked, 30, 24), SK_ColorTRANSPARENT);

  // A finer density under that host re-forms it beside itself, on the
  // device, with every mark scaled along.
  buffer.setDensityFloor(2);
  frame(buffer, stepping, nullptr);
  EXPECT_EQ(buffer.extent(), SkISize::Make(128, 64));
  EXPECT_TRUE(buffer.image()->isTextureBacked());
  const sk_sp<SkImage> finer = buffer.image();
  const SkImageInfo info = SkImageInfo::MakeN32Premul(finer->dimensions());
  std::vector<uint32_t> pixels(size_t(info.width()) * info.height());
  auto surface = SkSurfaces::RenderTarget(context->recorder(), info);
  ASSERT_TRUE(surface);
  SkPaint copy;
  copy.setBlendMode(SkBlendMode::kSrc);
  surface->getCanvas()->drawImage(finer, 0, 0, SkSamplingOptions(), &copy);
  ASSERT_TRUE(sigil::skia::readbackPixels(
      *context, *surface, SkPixmap(info, pixels.data(), info.minRowBytes())));
  const auto finerAt = [&](int x, int y) {
    const SkPixmap one(SkImageInfo::MakeN32Premul(1, 1),
                       &pixels[size_t(y) * info.width() + x], 4);
    return one.getColor(0, 0);
  };
  EXPECT_EQ(finerAt(16, 16), SK_ColorRED);
  EXPECT_EQ(finerAt(88, 16), SK_ColorRED);
}

TEST(GraphicsBackendGpu, ABufferFirstBegunWithoutASurfaceFollowsTheDeviceNext) {
  auto device = sigil::core::hardware::GpuDevice::createOwned().value_or(nullptr);
  if (!device) GTEST_SKIP() << "no GPU device";
  auto context = GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context";
  auto deviceHost = SkSurfaces::RenderTarget(
      context->recorder(), SkImageInfo::MakeN32Premul(kExtent));
  ASSERT_TRUE(deviceHost);

  // First begun by a host that draws nothing and makes no surface.
  SkNoDrawCanvas stepping(kExtent.width(), kExtent.height());
  draw::Graphics buffer(float(kExtent.width()), float(kExtent.height()));
  const SkRect left = SkRect::MakeXYWH(4, 4, 12, 12);
  frame(buffer, stepping, &left);
  ASSERT_TRUE(buffer.image());
  EXPECT_FALSE(buffer.image()->isTextureBacked());

  // A device beginning it later forms it again on the device, with what
  // was drawn on raster carried over.
  const SkRect right = SkRect::MakeXYWH(40, 4, 12, 12);
  frame(buffer, *deviceHost->getCanvas(), &right);
  EXPECT_EQ(buffer.extent(), kExtent);
  EXPECT_TRUE(buffer.image()->isTextureBacked());
  const auto marked = pixelsOf(buffer, context.get());
  ASSERT_FALSE(marked.empty());
  EXPECT_EQ(at(marked, 8, 8), SK_ColorRED);
  EXPECT_EQ(at(marked, 44, 8), SK_ColorRED);
}

TEST(GraphicsBackendGpu, ABufferDrawnOnAnotherThreadFollowsTheThreadDrawingItNext) {
  // A device's recorder belongs to the thread that made it. A buffer drawn
  // through one thread's recorder and begun next on another's is formed
  // again through the second, on the same device, with its marks kept.
  auto device = sigil::core::hardware::GpuDevice::createOwned().value_or(nullptr);
  if (!device) GTEST_SKIP() << "no GPU device";
  auto context = GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context";
  draw::Graphics buffer(float(kExtent.width()), float(kExtent.height()));
  std::unique_ptr<skgpu::graphite::Recorder> elsewhere;
  sk_sp<SkSurface> elsewhereHost;
  const SkRect left = SkRect::MakeXYWH(4, 4, 12, 12);
  std::thread([&] {
    elsewhere = context->makeRecorder();
    ASSERT_TRUE(elsewhere);
    elsewhereHost = SkSurfaces::RenderTarget(
        elsewhere.get(), SkImageInfo::MakeN32Premul(kExtent));
    ASSERT_TRUE(elsewhereHost);
    frame(buffer, *elsewhereHost->getCanvas(), &left);
    (void)context->submit(*elsewhere, skgpu::graphite::SyncToCpu::kYes);
  }).join();
  ASSERT_TRUE(buffer.image());

  auto deviceHost = SkSurfaces::RenderTarget(
      context->recorder(), SkImageInfo::MakeN32Premul(kExtent));
  ASSERT_TRUE(deviceHost);
  const SkRect right = SkRect::MakeXYWH(40, 4, 12, 12);
  frame(buffer, *deviceHost->getCanvas(), &right);
  EXPECT_TRUE(buffer.image()->isTextureBacked());
  const auto marked = pixelsOf(buffer, context.get());
  ASSERT_FALSE(marked.empty());
  EXPECT_EQ(at(marked, 8, 8), SK_ColorRED)
      << "the mark drawn on the other thread was not carried over";
  EXPECT_EQ(at(marked, 44, 8), SK_ColorRED);
  // A buffer standing where its host draws is not formed again.
  const uint32_t held = buffer.image()->uniqueID();
  frame(buffer, *deviceHost->getCanvas(), nullptr);
  EXPECT_EQ(buffer.image()->uniqueID(), held);
}

TEST(GraphicsBackendGpu, ADeviceThatCannotFormTheBufferLeavesItOnRaster) {
  auto device = sigil::core::hardware::GpuDevice::createOwned().value_or(nullptr);
  if (!device) GTEST_SKIP() << "no GPU device";
  auto context = GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context";
  auto deviceHost = SkSurfaces::RenderTarget(
      context->recorder(), SkImageInfo::MakeN32Premul(kExtent));
  ASSERT_TRUE(deviceHost);
  // Wider than any device texture, one pixel high: the device refuses it
  // and raster takes it.
  draw::Graphics buffer(70000, 1);
  draw::Pen pen;
  pen.begin(*deviceHost->getCanvas(), {.width = 64, .height = 32});
  buffer.begin(pen);
  buffer.end();
  pen.end();
  ASSERT_TRUE(buffer.image());
  EXPECT_FALSE(buffer.image()->isTextureBacked());
  const uint32_t held = buffer.image()->uniqueID();
  pen.begin(*deviceHost->getCanvas(), {.width = 64, .height = 32});
  buffer.begin(pen);
  buffer.end();
  pen.end();
  EXPECT_EQ(buffer.image()->uniqueID(), held);
}

TEST(GraphicsBackendGpu, ATranslucentGroundFadesATrailToTheGroundOnTheDevice) {
  // A device blends in eight bits and rounds, so a plain fade stops a few
  // steps short of the ground; the pen's fade moves at least a step a
  // frame there as on raster.
  auto device = sigil::core::hardware::GpuDevice::createOwned().value_or(nullptr);
  if (!device) GTEST_SKIP() << "no GPU device";
  auto context = GraphiteContext::create(*device);
  if (!context) GTEST_SKIP() << "no Graphite context";
  auto deviceHost = SkSurfaces::RenderTarget(
      context->recorder(), SkImageInfo::MakeN32Premul(kExtent));
  ASSERT_TRUE(deviceHost);

  draw::Graphics buffer(float(kExtent.width()), float(kExtent.height()));
  const auto run = [&](auto&& program) {
    draw::Pen pen;
    pen.begin(*deviceHost->getCanvas(), {.width = float(kExtent.width()),
                                         .height = float(kExtent.height())});
    program(buffer.begin(pen));
    buffer.end();
    pen.end();
  };
  run([](draw::Pen& pen) {
    pen.background(20);
    pen.noStroke();
    pen.fill(255);
    pen.rect(8, 8, 24, 16);
  });
  for (int frame = 0; frame < 40; ++frame)
    run([](draw::Pen& pen) { pen.background(20, 30); });
  ASSERT_TRUE(buffer.image());
  ASSERT_TRUE(buffer.image()->isTextureBacked());
  const auto faded = pixelsOf(buffer, context.get());
  ASSERT_FALSE(faded.empty());
  int left = 0;
  for (int y = 0; y < kExtent.height(); ++y)
    for (int x = 0; x < kExtent.width(); ++x)
      if (at(faded, x, y) != SkColorSetRGB(20, 20, 20)) ++left;
  EXPECT_EQ(left, 0);
}
