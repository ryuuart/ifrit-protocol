/** @file
 * A picture presented where the device drew it: sampled by a canvas on
 * another queue — a window's, on Metal — with nothing read back, and the
 * same picture the readback draws, whichever recorder of the device's own
 * Graphite samples it.
 *
 * Every case SKIPS where there is no Vulkan runtime, no Metal device to
 * stand a second queue on, or a driver that cannot name the Metal
 * objects behind its own.
 */

#include <gtest/gtest.h>
#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Recording.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/kit/Solids.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>
#include <sigilworld/World.h>
#include <sigilworld/advanced/Skia.h>
#include <sigilworld/advanced/Stats.h>
#include <sigilworld/scene/Scene.h>

#include <memory>
#include <mutex>
#include <string>

#include "DeviceSeams.h"
#include "TestMaterial.h"

using namespace sigil;
using namespace sigil::world;
using namespace sigil::world::test;

namespace {

constexpr glm::ivec2 kExtent{160, 120};

/** A lit body over a plate, so the picture has more than one colour. */
Frame lit(const Runtime& runtime) {
  namespace gm = ::sigil::geometry::mesh;
  const Group root = group({directionalLight({-0.4f, -0.8f, -0.4f}).key("sun"),
                            mesh(gm::quad(300, 300))
                                .key("plate")
                                .at({0, -40, 0})
                                .rotateX(-90.0f)
                                .fill(slangPaint({0.15f, 0.16f, 0.2f, 1.0f})),
                            mesh(gm::superellipsoid({40, 40, 40}, 2.0f, 24, 16))
                                .key("body")
                                .fill(slangPaint({0.8f, 0.6f, 0.3f, 1.0f}))})
                         .key("set");
  Frame frame(root);
  frame.extent(kExtent)
      .defaultCamera(diligent::raisedEye())
      .runtime(runtime)
      .pass(geometryPass("colour").writes("colour").clear(SkColors::kBlack));
  return frame;
}

/** A WINDOW'S QUEUE: Graphite on the machine's own Metal device and a
 *  queue of its own, which is what a window drawing through Qt stands
 *  on — the same device the Vulkan driver runs over, and another queue. */
class PresentedPicture : public testing::Test {
 protected:
  void SetUp() override {
    on = diligent::onDevice();
    if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
    std::string error;
    metal = core::hardware::GpuDevice::createOwned(&error);
    if (!metal || metal->backend() != core::hardware::Backend::Metal)
      GTEST_SKIP() << "no Metal device: " << error;
    window = skia::GraphiteContext::create(*metal);
    if (!window) GTEST_SKIP() << "no Graphite on the Metal device";
  }

  /** @p scene's picture drawn onto a canvas of the window's queue as
   *  @p how says, and read back from THAT canvas once the window has
   *  submitted it. */
  SkBitmap shown(Scene& scene, world::Presentation how) {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y);
    sk_sp<SkSurface> surface =
        SkSurfaces::RenderTarget(window->recorder(), info);
    SkBitmap pixels;
    if (!surface) return pixels;
    surface->getCanvas()->clear(SK_ColorBLACK);
    draw(scene, *surface->getCanvas(), how);
    pixels.allocPixels(info);
    if (!skia::readbackPixels(*window, *surface, pixels.pixmap()))
      pixels.reset();
    // What a window does after every frame: finished submissions let go
    // of what they sampled, which is what frees a presented picture.
    window->context()->checkAsyncWorkCompletion();
    return pixels;
  }

  diligent::OnDevice on;
  std::unique_ptr<core::hardware::GpuDevice> metal;
  std::unique_ptr<skia::GraphiteContext> window;
};

}  // namespace

TEST_F(PresentedPicture, APictureSampledInPlaceIsThePictureReadBack) {
  motion::Engine engine;
  Scene scene(engine);
  engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
  scene.render(lit(on.runtime));

  const SkBitmap inPlace = shown(scene, world::Presentation::OnDevice);
  ASSERT_FALSE(inPlace.drawsNothing());
  if (scene.stats().presentedOnDevice == 0)
    GTEST_SKIP() << "this driver cannot hand its pictures to a Metal queue";
  EXPECT_EQ(scene.stats().presentedOnDevice, 1);
  // NOTHING CROSSED BACK: the picture was sampled where it was drawn.
  EXPECT_EQ(scene.stats().readbacks, 0);

  const SkBitmap readBack = shown(scene, world::Presentation::Readback);
  ASSERT_FALSE(readBack.drawsNothing());
  EXPECT_EQ(scene.stats().readbacks, 1);
  // The same eight bits the final stage wrote, by either road.
  EXPECT_LE(diligent::worstChannel(inPlace, readBack), 1);
  // …and a picture rather than a cleared canvas: the body is lit.
  const SkColor4f middle = inPlace.getColor4f(kExtent.x / 2, kExtent.y / 2);
  EXPECT_GT(middle.fR, 0.1f);
}

