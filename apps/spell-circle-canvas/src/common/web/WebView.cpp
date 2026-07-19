#include "WebInternal.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>

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

  ultralight::IntRect dirty = surface->dirty_bounds();
  SkIRect dirtyBounds =
      SkIRect::MakeLTRB(dirty.left, dirty.top, dirty.right, dirty.bottom);

  // Snapshot into an immutable buffer so consumers on other threads can
  // hold the SkImage for as long as they like while Ultralight keeps
  // painting into the live surface. Buffers are pooled: once every
  // consumer releases a frame, its allocation is reused instead of
  // paying a fresh multi-megabyte alloc (and its page faults) per
  // publish.
  const SkBitmap &src = surface->bitmap();
  const size_t byteSize = src.computeByteSize();
  sk_sp<SkData> buffer;
  for (auto it = publishPool.begin(); it != publishPool.end(); ++it) {
    if ((*it)->size() == byteSize && (*it)->unique()) {
      // Take the pool's ref so the buffer is uniquely ours while writing
      // (SkData::writable_data requires it).
      buffer = std::move(*it);
      publishPool.erase(it);
      break;
    }
  }
  std::erase_if(publishPool, [byteSize](const sk_sp<SkData> &pooled) {
    return pooled->size() != byteSize && pooled->unique(); // post-resize
  });
  if (!buffer)
    buffer = SkData::MakeUninitialized(byteSize);
  std::memcpy(buffer->writable_data(), src.getPixels(), byteSize);

  sk_sp<SkImage> image =
      SkImages::RasterFromData(src.info(), buffer, src.rowBytes());
  if (publishPool.size() < 2)
    publishPool.push_back(std::move(buffer));
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    latestImage = image;
    lastDirtyBounds = dirtyBounds;
  }
  uint64_t newVersion = ++version;
  surface->ClearDirtyBounds();

  if (frameCallback)
    frameCallback({std::move(image), nullptr, src.width(), src.height(),
                   dirtyBounds, newVersion});
  return true;
}

bool WebView::Impl::publishGpuIfDirty(
    WebGpuDriver &driver,
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
  SkIRect dirtyBounds = SkIRect::MakeWH(frameWidth, frameHeight);
  {
    std::lock_guard<std::mutex> lock(frameMutex);
    std::swap(publishedGpuTexture, spareGpuTexture);
    lastDirtyBounds = dirtyBounds;
  }
  uint64_t newVersion = ++version;

  if (frameCallback)
    frameCallback({nullptr, publishedGpuTexture, frameWidth, frameHeight,
                   dirtyBounds, newVersion});
  return true;
}

void WebView::Impl::releaseGpuTextures() {
  std::lock_guard<std::mutex> lock(frameMutex);
  if (WebGpuDriver *driver = engine->gpuDriver()) {
    driver->releaseNativeTexture(publishedGpuTexture);
    driver->releaseNativeTexture(spareGpuTexture);
  }
  publishedGpuTexture = nullptr;
  spareGpuTexture = nullptr;
  gpuTextureWidth = 0;
  gpuTextureHeight = 0;
  cachedWrap = nullptr; // the wrap itself keeps its texture alive
  cachedWrapVersion = 0;
  cachedWrapRecorder = nullptr;
}

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
  // retains (see WebGpuDriver::wrapTexture).
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

WebView::Frame WebView::frame(skgpu::graphite::Recorder *recorder) const {
  std::lock_guard<std::mutex> lock(m_impl->frameMutex);
  Frame result;
  result.version = m_impl->version.load();
  result.dirtyBounds = m_impl->lastDirtyBounds;

  if (m_impl->publishedGpuTexture) {
    result.nativeTexture = m_impl->publishedGpuTexture;
    result.width = m_impl->gpuTextureWidth;
    result.height = m_impl->gpuTextureHeight;
    if (recorder) {
      if (!m_impl->cachedWrap ||
          m_impl->cachedWrapVersion != result.version ||
          m_impl->cachedWrapRecorder != recorder) {
        m_impl->cachedWrap = m_impl->engine->gpuDriver()->wrapTexture(
            recorder, m_impl->publishedGpuTexture, result.width,
            result.height);
        m_impl->cachedWrapVersion = result.version;
        m_impl->cachedWrapRecorder = recorder;
      }
      result.image = m_impl->cachedWrap;
    }
    return result;
  }

  result.image = m_impl->latestImage;
  if (result.image) {
    result.width = result.image->width();
    result.height = result.image->height();
  }
  return result;
}

uint64_t WebView::frameVersion() const { return m_impl->version.load(); }

void WebView::draw(SkCanvas &canvas, const SkRect &dst,
                   const SkSamplingOptions &sampling) const {
  Frame current = frame(canvas.recorder());
  if (!current.image)
    return;
  canvas.drawImageRect(current.image, dst, sampling);
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
