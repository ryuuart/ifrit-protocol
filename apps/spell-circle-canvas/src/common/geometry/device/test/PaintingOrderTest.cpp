/** @file
 * The painting order the device keeps: Graphite paints out of order and
 * leans on the depth test, and every pass that continues a target keeps
 * what the pass before it drew, whoever ended that pass. The device
 * picture is judged against the CPU's.
 */

#include <gpu/graphite/Context.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/Recording.h>
#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilgeometry/device/Device.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/PaintOrder.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <memory>

#include "DeviceFixtures.h"
#include "GraphiteReadback.h"
#include "OnDevice.h"

using namespace sigil;
using namespace sigil::geometry::device::test;

// A scene whose painting order the device must keep: an opaque ground, a
// fill that blends with what is under it, and an opaque box described
// after both. Graphite paints out of order and leans on the depth test to
// reject what the original order buried; where the destination read costs
// the depth attachment, the blending fill lands over the box that was
// described after it and the box comes back with a hole in it.
// The claim here is the one a plate is judged on: the device picture is
// the CPU's, pixel for pixel.
#ifdef __APPLE__
// Vulkan on an Apple platform is MoltenVK, the driver that loses depth
// across the restart a destination read costs, so its context is fenced.
TEST(AdoptedGraphite, OnAppleTheVulkanContextIsFencedForItsDriver) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";
  EXPECT_TRUE(skia::PaintOrderCanvas::needed(*ctx));
}
#endif

TEST(AdoptedGraphite, PaintingOrderHoldsThroughADestinationRead) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto describe = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setColor(SK_ColorRED);
    c.drawRect(SkRect::MakeXYWH(0, 0, 100, 100), fill);
    fill.setColor(SK_ColorMAGENTA);
    c.drawRect(SkRect::MakeXYWH(20, 20, 60, 60), fill);
    // A blend the hardware cannot express, so the backend reads back what
    // it is blending with.
    SkPaint shade;
    shade.setColor(SkColorSetARGB(255, 128, 128, 128));
    shade.setBlendMode(SkBlendMode::kMultiply);
    c.drawRect(SkRect::MakeXYWH(0, 0, 100, 100), shade);
    fill.setColor(SK_ColorBLUE);
    c.drawRect(SkRect::MakeXYWH(40, 40, 20, 20), fill);
    // and the same again through a layer whose own paint is the blend
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

  const SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  ASSERT_NE(raster, nullptr);
  describe(*raster->getCanvas());
  SkBitmap cpu;
  cpu.allocPixels(info);
  ASSERT_TRUE(raster->readPixels(cpu, 0, 0));

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  skia::PaintOrderCanvas ordered(*ctx, target->getCanvas());
  describe(ordered);
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());

  // The blend and the layer are the two reads, so a backend that needs
  // the fence closes the recording exactly twice.
  EXPECT_EQ(ordered.fences(), skia::PaintOrderCanvas::needed(*ctx) ? 2 : 0);
  for (int y = 0; y < info.height(); ++y)
    for (int x = 0; x < info.width(); ++x)
      ASSERT_EQ(cpu.getColor(x, y), device.getColor(x, y)) << x << "," << y;
}

