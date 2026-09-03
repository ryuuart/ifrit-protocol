/** @file
 * The scene behind a scene texture: the composer, the surface it paints
 * into — raster or a texture on a device — and the count of paints that
 * is the texture value's identity.
 */

#include "sigilcompose/texture/Texture.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>
#include <include/core/SkSurfaceProps.h>
#include <sigilcompose/core/Composer.h>
#include <sigilmotion/clock/FrameClock.h>
#include <sigilmotion/clock/Ticker.h>
#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
#include <sigilcore/hardware/GpuDevice.h>
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>
#endif

#include <algorithm>
#include <utility>

namespace sigil::compose {

struct TextureScene::Impl {
#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
  ~Impl() {
    if (device && handle) device->destroy(handle);
  }
#endif

  SkISize size{0, 0};
  SkColor4f background{0, 0, 0, 0};
  motion::Ticker ticker;
  motion::FrameClock clock;
  std::unique_ptr<Composer> composer;

  /** The raster surface, which every scene starts on. Null once a device
   *  took over. */
  sk_sp<SkSurface> raster;

#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
  /** The device side: the texture the scene paints into and the context
   *  that wraps it. Both null on the raster path. */
  core::hardware::GpuDevice* device = nullptr;
  skia::GraphiteContext* context = nullptr;
  core::hardware::TextureHandle handle;
#endif

  sk_sp<SkImage> image;
  uint64_t version = 0;
  double seconds = 0.0;
  bool painted = false;

  void makeRaster() {
    raster = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(size.width(), size.height()));
  }

  /** Paints the tree into whichever surface the scene stands on, and
   *  leaves what it painted in `image`. */
  void paint() {
#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
    if (device && context) {
      // Wrapped fresh for each paint: the wrap is a thin, cheap handle
      // over a texture the device owns, and the Graphite context it is
      // driven with is the one it was made on. The context's own lock is
      // taken by the submit below and must not be held around it.
      skia::OffscreenSurface surface(*context, *device, handle);
      SkCanvas* canvas = surface.canvas();
      if (!canvas) return;
      canvas->clear(background);
      composer->draw(*canvas);
      if (surface.surface()) image = surface.surface()->makeImageSnapshot();
      surface.submit();
      return;
    }
#endif
    if (!raster) return;
    SkCanvas* canvas = raster->getCanvas();
    canvas->clear(background);
    composer->draw(*canvas);
    image = raster->makeImageSnapshot();
  }
};

TextureScene::TextureScene() : m_impl(std::make_unique<Impl>()) {}
TextureScene::~TextureScene() = default;

std::shared_ptr<TextureScene> TextureScene::make(SkISize size,
                                                 weave::FontContext& fonts,
                                                 SkColor4f background) {
  std::shared_ptr<TextureScene> scene(new TextureScene());
  Impl& impl = *scene->m_impl;
  impl.size = {std::max(1, size.width()), std::max(1, size.height())};
  impl.background = background;
  impl.composer = std::make_unique<Composer>(impl.ticker, fonts);
  impl.composer->setSize(
      SkSize::Make((float)impl.size.width(), (float)impl.size.height()));
  impl.composer->setClock(&impl.clock);
  impl.makeRaster();
  return scene;
}

#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
bool TextureScene::useDevice(core::hardware::GpuDevice& device,
                             skia::GraphiteContext& context) {
  Impl& impl = *m_impl;
  // The usage left at its default is the one a scene needs: a shader
  // reads the texture and a canvas paints into it.
  core::hardware::TextureDesc desc;
  desc.width = impl.size.width();
  desc.height = impl.size.height();
  desc.format = core::hardware::TextureFormat::RGBA8Unorm;
  desc.label = "compose scene";
  const core::hardware::TextureHandle handle = device.createTexture(desc);
  if (!handle) return false;
  {
    const skia::OffscreenSurface probe(context, device, handle);
    if (!probe.canvas()) {
      device.destroy(handle);
      return false;
    }
  }
  if (impl.device && impl.handle) impl.device->destroy(impl.handle);
  impl.device = &device;
  impl.context = &context;
  impl.handle = handle;
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
#else
bool TextureScene::useDevice(core::hardware::GpuDevice&,
                             skia::GraphiteContext&) {
  // This build carries no device feature, so there is no device to paint
  // on and the raster surface stands.
  return false;
}
#endif

void TextureScene::render(const Element& root, double seconds) {
  Impl& impl = *m_impl;
  const double delta = std::max(0.0, seconds - impl.seconds);
  impl.seconds = seconds;
  impl.ticker.tick(delta);
  impl.clock.tick(seconds);
  impl.composer->render(root);
  // THE ONE PLACE THE VERSION MOVES. A reconcile that changed nothing
  // and no transition in flight means the pixels standing in the surface
  // are already the answer, and a consumer must be able to tell that
  // from the value alone.
  if (impl.painted && !impl.composer->active()) return;
  impl.paint();
  impl.painted = true;
  ++impl.version;
}

material::Texture TextureScene::texture() const {
  return material::Texture(SceneSource(shared_from_this(), m_impl->version));
}

uint64_t TextureScene::version() const { return m_impl->version; }
SkISize TextureScene::size() const { return m_impl->size; }
sk_sp<SkImage> TextureScene::image() const { return m_impl->image; }

material::DeviceImage TextureScene::deviceImage() const {
#ifdef SIGILCOMPOSE_TEXTURE_DEVICE
  const Impl& impl = *m_impl;
  if (!impl.device || !impl.handle) return {};
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
#else
  return {};
#endif
}

bool TextureScene::active() const { return m_impl->composer->active(); }

const Composer& TextureScene::composer() const { return *m_impl->composer; }

material::Texture texture(const Element& root, SkISize size,
                          weave::FontContext& fonts, SkColor4f background) {
  const std::shared_ptr<TextureScene> scene =
      TextureScene::make(size, fonts, background);
  scene->render(root);
  return scene->texture();
}

}  // namespace sigil::compose
