#include "WebInternal.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>

#ifdef __APPLE__
#include <include/gpu/graphite/BackendTexture.h>
#include <include/gpu/graphite/Image.h>
#include <include/gpu/graphite/mtl/MtlGraphiteTypes_cpp.h>

#include <CoreFoundation/CoreFoundation.h>
#endif

#include <cstring>

namespace ifrit::web {

namespace {

ultralight::MouseEvent::Button toUlButton(WebView::MouseButton button) {
  switch (button) {
  case WebView::MouseButton::Left:
    return ultralight::MouseEvent::kButton_Left;
  case WebView::MouseButton::Middle:
    return ultralight::MouseEvent::kButton_Middle;
  case WebView::MouseButton::Right:
    return ultralight::MouseEvent::kButton_Right;
  case WebView::MouseButton::None:
    break;
  }
  return ultralight::MouseEvent::kButton_None;
}

} // namespace

bool WebView::Impl::publishIfDirty() {
  auto *surface = static_cast<SkiaSurface *>(view->surface());
  if (!surface || surface->dirty_bounds().IsEmpty())
    return false;

  // Snapshot into a fresh immutable bitmap so consumers on other threads
  // can hold the SkImage for as long as they like while Ultralight keeps
  // painting into the live surface.
  const SkBitmap &src = surface->bitmap();
  SkBitmap copy;
  copy.allocPixels(src.info(), src.rowBytes());
  std::memcpy(copy.getPixels(), src.getPixels(), src.computeByteSize());
  copy.setImmutable();

  sk_sp<SkImage> image = copy.asImage();
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    latestImage = image;
  }
  uint64_t newVersion = ++version;
  surface->ClearDirtyBounds();

  if (frameCallback)
    frameCallback({std::move(image), newVersion});
  return true;
}

#ifdef __APPLE__

bool WebView::Impl::publishGpuIfDirty(
    UltralightMetalDriver &driver,
    const std::unordered_set<uint32_t> &dirtyRenderBuffers) {
  ultralight::RenderTarget target = view->render_target();
  if (target.is_empty || !dirtyRenderBuffers.count(target.render_buffer_id))
    return false;

  const int frameWidth = static_cast<int>(target.width);
  const int frameHeight = static_cast<int>(target.height);
  if (gpuTextureWidth != frameWidth || gpuTextureHeight != frameHeight) {
    releaseGpuTextures();
    publishedGpuTexture = driver.createPublishTexture(frameWidth, frameHeight);
    spareGpuTexture = driver.createPublishTexture(frameWidth, frameHeight);
    gpuTextureWidth = frameWidth;
    gpuTextureHeight = frameHeight;
  }

  driver.copyTexture(target.texture_id, spareGpuTexture, frameWidth,
                     frameHeight);
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    std::swap(publishedGpuTexture, spareGpuTexture);
  }
  uint64_t newVersion = ++version;

  if (frameCallback)
    frameCallback({nullptr, newVersion});
  return true;
}

void WebView::Impl::releaseGpuTextures() {
  std::lock_guard<std::mutex> lock(frameMutex);
  UltralightMetalDriver::releaseTexture(publishedGpuTexture);
  UltralightMetalDriver::releaseTexture(spareGpuTexture);
  publishedGpuTexture = nullptr;
  spareGpuTexture = nullptr;
  gpuTextureWidth = 0;
  gpuTextureHeight = 0;
}

#endif // __APPLE__

void WebView::Impl::OnFinishLoading(ultralight::View *, uint64_t,
                                    bool isMainFrame,
                                    const ultralight::String &) {
  if (isMainFrame && loadCallback)
    loadCallback();
}

void WebView::Impl::OnAddConsoleMessage(
    ultralight::View *, const ultralight::ConsoleMessage &message) {
  LogLevel level = LogLevel::Info;
  if (message.level() == ultralight::MessageLevel::kMessageLevel_Error)
    level = LogLevel::Error;
  else if (message.level() == ultralight::MessageLevel::kMessageLevel_Warning)
    level = LogLevel::Warning;
  engine->logger()->log(level, "console: " + toUtf8(message.message()));
}

WebView::WebView(std::shared_ptr<WebEngine> engine, std::shared_ptr<Impl> impl)
    : m_engine(std::move(engine)), m_impl(std::move(impl)) {}

WebView::~WebView() {
  // Tear the ultralight::View down on the web thread; the impl travels
  // with the task so it outlives any in-flight callbacks. Publish
  // textures are safe to release here: wrapped SkImages hold their own
  // retains (see frameImage).
  auto impl = m_impl;
  m_impl->engine->post([impl] {
    if (impl->view) {
      impl->view->set_load_listener(nullptr);
      impl->view->set_view_listener(nullptr);
      impl->view = nullptr;
    }
#ifdef __APPLE__
    impl->releaseGpuTextures();
#endif
  });
}

int WebView::width() const { return m_impl->width.load(); }
int WebView::height() const { return m_impl->height.load(); }

void WebView::resize(int width, int height) {
  m_impl->width = width;
  m_impl->height = height;
  auto impl = m_impl;
  m_impl->engine->post([impl, width, height] {
    if (impl->view)
      impl->view->Resize(static_cast<uint32_t>(width),
                         static_cast<uint32_t>(height));
  });
}

