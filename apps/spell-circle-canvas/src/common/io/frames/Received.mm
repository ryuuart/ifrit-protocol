// An arrived frame made a picture: the Metal texture retained, and the
// binding that wraps it for the recorder a drawing is recorded on — or
// reads it back into host memory — and turns it the right way up once
// per arrival.

#import <Metal/Metal.h>

#include "Received.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <include/gpu/graphite/Surface.h>
#include <sigilskia/graphite/TextureImage.h>

#include <mutex>
#include <utility>
#include <sigilmedia/advanced/Skia.h>

namespace sigil::io::frames::detail {

namespace {

/** @p carried drawn once into a target of its own on @p recorder, first
 *  row last. @p target is the surface the turn lands in, held for as long
 *  as the answer is: an image made from a surface names the surface's
 *  texture. Null when no target could be made, which leaves a scene with
 *  no picture rather than with a picture the wrong way up. */
sk_sp<SkImage> turnOver(skgpu::graphite::Recorder& recorder,
                        const sk_sp<SkImage>& carried, sk_sp<SkSurface>& target) {
  target.reset();
  if (!carried) return nullptr;
  target = SkSurfaces::RenderTarget(&recorder, carried->imageInfo());
  if (!target) return nullptr;
  SkCanvas* canvas = target->getCanvas();
  canvas->translate(0, (float)carried->height());
  canvas->scale(1, -1);
  canvas->drawImage(carried, 0, 0, SkSamplingOptions());
  return SkSurfaces::AsImage(target);
}

/** The same turn for pixels already in host memory: the rows walked
 *  backwards into a bitmap of its own — a row is the same bytes wherever
 *  it stands, so nothing is resampled and no channel moves. */
sk_sp<SkImage> turnOver(const sk_sp<SkImage>& read) {
  if (!read) return nullptr;
  SkBitmap turned;
  if (!turned.tryAllocPixels(read->imageInfo())) return nullptr;
  SkCanvas canvas(turned);
  canvas.translate(0, (float)read->height());
  canvas.scale(1, -1);
  canvas.drawImage(read, 0, 0, SkSamplingOptions());
  turned.setImmutable();
  return turned.asImage();
}

/** ONE ARRIVAL, BOUND: one turn per recorder that asks, one read back for
 *  every caller with none. The frame is premultiplied the way the
 *  publisher's canvas wrote it and its texels mean what its own format
 *  says; nothing converts on the way across, so no colour space is
 *  stated. */
class ArrivalBinding final : public media::DeviceBinding {
 public:
  explicit ArrivalBinding(std::shared_ptr<void> texture) : m_texture(std::move(texture)) {}

  sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) override {
    std::lock_guard lock(m_mutex);
    if (recorder) {
      if (m_recorder != recorder || !m_turned) {
        m_recorder = recorder;
        m_turned = turnOver(*recorder, skia::wrapImage(*recorder, m_texture.get()), m_target);
      }
      return m_turned;
    }
    if (!m_read) m_read = turnOver(skia::readImage(m_texture.get()));
    return m_read;
  }

 private:
  std::mutex m_mutex;
  std::shared_ptr<void> m_texture;
  skgpu::graphite::Recorder* m_recorder = nullptr;
  sk_sp<SkSurface> m_target;
  sk_sp<SkImage> m_turned;
  sk_sp<SkImage> m_read;
};

}  // namespace

std::shared_ptr<void> retainTexture(void* texture) {
  if (!texture) return nullptr;
  CFRetain(texture);
  return std::shared_ptr<void>(texture, [](void* held) { CFRelease(held); });
}

std::shared_ptr<media::DeviceBinding> bindArrival(std::shared_ptr<void> texture) {
  if (!texture) return nullptr;
  return std::make_shared<ArrivalBinding>(std::move(texture));
}

}  // namespace sigil::io::frames::detail