namespace {

/** A star centred on (@p centerX, @p centerY): concave, many-sided and,
 *  at any size, a shape Graphite hands to its multisample renderer rather
 *  than to an analytic one. */
SkPath star(float centerX, float centerY, float outer, float inner) {
  SkPathBuilder path;
  for (int point = 0; point < 10; ++point) {
    const float angle = (float)point * 3.14159265f / 5.0f - 1.5707963f;
    const float radius = (point % 2 == 0) ? outer : inner;
    const float x = centerX + radius * std::cos(angle);
    const float y = centerY + radius * std::sin(angle);
    if (point == 0)
      path.moveTo(x, y);
    else
      path.lineTo(x, y);
  }
  path.close();
  return path.detach();
}

/** The CPU's picture of a scene, the reference a device picture is
 *  judged against. */
SkBitmap rasterise(const SkImageInfo& info,
                   const std::function<void(SkCanvas&)>& describe) {
  SkBitmap pixels;
  sk_sp<SkSurface> raster = SkSurfaces::Raster(info);
  if (!raster) return pixels;
  describe(*raster->getCanvas());
  pixels.allocPixels(info);
  if (!raster->readPixels(pixels, 0, 0)) return {};
  return pixels;
}

/** Whether every pixel @p cpu painted one of @p flat came back from the
 *  device the same colour, and how many pixels that judged.
 *
 *  Antialiased edges are the one thing two rasterisers are allowed to
 *  disagree about, so the claim is made where neither is guessing. A flat
 *  colour is flat on both, but a pixel one of them still resolved through
 *  a coverage value can come back a code value or two off it; the claim is
 *  that the pixel is the colour it was painted, not that two rasterisers
 *  agree bit for bit, so a couple of code values are allowed and a lost
 *  pass — a whole colour away — is not. */
::testing::AssertionResult keepsFlatColours(const SkBitmap& cpu,
                                            const SkBitmap& device,
                                            std::initializer_list<SkColor> flat,
                                            int& judged) {
  const auto worstChannel = [](SkColor first, SkColor second) {
    const auto gap = [](uint32_t left, uint32_t right) {
      return (int)(left > right ? left - right : right - left);
    };
    return std::max({gap(SkColorGetR(first), SkColorGetR(second)),
                     gap(SkColorGetG(first), SkColorGetG(second)),
                     gap(SkColorGetB(first), SkColorGetB(second)),
                     gap(SkColorGetA(first), SkColorGetA(second))});
  };
  judged = 0;
  if (cpu.empty() || device.empty() || cpu.dimensions() != device.dimensions())
    return ::testing::AssertionFailure() << "no picture to compare";
  for (int y = 0; y < cpu.height(); ++y) {
    for (int x = 0; x < cpu.width(); ++x) {
      const SkColor want = cpu.getColor(x, y);
      bool isFlat = false;
      for (const SkColor colour : flat) isFlat = isFlat || colour == want;
      if (!isFlat) continue;
      ++judged;
      const SkColor got = device.getColor(x, y);
      if (worstChannel(want, got) > 4)
        return ::testing::AssertionFailure()
               << "pixel " << x << "," << y << " was painted " << std::hex
               << want << " and came back " << got;
    }
  }
  return ::testing::AssertionSuccess();
}

}  // namespace

// A pass the fence cut in two must keep what the pass before it drew.
// Graphite antialiases a large path by rendering it multisampled and
// resolving the samples onto the target, and a pass that begins where
// another ended reloads the multisample attachment from the target first.
// A backend that resolves the whole attachment whatever area the pass
// names keeps the ground and whatever the first pass drew on it only if
// that reload reaches the whole attachment too. The scene here is the
// smallest one that asks for both: a path big enough to take the
// multisample renderer on either side of a draw whose blend the hardware
// cannot express, which is what ends the pass between them.
//
// Antialiased edges are the one thing two rasterisers are allowed to
// disagree about, so the claim is made where neither is guessing: every
// pixel the CPU painted one of the scene's flat colours, the device
// painted the same colour.
TEST(AdoptedGraphite, AFencedPassKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto describe = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 200, 170, 70), fill);
    // The blend the hardware cannot express: over the white ground alone,
    // so what it leaves is a flat colour too.
    SkPaint shade;
    shade.setColor(SkColorSetARGB(255, 128, 128, 128));
    shade.setBlendMode(SkBlendMode::kMultiply);
    c.drawRect(SkRect::MakeXYWH(0, 0, 120, 60), shade);
    // Described after the read, so it belongs to the pass that begins
    // where the fence ended the one before.
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(300, 460, 130, 55), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(600, 600);
  const SkBitmap cpu = rasterise(info, describe);
  ASSERT_FALSE(cpu.empty());

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  skia::PaintOrderCanvas ordered(*ctx, target->getCanvas());
  describe(ordered);
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());
  EXPECT_EQ(ordered.fences(), skia::PaintOrderCanvas::needed(*ctx) ? 1 : 0);

  int judged = 0;
  EXPECT_TRUE(keepsFlatColours(cpu, device,
                               {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE,
                                SkColorSetARGB(255, 128, 128, 128)},
                               judged));
  // The whole ground, both stars and the multiplied corner: a scene this
  // size has no way to be judged on a handful of pixels.
  EXPECT_GT(judged, 300000);
}

