/** @file
 * The process's own 2D painter, and the host-less device standing on
 * whichever context 2D drawing with no frame lands on.
 */

#include "sigilsketch/core/Painting.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Context.h>
#include <include/gpu/graphite/Recorder.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigildraw/Hostless.h>
#include <sigilsketch/plate/Graphite.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/Readback.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace sigil::sketch {

namespace {

/** The painter and the thread whose recorder it is. Never released: an
 *  image formed on it may be held by a value Python or a static keeps
 *  until the process ends, and the device must outlive every one. */
struct OwnPainter {
  std::unique_ptr<core::hardware::GpuDevice> device;
  std::unique_ptr<skia::GraphiteContext> graphite;
  std::thread::id thread;
};

std::atomic<bool>& stood() {
  static std::atomic<bool> flag{false};
  return flag;
}

OwnPainter& standUp() {
  static OwnPainter* painter = [] {
    auto* made = new OwnPainter;
    made->device = core::hardware::GpuDevice::createOwned().value_or(nullptr);
    if (made->device)
      made->graphite = skia::GraphiteContext::create(*made->device);
    made->thread = std::this_thread::get_id();
    stood().store(true, std::memory_order_release);
    return made;
  }();
  return *painter;
}

/** The context drawing with no frame lands on, or null for raster. */
skia::GraphiteContext* reachable() {
  if (skia::GraphiteContext* installed = deviceGraphite()) return installed;
  OwnPainter& own = standUp();
  return own.graphite.get();
}

/** A RECORDER OF THIS THREAD'S OWN ON @p context, made the first time the
 *  thread draws there with no frame open and kept: a context's own
 *  recorder belongs to the thread that made it, which is the one drawing
 *  a window's frames, and a recorder made for another thread is what a
 *  context hands out for exactly this. Recordings of either are inserted
 *  into the same context, so an image one paints the other draws as it
 *  stands. Never destroyed: a surface formed through it names it for as
 *  long as a layer holds the surface. */
skgpu::graphite::Recorder* recorderOn(skia::GraphiteContext& context) {
  struct Held {
    skgpu::graphite::Context::ContextID context;
    skgpu::graphite::Recorder* recorder = nullptr;
  };
  thread_local std::vector<Held> held;
  const skgpu::graphite::Context::ContextID identity =
      context.context()->contextID();
  for (const Held& entry : held)
    if (entry.context == identity) return entry.recorder;
  skgpu::graphite::Recorder* made = context.makeRecorder().release();
  if (made) held.push_back({identity, made});
  return made;
}

/** SigilDraw's host-less seam on the process's device. */
class DevicePainting final : public draw::HostlessDevice {
 public:
  sk_sp<SkSurface> makeSurface(const SkImageInfo& info) override {
    skia::GraphiteContext* context = reachable();
    skgpu::graphite::Recorder* recorder =
        context ? recorderOn(*context) : nullptr;
    if (!recorder) return nullptr;
    return SkSurfaces::RenderTarget(recorder, info);
  }

  sk_sp<SkImage> rasterImage(const sk_sp<SkImage>& image) override {
    skia::GraphiteContext* context = reachable();
    skgpu::graphite::Recorder* recorder =
        context ? recorderOn(*context) : nullptr;
    if (!image || !recorder) return nullptr;
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::MakeN32Premul(
            image->width(), image->height(), image->refColorSpace())))
      return nullptr;
    if (!skia::readbackPixels(*context, *recorder, *image, bitmap.pixmap()))
      return nullptr;
    bitmap.setImmutable();
    return bitmap.asImage();
  }

  bool drawsOn(SkCanvas& canvas) override {
    const skgpu::graphite::Recorder* recorder = canvas.recorder();
    skia::GraphiteContext* context = reachable();
    return recorder && context && skia::contextOf(*recorder) == context;
  }

  void submit() override {
    // No host's frame snaps this thread's recorder, so what was drawn
    // with no frame open is inserted here, ahead of any frame that puts
    // it down.
    skia::GraphiteContext* context = reachable();
    skgpu::graphite::Recorder* recorder =
        context ? recorderOn(*context) : nullptr;
    if (recorder)
      (void)context->submit(*recorder, skgpu::graphite::SyncToCpu::kNo);
  }
};

}  // namespace

Painter ownPainter() {
  OwnPainter& own = standUp();
  if (!own.graphite || own.thread != std::this_thread::get_id()) return {};
  return {own.device.get(), own.graphite.get()};
}

Painter standingOwnPainter() {
  if (!stood().load(std::memory_order_acquire)) return {};
  return ownPainter();
}

skia::GraphiteContext* hostlessGraphite() { return reachable(); }

draw::HostlessDevice& hostlessPainting() {
  static DevicePainting painting;
  return painting;
}

}  // namespace sigil::sketch
