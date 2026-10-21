/** @file
 * The scene behind a scene texture: the composer, the surface it paints
 * into — raster or a texture on a device — and the count of paints that
 * is the texture value's identity.
 */

#include "sigilcompose/texture/Texture.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRegion.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilgeometry/advanced/Skia.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/SurfaceMaps.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/graphite/Readback.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace sigil::compose {

namespace {

bool supportedRaster(const SkImageInfo& info) {
  if (info.width() <= 0 || info.height() <= 0 ||
      info.alphaType() != kPremul_SkAlphaType)
    return false;
  switch (info.colorType()) {
    case kRGBA_8888_SkColorType:
    case kBGRA_8888_SkColorType:
    case kRGBA_F16_SkColorType:
    case kRGBA_F32_SkColorType:
      return true;
    default:
      return false;
  }
}

struct CandidateTexture {
  core::hardware::GpuDevice& device;
  core::hardware::TextureHandle handle;
  ~CandidateTexture() {
    if (handle) device.destroy(handle);
  }
};

}  // namespace

struct TextureScene::Impl {
  ~Impl() {
    if (device && handle) device->destroy(handle);
  }

  SkImageInfo info;
  /** What the surface is cleared to: the stated background, or the
   *  ground of the surface map the tree is painted as. */
  material::Color background{0, 0, 0, 0};
  material::Color statedBackground{0, 0, 0, 0};
  motion::Engine engine;
  std::unique_ptr<Composer> composer;

  /** The raster surface, which every scene starts on. Null once a device
   *  took over. */
  sk_sp<SkSurface> raster;

  /** The device side: the texture the scene paints into and the context
   *  that wraps it. Both null on the raster path. */
  core::hardware::GpuDevice* device = nullptr;
  skia::GraphiteContext* context = nullptr;
  core::hardware::TextureHandle handle;

  sk_sp<SkImage> image;
  uint64_t version = 0;
  bool painted = false;

  /** A cut's source, the rect of it the cut shows, and the source's
   *  revision the cut last copied. No source on a scene with a tree. */
  std::shared_ptr<const TextureScene> cutFrom;
  SkIRect cutRect = SkIRect::MakeEmpty();
  uint64_t cutRevision = 0;

  /** The source's rect copied over the whole of @p canvas, pixel for
   *  pixel. */
  void copyCut(SkCanvas& canvas) const {
    const sk_sp<SkImage> from = cutFrom->image();
    canvas.clear(SK_ColorTRANSPARENT);
    if (!from) return;
    SkPaint copy;
    copy.setBlendMode(SkBlendMode::kSrc);
    canvas.drawImageRect(from, SkRect::Make(cutRect),
                         SkRect::MakeIWH(cutRect.width(), cutRect.height()),
                         SkSamplingOptions(), &copy,
                         SkCanvas::kStrict_SrcRectConstraint);
  }

  /** THE PIXELS A PAINT REPAINTS: the rects the composer says its draw
   *  changes, each grown to whole pixels with one more for the antialiased
   *  edge, or the whole surface when the composer cannot say or nothing
   *  stands in the surface yet. Empty when nothing changed. */
  std::optional<SkRegion> repaintRegion() {
    // Asked even when the whole surface is painted, so the draw keeps what
    // it painted for the next one.
    const std::optional<std::vector<geometry::path::Rect>> changed =
        composer->changedRegion();
    if (!painted || !image || !changed) return std::nullopt;
    const SkIRect whole = SkIRect::MakeSize(info.dimensions());
    SkRegion pixels;
    for (const geometry::path::Rect& part : *changed) {
      SkRect grown = geometry::path::toSk(part);
      grown.outset(1, 1);
      SkIRect rect = grown.roundOut();
      if (rect.intersect(whole)) pixels.op(rect, SkRegion::kUnion_Op);
    }
    if (pixels.contains(whole)) return std::nullopt;
    return pixels;
  }

  /** The tree drawn over @p canvas: within @p region over what the last
   *  paint left there, and over the whole surface when the draw reached
   *  past it or there is no region. */
  void drawInto(SkCanvas& canvas, const std::optional<SkRegion>& region) {
    if (cutFrom) {
      copyCut(canvas);
      return;
    }
    if (region) {
      canvas.save();
      canvas.clipRegion(*region);
      canvas.clear(material::skia::toSkColor(background));
      composer->draw(canvas);
      canvas.restore();
      if (composer->drewWithinChangedRegion()) return;
    }
    canvas.clear(material::skia::toSkColor(background));
    composer->draw(canvas);
  }

