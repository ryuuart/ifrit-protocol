/** @file
 * A material painted into a square map: by Skia's CPU rasteriser until a
 * renderer hands it a device, and by Graphite on that device after.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkColorFilter.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/surface/PaintedMap.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <atomic>
#include <mutex>
#include <utility>

namespace sigil::material::surface {

namespace {

bool solid(const Material& material) {
  return material.color() && material.layers().empty();
}

/** Each colour divided by its own coverage: the straight values a data
 *  map is read as. */
sk_sp<SkColorFilter> straightAlphaFilter() {
  static const sk_sp<SkColorFilter> filter = [] {
    const SkRuntimeEffect::Result made = SkRuntimeEffect::MakeForColorFilter(
        SkString("half4 main(half4 color) {\n"
                 "  return color.a > 0 ? half4(color.rgb / color.a, color.a)\n"
                 "                     : half4(0);\n"
                 "}\n"));
    return made.effect ? made.effect->makeColorFilter(nullptr) : nullptr;
  }();
  return filter;
}

/** @p material painted at @p time over @p canvas, @p side pixels across,
 *  with alpha @p straight where a data slot reads the map. */
void paintInto(SkCanvas& canvas, const Material& material, int side,
               std::chrono::duration<double> time, bool straight = false) {
  canvas.clear(SK_ColorTRANSPARENT);
  const Paint paint = skia::paint(material);
  SkPaint painting;
  if (paint.isSolid()) {
    if (!straight) {
      canvas.clear(skia::toSkColor(paint.solidColor()));
      return;
    }
    painting.setColor4f(skia::toSkColor(paint.solidColor()));
  } else {
    FrameData frame;
    frame.seconds = time.count();
    frame.resolution = {(float)side, (float)side};
    painting.setShader(skia::shader(paint, frame));
    if (!painting.getShader()) return;
  }
  if (straight) {
    // Written as it stands rather than blended, so the straight values
    // land in the texels unchanged.
    painting.setColorFilter(straightAlphaFilter());
    painting.setBlendMode(SkBlendMode::kSrc);
  }
  canvas.drawPaint(painting);
}

}  // namespace

/** What was painted, the time it was painted at and how many paintings
 *  have stood — derived state, shared by copies of one source — and the
 *  device it is painted on once one was handed over. */
struct PaintedMap::Kept {
  ~Kept() {
    if (device && handle) device->destroy(handle);
  }

  std::mutex mutex;
  media::Picture picture;
  double paintedAt = 0;
  bool painted = false;
  uint64_t revision = 0;
  /** Shared with every binding a frame hands out, which may paint on the
   *  host for a reader after the frame is gone. */
  std::shared_ptr<std::atomic<uint64_t>> hostPaintings =
      std::make_shared<std::atomic<uint64_t>>(0);

  core::hardware::GpuDevice* device = nullptr;
  sigil::skia::GraphiteContext* context = nullptr;
  core::hardware::TextureHandle handle;
  /** The snapshot of the last painting on the device, which only a
   *  recorder of `context` may draw. */
  sk_sp<SkImage> painting;
};

namespace {

/** THE MAP AS AN IMAGE A RECORDER DRAWS: the painting on the device for
 *  a recorder of the context that painted it, and for any other reader
 *  the same material painted on the host at the same time. */
class PaintedBinding final : public media::DeviceBinding {
 public:
  PaintedBinding(Material material, int side,
                 std::chrono::duration<double> time, sk_sp<SkImage> painting,
                 const sigil::skia::GraphiteContext* context,
                 std::shared_ptr<std::atomic<uint64_t>> hostPaintings)
      : m_material(std::move(material)),
        m_side(side),
        m_time(time),
        m_painting(std::move(painting)),
        m_context(context),
        m_hostPaintings(std::move(hostPaintings)) {}

  sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) override {
    if (recorder && m_painting && m_context &&
        sigil::skia::contextOf(*recorder) == m_context)
      return m_painting;
    const std::lock_guard lock(m_mutex);
    if (m_host) return m_host;
    sk_sp<SkSurface> raster =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(m_side, m_side));
    if (!raster) return nullptr;
    paintInto(*raster->getCanvas(), m_material, m_side, m_time);
    m_host = raster->makeImageSnapshot();
    ++*m_hostPaintings;
    return m_host;
  }

 private:
  Material m_material;
  int m_side;
  std::chrono::duration<double> m_time;
  sk_sp<SkImage> m_painting;
  const sigil::skia::GraphiteContext* m_context;
  std::shared_ptr<std::atomic<uint64_t>> m_hostPaintings;
  std::mutex m_mutex;
  sk_sp<SkImage> m_host;
};

}  // namespace