void WebView::loadHTML(std::string html) {
  auto impl = m_impl;
  m_impl->engine->post([impl, html = std::move(html)] {
    // A file:/// base URL lets the page load relative resources (images,
    // .imgsrc indirections, fonts) through the engine's FileSystem; the
    // default about:blank origin is blocked from file content entirely.
    if (impl->view)
      impl->view->LoadHTML(html.c_str(), "file:///");
  });
}

void WebView::loadURL(std::string url) {
  auto impl = m_impl;
  m_impl->engine->post([impl, url = std::move(url)] {
    if (impl->view)
      impl->view->LoadURL(url.c_str());
  });
}

void WebView::evaluateScript(std::string script,
                             std::function<void(std::string)> onResult) {
  auto impl = m_impl;
  m_impl->engine->post(
      [impl, script = std::move(script), onResult = std::move(onResult)] {
        if (!impl->view)
          return;
        ultralight::String exception;
        ultralight::String result =
            impl->view->EvaluateScript(script.c_str(), &exception);
        if (onResult)
          onResult(exception.empty() ? toUtf8(result) : toUtf8(exception));
      });
}

void WebView::setLoadCallback(std::function<void()> callback) {
  auto impl = m_impl;
  m_impl->engine->post([impl, callback = std::move(callback)]() mutable {
    impl->loadCallback = std::move(callback);
  });
}

void WebView::setFrameCallback(std::function<void(const Frame &)> callback) {
  auto impl = m_impl;
  m_impl->engine->post([impl, callback = std::move(callback)]() mutable {
    impl->frameCallback = std::move(callback);
  });
}

WebView::Frame WebView::frame() const {
  std::lock_guard<std::mutex> lock(m_impl->frameMutex);
  return {m_impl->latestImage, m_impl->version.load()};
}

uint64_t WebView::frameVersion() const { return m_impl->version.load(); }

WebView::GpuFrame WebView::gpuFrame() const {
  std::lock_guard<std::mutex> lock(m_impl->frameMutex);
  if (!m_impl->publishedGpuTexture)
    return {};
  return {m_impl->publishedGpuTexture, m_impl->gpuTextureWidth,
          m_impl->gpuTextureHeight, m_impl->version.load()};
}

sk_sp<SkImage> WebView::frameImage(skgpu::graphite::Recorder *recorder) const {
#ifdef __APPLE__
  {
    std::lock_guard<std::mutex> lock(m_impl->frameMutex);
    if (m_impl->publishedGpuTexture) {
      if (!recorder)
        return nullptr;
      // The wrapped image retains the MTLTexture so it stays valid even
      // if the view resizes or is destroyed while the image is alive.
      CFTypeRef texture =
          static_cast<CFTypeRef>(m_impl->publishedGpuTexture);
      CFRetain(texture);
      skgpu::graphite::BackendTexture backendTexture =
          skgpu::graphite::BackendTextures::MakeMetal(
              SkISize::Make(m_impl->gpuTextureWidth,
                            m_impl->gpuTextureHeight),
              texture);
      return SkImages::WrapTexture(
          recorder, backendTexture, kPremul_SkAlphaType,
          SkColorSpace::MakeSRGB(),
          [](void *context) { CFRelease(static_cast<CFTypeRef>(context)); },
          const_cast<void *>(static_cast<const void *>(texture)));
    }
  }
#else
  (void)recorder;
#endif
  return frame().image;
}

void WebView::draw(SkCanvas &canvas, const SkRect &dst,
                   const SkSamplingOptions &sampling) const {
  sk_sp<SkImage> image = frameImage(canvas.recorder());
  if (!image)
    return;
  canvas.drawImageRect(image, dst, sampling);
}

bool WebView::peekPixels(SkPixmap *pixmap) const {
  if (!m_impl->engine->onWebThread() || !m_impl->view)
    return false;
  auto *surface = static_cast<SkiaSurface *>(m_impl->view->surface());
  if (!surface)
    return false;
  return surface->bitmap().peekPixels(pixmap);
}

void WebView::mouseMove(int x, int y, MouseButton button) {
  auto impl = m_impl;
  m_impl->engine->post([impl, x, y, button] {
    if (!impl->view)
      return;
    ultralight::MouseEvent event{ultralight::MouseEvent::kType_MouseMoved, x,
                                 y, toUlButton(button)};
    impl->view->FireMouseEvent(event);
  });
}

void WebView::mouseDown(int x, int y, MouseButton button) {
  auto impl = m_impl;
  m_impl->engine->post([impl, x, y, button] {
    if (!impl->view)
      return;
    ultralight::MouseEvent event{ultralight::MouseEvent::kType_MouseDown, x, y,
                                 toUlButton(button)};
    impl->view->FireMouseEvent(event);
  });
}

void WebView::mouseUp(int x, int y, MouseButton button) {
  auto impl = m_impl;
  m_impl->engine->post([impl, x, y, button] {
    if (!impl->view)
      return;
    ultralight::MouseEvent event{ultralight::MouseEvent::kType_MouseUp, x, y,
                                 toUlButton(button)};
    impl->view->FireMouseEvent(event);
  });
}

void WebView::scroll(int dx, int dy) {
  auto impl = m_impl;
  m_impl->engine->post([impl, dx, dy] {
    if (!impl->view)
      return;
    ultralight::ScrollEvent event{ultralight::ScrollEvent::kType_ScrollByPixel,
                                  dx, dy};
    impl->view->FireScrollEvent(event);
  });
}

} // namespace ifrit::web