  /** Paints the tree into whichever surface the scene stands on, and
   *  leaves what it painted in `image`. A tree whose draw changes a region
   *  of what the last paint left repaints that region alone; one whose
   *  draw changes nothing paints nothing and answers false. */
  bool paint() {
    const std::optional<SkRegion> region =
        cutFrom ? std::nullopt : repaintRegion();
    if (region && region->isEmpty()) return false;
    if (device && context) {
      // Wrapped fresh for each paint: the wrap is a thin, cheap handle
      // over a texture the device owns, and the Graphite context it is
      // driven with is the one it was made on. The context's own lock is
      // taken by the submit below and must not be held around it.
      skia::OffscreenSurface surface(*context, *device, handle,
                                     info.refColorSpace());
      SkCanvas* canvas = surface.canvas();
      if (!canvas) return false;
      drawInto(*canvas, region);
      if (surface.surface()) image = surface.surface()->makeImageSnapshot();
      surface.submit();
      return image != nullptr;
    }
    if (!raster) return false;
    drawInto(*raster->getCanvas(), region);
    image = raster->makeImageSnapshot();
    return image != nullptr;
  }
};

TextureScene::TextureScene() : m_impl(std::make_unique<Impl>()) {}
TextureScene::~TextureScene() = default;

std::shared_ptr<TextureScene> TextureScene::make(SkISize size,
                                                 weave::FontContext& fonts,
                                                 material::Color background) {
  return make(SkImageInfo::MakeN32Premul(std::max(1, size.width()),
                                         std::max(1, size.height())),
              fonts, background);
}

std::shared_ptr<TextureScene> TextureScene::make(SkImageInfo info,
                                                 weave::FontContext& fonts,
                                                 material::Color background) {
  if (!supportedRaster(info)) return nullptr;
  std::shared_ptr<TextureScene> scene(new TextureScene());
  Impl& impl = *scene->m_impl;
  impl.info = std::move(info);
  impl.raster = SkSurfaces::Raster(impl.info);
  if (!impl.raster) return nullptr;
  impl.background = background;
  impl.statedBackground = background;
  impl.composer = std::make_unique<Composer>(impl.engine, fonts);
  impl.composer->setSize(
      glm::vec2{(float)impl.info.width(), (float)impl.info.height()});
  return scene;
}

std::shared_ptr<TextureScene> TextureScene::cut(
    std::shared_ptr<const TextureScene> source, material::PixelRect rect,
    weave::FontContext& fonts) {
  if (!source || rect.empty()) return nullptr;
  SkIRect pixels = SkIRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
  if (!pixels.intersect(SkIRect::MakeSize(source->size()))) return nullptr;
  std::shared_ptr<TextureScene> scene =
      make(SkImageInfo::MakeN32Premul(pixels.width(), pixels.height()), fonts);
  if (!scene) return nullptr;
  scene->m_impl->cutFrom = std::move(source);
  scene->m_impl->cutRect = pixels;
  return scene;
}

std::optional<material::PixelRect> TextureScene::cutRect() const {
  const Impl& impl = *m_impl;
  if (!impl.cutFrom) return std::nullopt;
  return material::PixelRect{impl.cutRect.x(), impl.cutRect.y(),
                             impl.cutRect.width(), impl.cutRect.height()};
}

bool TextureScene::useDevice(core::hardware::GpuDevice& device,
                             skia::GraphiteContext& context) {
  Impl& impl = *m_impl;
  // The usage left at its default is the one a scene needs: a shader
  // reads the texture and a canvas paints into it. THE TEXTURE CARRIES
  // ITS OWN CHAIN: a body wearing the scene smaller than it is reads a
  // level its size calls for, and whoever wears it fills the levels below
  // the one painted here in place, so no second texture is kept for them.
  core::hardware::TextureDescription desc;
  desc.width = impl.info.width();
  desc.height = impl.info.height();
  desc.mipLevels = core::hardware::mipLevelsFor(desc.width, desc.height);
  switch (impl.info.colorType()) {
    case kRGBA_8888_SkColorType:
      desc.format = core::hardware::TextureFormat::RGBA8Unorm;
      break;
    case kBGRA_8888_SkColorType:
      desc.format = core::hardware::TextureFormat::BGRA8Unorm;
      break;
    case kRGBA_F16_SkColorType:
      desc.format = core::hardware::TextureFormat::RGBA16Float;
      break;
    default:
      return false;
  }
  desc.label = "compose scene";
  CandidateTexture candidate{device, device.createTexture(desc)};
  if (!candidate.handle) return false;
  {
    const skia::OffscreenSurface probe(context, device, candidate.handle,
                                       impl.info.refColorSpace());
    if (!probe.canvas() ||
        probe.surface()->imageInfo().colorInfo() != impl.info.colorInfo())
      return false;
  }
  if (impl.device && impl.handle) impl.device->destroy(impl.handle);
  impl.device = &device;
  impl.context = &context;
  impl.handle = std::exchange(candidate.handle, {});
  impl.raster.reset();
  // The pixels stand somewhere else now, so nothing painted before this
  // is what the scene holds.
  impl.image.reset();
  impl.painted = false;
  // The retained tree is untouched, but every cache it holds was minted
  // by a surface that is no longer the one being painted into.
  impl.composer->purgeCaches();
  return true;
}