PaintedMap::PaintedMap(Material material, bool straightAlpha)
    : m_material(std::move(material)),
      m_straightAlpha(straightAlpha),
      m_side(solid(m_material) ? 1 : kSide),
      m_kept(std::make_shared<Kept>()) {}

media::Frame PaintedMap::frameAt(std::chrono::duration<double> time) const {
  Kept& kept = *m_kept;
  const std::lock_guard lock(kept.mutex);
  const bool stale = !kept.painted ||
                     (m_material.isRunning() && kept.paintedAt != time.count());
  media::Frame frame;
  frame.time = time;
  if (kept.device && kept.context && kept.handle) {
    if (stale) {
      // Wrapped fresh for each painting: the wrap is a thin handle over a
      // texture the device owns, driven with the context it was made on.
      sigil::skia::OffscreenSurface target(*kept.context, *kept.device,
                                           kept.handle);
      if (SkCanvas* canvas = target.canvas()) {
        paintInto(*canvas, m_material, m_side, time, m_straightAlpha);
        kept.painting = target.surface()->makeImageSnapshot();
        target.submit();
        kept.painted = true;
        kept.paintedAt = time.count();
        ++kept.revision;
      }
    }
    const core::hardware::NativeTexture native =
        kept.device->exportNative(kept.handle);
    if (kept.painted && native) {
      frame.device.kind = media::DeviceFrame::Kind::Texture;
      frame.device.device = kept.device;
      frame.device.pointer = native.mtlTexture;
      frame.device.handle = native.vkImage;
      frame.device.format = native.vkFormat;
      frame.device.layout = native.vkLayout;
      frame.device.width = native.width;
      frame.device.height = native.height;
      frame.device.mipLevels = native.mipLevels;
      frame.device.revision = kept.revision;
      frame.device.binding = std::make_shared<PaintedBinding>(
          m_material, m_side, std::chrono::duration<double>(kept.paintedAt),
          kept.painting, kept.context, kept.hostPaintings);
      return frame;
    }
  }
  if (stale || !kept.picture) {
    sk_sp<SkSurface> raster =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(m_side, m_side));
    if (!raster) return frame;
    paintInto(*raster->getCanvas(), m_material, m_side, time);
    kept.picture = media::fromSk(raster->makeImageSnapshot());
    kept.painted = true;
    kept.paintedAt = time.count();
    ++kept.revision;
    ++*kept.hostPaintings;
  }
  frame.image = kept.picture;
  return frame;
}

bool PaintedMap::isRunning() const { return m_material.isRunning(); }

glm::ivec2 PaintedMap::size() const { return {m_side, m_side}; }

uint64_t PaintedMap::revision() const {
  const std::lock_guard lock(m_kept->mutex);
  return m_kept->revision;
}

bool PaintedMap::operator==(const PaintedMap& other) const {
  return m_straightAlpha == other.m_straightAlpha &&
         m_material == other.m_material;
}

bool PaintedMap::useDevice(core::hardware::GpuDevice& device,
                           sigil::skia::GraphiteContext& context) const {
  Kept& kept = *m_kept;
  const std::lock_guard lock(kept.mutex);
  if (kept.device == &device && kept.context == &context && kept.handle)
    return true;
  // THE TEXTURE CARRIES ITS OWN CHAIN: a body wearing the map smaller than
  // it is reads the level its size calls for, and whoever wears it fills
  // the levels below the one painted here.
  core::hardware::TextureDescription description;
  description.width = m_side;
  description.height = m_side;
  description.format = core::hardware::TextureFormat::RGBA8Unorm;
  description.mipLevels = core::hardware::mipLevelsFor(m_side, m_side);
  description.label = "painted material map";
  core::hardware::TextureHandle handle = device.createTexture(description);
  if (!handle) return false;
  {
    const sigil::skia::OffscreenSurface probe(context, device, handle);
    if (!probe.canvas()) {
      device.destroy(handle);
      return false;
    }
  }
  if (kept.device && kept.handle) kept.device->destroy(kept.handle);
  kept.device = &device;
  kept.context = &context;
  kept.handle = handle;
  kept.painting.reset();
  // What stands in the new texture is nothing yet.
  kept.painted = false;
  return true;
}

uint64_t PaintedMap::hostPaintings() const {
  return m_kept->hostPaintings->load();
}

}  // namespace sigil::material::surface
