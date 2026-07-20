#include "WebInternal.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <algorithm>
#include <cstring>

namespace sigil::scry {

WebImage::WebImage(std::shared_ptr<WebEngine> engine,
                   std::shared_ptr<Impl> impl)
    : m_engine(std::move(engine)), m_impl(std::move(impl)) {}

WebImage::~WebImage() {
  auto impl = m_impl;
  m_impl->engine->post([impl] {
    ultralight::ImageSourceProvider::instance().RemoveImageSource(
        impl->name.c_str());
    impl->source = nullptr;
    impl->bitmap = nullptr;
    if (impl->gpuTexture) {
      WebGpuDriver *driver = impl->engine->gpuDriver();
      driver->unregisterExternalTexture(impl->gpuTextureId);
      driver->releaseNativeTexture(impl->gpuTexture);
      impl->gpuTexture = nullptr;
    }
  });
}

const std::string &WebImage::name() const { return m_impl->name; }
int WebImage::width() const { return m_impl->width; }
int WebImage::height() const { return m_impl->height; }

bool WebImage::paint(const std::function<void(SkCanvas &)> &painter) {
  bool ok = false;
  auto impl = m_impl;
  m_impl->engine->postAndWait([impl, &painter, &ok] {
    if (impl->gpuTexture) {
      ok = impl->engine->gpuDriver()->paintTexture(
          impl->gpuTexture, impl->width, impl->height, painter);
      if (ok && impl->source)
        impl->source->Invalidate();
      return;
    }
    if (!impl->bitmap)
      return;
    if (void *pixels = impl->bitmap->LockPixels()) {
      SkImageInfo info = SkImageInfo::Make(
          impl->width, impl->height, kBGRA_8888_SkColorType,
          kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
      if (std::unique_ptr<SkCanvas> canvas = SkCanvas::MakeRasterDirect(
              info, pixels, impl->bitmap->row_bytes())) {
        painter(*canvas);
        ok = true;
      }
      impl->bitmap->UnlockPixels();
    }
    if (ok && impl->source)
      impl->source->Invalidate();
  });
  return ok;
}

bool WebImage::update(const SkPixmap &pixels) {
  // Convert to premultiplied BGRA (Ultralight's pixel format) on the
  // caller's thread; only the upload runs on the web thread.
  SkImageInfo info = SkImageInfo::Make(
      m_impl->width, m_impl->height, kBGRA_8888_SkColorType,
      kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  auto converted = std::make_shared<SkBitmap>();
  if (!converted->tryAllocPixels(info) ||
      !pixels.readPixels(converted->pixmap(), 0, 0))
    return false;

  auto impl = m_impl;
  m_impl->engine->post([impl, converted] {
    if (impl->gpuTexture) {
      impl->engine->gpuDriver()->uploadToTexture(
          impl->gpuTexture, converted->getPixels(), converted->width(),
          converted->height(), converted->rowBytes());
      if (impl->source)
        impl->source->Invalidate();
      return;
    }
    if (!impl->bitmap)
      return;
    if (void *dst = impl->bitmap->LockPixels()) {
      const size_t dstRowBytes = impl->bitmap->row_bytes();
      const size_t copyBytes =
          std::min(dstRowBytes, converted->rowBytes());
      for (int y = 0; y < converted->height(); ++y)
        std::memcpy(static_cast<char *>(dst) + y * dstRowBytes,
                    static_cast<const char *>(converted->getPixels()) +
                        y * converted->rowBytes(),
                    copyBytes);
      impl->bitmap->UnlockPixels();
    }
    if (impl->source)
      impl->source->Invalidate();
  });
  return true;
}

bool WebImage::update(const sk_sp<SkImage> &image) {
  if (!image)
    return false;
  SkPixmap pixmap;
  if (image->peekPixels(&pixmap))
    return update(pixmap);
  if (image->isTextureBacked()) {
    m_impl->engine->logger()->log(
        LogLevel::Warning,
        "WebImage '" + m_impl->name +
            "': texture-backed SkImages are recorder-bound and can't be "
            "read here — use updateTexture() with the native texture, or "
            "draw via paint()");
    return false;
  }
  // Lazy/generated raster image without peekable pixels: rasterize it.
  SkImageInfo info = SkImageInfo::Make(
      image->width(), image->height(), kBGRA_8888_SkColorType,
      kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(info) ||
      !image->readPixels(nullptr, bitmap.pixmap(), 0, 0))
    return false;
  return update(bitmap.pixmap());
}

bool WebImage::updateTexture(void *texture) {
  if (!texture || !m_impl->gpuTexture)
    return false;
  auto impl = m_impl;
  bool ok = false;
  m_impl->engine->postAndWait([impl, texture, &ok] {
    if (!impl->gpuTexture)
      return;
    impl->engine->gpuDriver()->copyNativeTexture(
        texture, impl->gpuTexture, impl->width, impl->height);
    if (impl->source)
      impl->source->Invalidate();
    ok = true;
  });
  return ok;
}

void *WebImage::nativeTexture() const { return m_impl->gpuTexture; }

void WebImage::invalidate() {
  auto impl = m_impl;
  m_impl->engine->post([impl] {
    if (impl->source)
      impl->source->Invalidate();
  });
}

} // namespace sigil::scry