TEST_F(PresentedPicture, ALiveRunReadsNothingBack) {
  motion::Engine engine;
  Scene scene(engine);
  // MORE FRAMES THAN PICTURES MAY BE OUT AT ONCE: each is handed back
  // once the window's submission that sampled it has finished, so a run
  // keeps presenting in place rather than falling back to a readback.
  int presented = 0;
  for (int frame = 0; frame < 12; ++frame) {
    engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
    scene.render(lit(on.runtime));
    const SkBitmap pixels = shown(scene, world::Presentation::OnDevice);
    ASSERT_FALSE(pixels.drawsNothing());
    if (frame == 0 && scene.stats().presentedOnDevice == 0)
      GTEST_SKIP() << "this driver cannot hand its pictures to a Metal queue";
    EXPECT_EQ(scene.stats().readbacks, 0) << "frame " << frame;
    presented += (int)scene.stats().presentedOnDevice;
  }
  EXPECT_EQ(presented, 12);
}

TEST_F(PresentedPicture, ACaptureIsReadBack) {
  motion::Engine engine;
  Scene scene(engine);
  engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
  scene.render(lit(on.runtime));
  const SkBitmap pixels = shown(scene, world::Presentation::Readback);
  ASSERT_FALSE(pixels.drawsNothing());
  EXPECT_EQ(scene.stats().readbacks, 1);
  EXPECT_EQ(scene.stats().presentedOnDevice, 0);
}

TEST(PresentedOnQueue, AFrameOnTheDevicesOwnGraphiteIsSampledWhereItStands) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  skia::GraphiteContext* own = diligent::vulkanGraphite(*on.device);
  if (!own) GTEST_SKIP() << "the device carries no Graphite on its own queue";
  motion::Engine engine;
  Scene scene(engine);
  engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
  scene.render(lit(on.runtime));

  // A canvas of Graphite on the device's own queue, the queue the
  // picture was drawn on; its pixels are read once it is submitted,
  // under the lock every submission on that queue takes.
  const auto shown = [&](world::Presentation how) {
    const SkImageInfo info = SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y);
    sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(own->recorder(), info);
    SkBitmap pixels;
    if (!surface) return pixels;
    surface->getCanvas()->clear(SK_ColorBLACK);
    draw(scene, *surface->getCanvas(), how);
    pixels.allocPixels(info);
    geometry::device::Device::QueueLock lock(*on.device);
    if (!skia::readbackPixels(*own, *surface, pixels.pixmap())) pixels.reset();
    own->context()->checkAsyncWorkCompletion();
    return pixels;
  };
  const SkBitmap inPlace = shown(world::Presentation::OnDevice);
  ASSERT_FALSE(inPlace.drawsNothing());
  EXPECT_EQ(scene.stats().presentedOnDevice, 1);
  // NOTHING CROSSED BACK: the picture was sampled where it was drawn.
  EXPECT_EQ(scene.stats().readbacks, 0);

  const SkBitmap readBack = shown(world::Presentation::Readback);
  ASSERT_FALSE(readBack.drawsNothing());
  EXPECT_EQ(scene.stats().readbacks, 1);
  EXPECT_LE(diligent::worstChannel(inPlace, readBack), 1);
  const SkColor4f middle = inPlace.getColor4f(kExtent.x / 2, kExtent.y / 2);
  EXPECT_GT(middle.fR, 0.1f);
}

TEST(PresentedOnQueue, ASecondRecorderOfTheDevicesGraphiteSamplesInPlaceToo) {
  const auto on = diligent::onDevice();
  if (!on) GTEST_SKIP() << "no Vulkan device: " << on.error;
  skia::GraphiteContext* own = diligent::vulkanGraphite(*on.device);
  if (!own) GTEST_SKIP() << "the device carries no Graphite on its own queue";
  // A recorder made for another thread from Graphite on the device's own
  // queue: it records onto the same queue the picture was drawn on.
  std::unique_ptr<skgpu::graphite::Recorder> second = own->makeRecorder();
  ASSERT_NE(second, nullptr);
  motion::Engine engine;
  Scene scene(engine);
  engine.advance(engine.elapsed() + motion::Duration(1.0 / 60.0));
  scene.render(lit(on.runtime));

  const SkImageInfo info = SkImageInfo::MakeN32Premul(kExtent.x, kExtent.y);
  sk_sp<SkSurface> surface = SkSurfaces::RenderTarget(second.get(), info);
  ASSERT_NE(surface, nullptr);
  surface->getCanvas()->clear(SK_ColorBLACK);
  draw(scene, *surface->getCanvas(), world::Presentation::OnDevice);
  EXPECT_EQ(scene.stats().presentedOnDevice, 1);
  // NOTHING CROSSED BACK: the picture was sampled where it was drawn.
  EXPECT_EQ(scene.stats().readbacks, 0);

  // What the second recorder drew is inserted under the context's lock,
  // and its pixels read once it is submitted, under the lock every
  // submission on that queue takes.
  SkBitmap pixels;
  pixels.allocPixels(info);
  {
    geometry::device::Device::QueueLock lock(*on.device);
    std::unique_ptr<skgpu::graphite::Recording> recording = second->snap();
    ASSERT_NE(recording, nullptr);
    {
      const std::unique_lock<std::mutex> held = own->lockContext();
      skgpu::graphite::InsertRecordingInfo insert;
      insert.fRecording = recording.get();
      ASSERT_EQ(own->context()->insertRecording(insert),
                skgpu::graphite::InsertStatus::kSuccess);
    }
    ASSERT_TRUE(skia::readbackPixels(*own, *surface, pixels.pixmap()));
    own->context()->checkAsyncWorkCompletion();
  }
  const SkColor4f middle = pixels.getColor4f(kExtent.x / 2, kExtent.y / 2);
  EXPECT_GT(middle.fR, 0.1f);
}
