#include "WebInternal.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkPixmap.h>

#include <algorithm>
#include <cstring>

namespace ifrit::web {

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
#ifdef __APPLE__
    if (impl->gpuTexture) {
      impl->engine->gpuDriver()->unregisterExternalTexture(
          impl->gpuTextureId);
      UltralightMetalDriver::releaseTexture(impl->gpuTexture);
      impl->gpuTexture = nullptr;
    }
#endif
  });
}

const std::string &WebImage::name() const { return m_impl->name; }
int WebImage::width() const { return m_impl->width; }
int WebImage::height() const { return m_impl->height; }

bool WebImage::paint(const std::function<void(SkCanvas &)> &painter) {
  bool ok = false;
  auto impl = m_impl;
  m_impl->engine->postAndWait([impl, &painter, &ok] {
#ifdef __APPLE__
    if (impl->gpuTexture) {
      ok = impl->engine->gpuDriver()->paintTexture(
          impl->gpuTexture, impl->width, impl->height, painter);
      if (ok && impl->source)
        impl->source->Invalidate();
      return;
    }
#endif
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

void WebImage::update(const SkPixmap &pixels) {
  // Convert to premultiplied BGRA (Ultralight's pixel format) on the
  // caller's thread; only the upload runs on the web thread.
  SkImageInfo info = SkImageInfo::Make(
      m_impl->width, m_impl->height, kBGRA_8888_SkColorType,
      kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
  auto converted = std::make_shared<SkBitmap>();
  if (!converted->tryAllocPixels(info) ||
      !pixels.readPixels(converted->pixmap(), 0, 0))
    return;

  auto impl = m_impl;
  m_impl->engine->post([impl, converted] {
#ifdef __APPLE__
    if (impl->gpuTexture) {
      UltralightMetalDriver::uploadToTexture(
          impl->gpuTexture, converted->getPixels(), converted->width(),
          converted->height(), converted->rowBytes());
      if (impl->source)
        impl->source->Invalidate();
      return;
    }
#endif
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
}

void WebImage::update(const sk_sp<SkImage> &rasterImage) {
  SkPixmap pixmap;
  if (rasterImage && rasterImage->peekPixels(&pixmap))
    update(pixmap);
}

void *WebImage::mtlTexture() const { return m_impl->gpuTexture; }

void WebImage::invalidate() {
  auto impl = m_impl;
  m_impl->engine->post([impl] {
    if (impl->source)
      impl->source->Invalidate();
  });
}

} // namespace ifrit::web
