/** @file
 * The scene behind a scene texture: the composer, the surface it paints
 * into — raster or a texture on a device — and the count of paints that
 * is the texture value's identity.
 */

#include "sigilcompose/texture/Texture.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Lit.h>
#include <sigilmedia/advanced/Skia.h>
#include <sigilmotion/clock/Engine.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <algorithm>
#include <memory>
#include <utility>

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

  /** Paints the tree into whichever surface the scene stands on, and
   *  leaves what it painted in `image`. */
  bool paint() {
    if (device && context) {
      // Wrapped fresh for each paint: the wrap is a thin, cheap handle
      // over a texture the device owns, and the Graphite context it is
      // driven with is the one it was made on. The context's own lock is
      // taken by the submit below and must not be held around it.
      skia::OffscreenSurface surface(*context, *device, handle,
                                     info.refColorSpace());
      SkCanvas* canvas = surface.canvas();
      if (!canvas) return false;
      canvas->clear(material::skia::toSkColor(background));
      composer->draw(*canvas);
      if (surface.surface()) image = surface.surface()->makeImageSnapshot();
      surface.submit();
      return image != nullptr;
    }
    if (!raster) return false;
    SkCanvas* canvas = raster->getCanvas();
    canvas->clear(material::skia::toSkColor(background));
    composer->draw(*canvas);
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

bool TextureScene::useDevice(core::hardware::GpuDevice& device,
                             skia::GraphiteContext& context) {
  Impl& impl = *m_impl;
  // The usage left at its default is the one a scene needs: a shader
  // reads the texture and a canvas paints into it.
  core::hardware::TextureDescription desc;
  desc.width = impl.info.width();
  desc.height = impl.info.height();
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

void TextureScene::render(const Element& root, double seconds) {
  Impl& impl = *m_impl;
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

media::Frame SceneSource::frameAt(std::chrono::duration<double>) const {
  media::Frame frame;
  if (!m_scene) return frame;
  frame.image = media::fromSk(m_scene->image());
  const material::DeviceImage where = m_scene->deviceImage();
  if (!where) return frame;
  frame.device.kind = media::DeviceFrame::Kind::Texture;
  frame.device.device = where.device;
  frame.device.pointer = where.pointer;
  frame.device.handle = where.handle;
  frame.device.format = where.format;
  frame.device.layout = where.layout;
  frame.device.width = where.width;
  frame.device.height = where.height;
  return frame;
}
SkISize TextureScene::size() const { return m_impl->info.dimensions(); }
sk_sp<SkImage> TextureScene::image() const { return m_impl->image; }

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
  return out;
}

bool TextureScene::isRunning() const { return m_impl->composer->isRunning(); }

const Composer& TextureScene::composer() const { return *m_impl->composer; }

void TextureScene::setAutoTexturePromotion(PromotionPolicy policy) {
  m_impl->composer->setAutoTexturePromotion(policy);
}

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

}  // namespace sigil::compose