void TextureScene::setSurfaceMap(std::optional<material::texture::Role> role) {
  Impl& impl = *m_impl;
  if (impl.composer->surfaceMap() == role) return;
  impl.composer->setSurfaceMap(role);
  impl.background =
      role ? material::skia::surfaceMapGround(*role) : impl.statedBackground;
  // What stands in the surface was painted for the other map.
  impl.painted = false;
}

void TextureScene::setSurfaceMapScale(float scale) {
  Impl& impl = *m_impl;
  const float before = impl.composer->surfaceMapScale();
  impl.composer->setSurfaceMapScale(scale);
  // What stands in the surface was read against the other scale.
  if (impl.composer->surfaceMapScale() != before) impl.painted = false;
}

void TextureScene::render(const Element& root, double seconds) {
  Impl& impl = *m_impl;
  if (impl.cutFrom) {
    const uint64_t source = impl.cutFrom->revision();
    if (impl.painted && source == impl.cutRevision) return;
    if (!impl.paint()) return;
    impl.cutRevision = source;
    impl.painted = true;
    ++impl.version;
    return;
  }
  impl.engine.advance(motion::Duration(seconds));
  impl.composer->render(root);
  // THE ONE PLACE THE VERSION MOVES. A reconcile that changed nothing
  // and no transition in flight means the pixels standing in the surface
  // are already the answer, and a consumer must be able to tell that
  // from the value alone.
  if (impl.painted && !impl.composer->isRunning()) return;
  if (!impl.paint()) return;
  impl.painted = true;
  ++impl.version;
}

material::Texture TextureScene::texture() const {
  return material::Texture(SceneSource(shared_from_this(), m_impl->version));
}

uint64_t TextureScene::revision() const { return m_impl->version; }

namespace {

/** A SCENE PAINTED ON A DEVICE, AS THE IMAGE A RECORDER DRAWS. The
 *  snapshot the scene keeps belongs to the Graphite context it painted
 *  through, so only a recorder of that same context may draw it — two
 *  contexts on one device each keep their own resources, and neither may
 *  draw an image the other made. Any other recorder, and none, is handed
 *  the pixels read back to host memory, read once per revision of the
 *  scene. */
class SceneBinding final : public media::DeviceBinding {
 public:
  explicit SceneBinding(std::shared_ptr<const TextureScene> scene)
      : m_scene(std::move(scene)) {}

  sk_sp<SkImage> image(skgpu::graphite::Recorder* recorder) override {
    const sk_sp<SkImage> painted = m_scene->image();
    if (!painted) return nullptr;
    if (recorder && m_scene->recorder() &&
        skia::contextOf(*recorder) == skia::contextOf(*m_scene->recorder()) &&
        skia::contextOf(*recorder))
      return painted;
    std::lock_guard lock(m_mutex);
    if (m_read && m_readFrom == painted) return m_read;
    m_read = m_scene->hostImage();
    m_readFrom = m_read ? painted : nullptr;
    return m_read;
  }

 private:
  std::mutex m_mutex;
  std::shared_ptr<const TextureScene> m_scene;
  sk_sp<SkImage> m_readFrom;
  sk_sp<SkImage> m_read;
};

}  // namespace

media::Frame SceneSource::frameAt(std::chrono::duration<double>) const {
  media::Frame frame;
  if (!m_scene) return frame;
  const material::DeviceImage where = m_scene->deviceImage();
  if (!where) {
    frame.image = media::fromSk(m_scene->image());
    return frame;
  }
  // On a device the pixels are named, never handed over as the image:
  // the snapshot belongs to the scene's own context, and the binding
  // answers each recorder with what it can draw.
  frame.device.binding = std::make_shared<SceneBinding>(m_scene);
  frame.device.kind = media::DeviceFrame::Kind::Texture;
  frame.device.device = where.device;
  frame.device.pointer = where.pointer;
  frame.device.handle = where.handle;
  frame.device.format = where.format;
  frame.device.layout = where.layout;
  frame.device.width = where.width;
  frame.device.height = where.height;
  frame.device.mipLevels = where.mipLevels;
  return frame;
}
SkISize TextureScene::size() const { return m_impl->info.dimensions(); }
sk_sp<SkImage> TextureScene::image() const { return m_impl->image; }
skgpu::graphite::Recorder* TextureScene::recorder() const {
  return m_impl->context ? m_impl->context->recorder() : nullptr;
}