// Every pass that continues a target keeps what the pass before it drew,
// whoever ended that pass. Here Graphite ends it itself, inside one
// recording, because a snapshot of the surface is taken while the surface
// is still being drawn; nothing in this repository sees that pass end, so
// the canvas is the surface's own and the pass continues the target. The
// pass that follows holds one small path, so the area its draws cover is a
// corner of the target and everything the first drawing left outside that
// corner has to come through the multisample attachment's reload.
TEST(AdoptedGraphite, ASnapshotTakenMidSceneKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto first = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 250, 200, 80), fill);
  };
  const auto second = [](SkCanvas& c) {
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(70, 530, 40, 16), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(600, 600);
  const SkBitmap cpu = rasterise(info, [&](SkCanvas& c) {
    first(c);
    second(c);
  });
  ASSERT_FALSE(cpu.empty());

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  first(*target->getCanvas());
  const sk_sp<SkImage> snapshot = target->makeImageSnapshot();
  ASSERT_NE(snapshot, nullptr);
  second(*target->getCanvas());
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());

  int judged = 0;
  EXPECT_TRUE(keepsFlatColours(
      cpu, device, {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE}, judged));
  EXPECT_GT(judged, 300000);
}

// The same promise across a recording a caller closes itself: the first
// drawing is snapped and inserted by the test rather than through this
// repository, and the recording after it begins with no clear, so the
// pass it opens continues the target and the pass continues the target.
TEST(AdoptedGraphite, ACallersOwnSnapKeepsWhatThePassBeforeItDrew) {
  SIGIL_ON_DEVICE_OR_SKIP(on);
  skia::GraphiteContext* ctx = vulkanGraphite(*on);
  if (!ctx) GTEST_SKIP() << "this Skia carries no Vulkan backend";

  const auto first = [](SkCanvas& c) {
    c.clear(SK_ColorWHITE);
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorRED);
    c.drawPath(star(300, 250, 200, 80), fill);
  };
  const auto second = [](SkCanvas& c) {
    SkPaint fill;
    fill.setAntiAlias(true);
    fill.setColor(SK_ColorBLUE);
    c.drawPath(star(70, 530, 40, 16), fill);
  };

  const SkImageInfo info = SkImageInfo::MakeN32Premul(600, 600);
  const SkBitmap cpu = rasterise(info, [&](SkCanvas& c) {
    first(c);
    second(c);
  });
  ASSERT_FALSE(cpu.empty());

  sk_sp<SkSurface> target = SkSurfaces::RenderTarget(ctx->recorder(), info);
  ASSERT_NE(target, nullptr);
  first(*target->getCanvas());
  {
    // A snapped recording must be inserted: the recorder replays in
    // order, and one dropped recording stops it for good.
    std::unique_ptr<skgpu::graphite::Recording> recording =
        ctx->recorder()->snap();
    ASSERT_NE(recording, nullptr);
    const auto lock = ctx->lockContext();
    skgpu::graphite::InsertRecordingInfo insert;
    insert.fRecording = recording.get();
    ASSERT_TRUE(ctx->context()->insertRecording(insert));
  }
  second(*target->getCanvas());
  const SkBitmap device = skia::test::readGraphiteSurface(*ctx, target.get());
  ASSERT_FALSE(device.empty());

  int judged = 0;
  EXPECT_TRUE(keepsFlatColours(
      cpu, device, {SK_ColorWHITE, SK_ColorRED, SK_ColorBLUE}, judged));
  EXPECT_GT(judged, 300000);
}