sk_sp<SkImage> TextureScene::hostImage() const {
  const Impl& impl = *m_impl;
  if (!impl.image) return nullptr;
  if (!impl.device || !impl.context || !impl.handle) return impl.image;
  // Wrapped as a paint wraps it, and read through the context that
  // painted it, which waits for that paint on the device.
  skia::OffscreenSurface surface(*impl.context, *impl.device, impl.handle,
                                 impl.info.refColorSpace());
  if (!surface.surface()) return nullptr;
  SkBitmap pixels;
  if (!pixels.tryAllocPixels(impl.info) ||
      !skia::readbackPixels(*impl.context, *surface.surface(), pixels.pixmap()))
    return nullptr;
  pixels.setImmutable();
  return pixels.asImage();
}

material::DeviceImage TextureScene::deviceImage() const {
  const Impl& impl = *m_impl;
  if (!impl.painted || !impl.device || !impl.handle) return {};
  const core::hardware::NativeTexture native =
      impl.device->exportNative(impl.handle);
  if (!native) return {};
  material::DeviceImage out;
  out.device = impl.device;
  out.pointer = native.mtlTexture;
  out.handle = native.vkImage;
  out.format = native.vkFormat;
  out.layout = native.vkLayout;
  out.width = native.width;
  out.height = native.height;
  out.mipLevels = native.mipLevels;
  return out;
}

bool TextureScene::isRunning() const {
  if (m_impl->cutFrom) return false;
  return m_impl->composer->isRunning();
}

const Composer& TextureScene::composer() const { return *m_impl->composer; }

material::Texture texture(const Element& root, SkISize size,
                          weave::FontContext& fonts,
                          material::Color background) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make(size, fonts, background);
  if (!scene) return {};
  scene->render(root);
  return scene->texture();
}

material::Texture texture(const Element& root, SkImageInfo info,
                          weave::FontContext& fonts,
                          material::Color background) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make(std::move(info), fonts, background);
  if (!scene) return {};
  scene->render(root);
  return scene->texture();
}

struct Textures::Impl {
  explicit Impl(weave::FontContext& fontsIn) : fonts(fontsIn) {}

  /** One painted tree: what it was asked for with, the texture it was
   *  painted as, and whether it was asked for since the last sweep. */
  struct Entry {
    Element tree;
    SkISize size;
    material::Color background;
    material::Texture texture;
    bool asked = true;
  };

  weave::FontContext& fonts;
  core::hardware::GpuDevice* device = nullptr;
  skia::GraphiteContext* context = nullptr;
  std::vector<Entry> entries;
  uint64_t paints = 0;
};

Textures::Textures(weave::FontContext& fonts)
    : m_impl(std::make_unique<Impl>(fonts)) {}

Textures::~Textures() = default;

void Textures::useDevice(core::hardware::GpuDevice& device,
                         skia::GraphiteContext& context) {
  m_impl->device = &device;
  m_impl->context = &context;
}

material::Texture Textures::paint(const Element& tree, SkISize size,
                                  material::Color background) {
  Impl& impl = *m_impl;
  // The cheap halves of the key first: a size and a background decide
  // most misses before a whole description is walked.
  for (Impl::Entry& entry : impl.entries) {
    if (entry.size != size || entry.background != background) continue;
    if (!sameDescription(entry.tree, tree)) continue;
    entry.asked = true;
    return entry.texture;
  }
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make(size, impl.fonts, background);
  if (!scene) return {};
  if (impl.device && impl.context)
    (void)scene->useDevice(*impl.device, *impl.context);
  scene->render(tree);
  ++impl.paints;
  material::Texture painted = scene->texture();
  impl.entries.push_back({tree, size, background, painted});
  return painted;
}

uint64_t Textures::paints() const { return m_impl->paints; }

size_t Textures::kept() const { return m_impl->entries.size(); }

void Textures::sweep() {
  std::vector<Impl::Entry>& entries = m_impl->entries;
  std::erase_if(entries,
                [](const Impl::Entry& entry) { return !entry.asked; });
  for (Impl::Entry& entry : entries) entry.asked = false;
}

}  // namespace sigil::compose
