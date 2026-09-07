/** @file
 * WebView's public surface: every command marshalled to the web thread
 * with the impl travelling in the task, the latest frame acquired under
 * its lock and wrapped once per version for a recorder, and the live
 * surface peeked at from the web thread.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRect.h>

#include "EngineImpl.h"
#include "Utf8.h"
#include "ViewImpl.h"

namespace sigil::scry {

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

}  // namespace

WebView::WebView(std::shared_ptr<WebEngine> engine, std::shared_ptr<Impl> impl)
    : m_engine(std::move(engine)), m_impl(std::move(impl)) {}

WebView::~WebView() {
  // Tear the ultralight::View down on the web thread; the impl travels
  // with the task so it outlives any in-flight callbacks. The engine is
  // told to forget the page in the same breath as it is destroyed: the
  // impl can outlive this task, and an impl with no page in the publish
  // list is a page the render pass would dereference. Publish textures
  // are safe to release here: wrapped SkImages hold their own retains
  // (see GpuDriver::wrapTexture).
  auto impl = m_impl;
  m_impl->engine->post([impl] {
    if (impl->view) {
      impl->view->set_load_listener(nullptr);
      impl->view->set_view_listener(nullptr);
      impl->view = nullptr;
    }
    impl->engine->forgetView(impl.get());
    // …and calls nobody back: a consumer's callbacks were written for a
    // WebView that no longer exists, and the frames still in flight
    // would reach whatever it captured.
    impl->frameCallback = nullptr;
    impl->loadCallback = nullptr;
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
    if (impl->view) impl->view->LoadHTML(html.c_str(), "file:///");
  });
}

void WebView::loadURL(std::string url) {
  auto impl = m_impl;
  m_impl->engine->post([impl, url = std::move(url)] {
    if (impl->view) impl->view->LoadURL(url.c_str());
  });
}

void WebView::evaluateScript(std::string script,
                             std::function<void(std::string)> onResult) {
  auto impl = m_impl;
  m_impl->engine->post(
      [impl, script = std::move(script), onResult = std::move(onResult)] {
        if (!impl->view) return;
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

void WebView::setFrameCallback(std::function<void(const Frame&)> callback) {
  auto impl = m_impl;
  m_impl->engine->post([impl, callback = std::move(callback)]() mutable {
    impl->frameCallback = std::move(callback);
  });
}

WebView::Frame WebView::frame(skgpu::graphite::Recorder* recorder) const {
  std::lock_guard<std::mutex> lock(m_impl->frameMutex);
  Frame result;
  result.version = m_impl->version.load();
  result.dirtyBounds = m_impl->lastDirtyBounds;

  if (m_impl->publishedGpuTexture) {
    result.texture = m_impl->publishedGpuTexture;
    result.width = m_impl->gpuTextureWidth;
    result.height = m_impl->gpuTextureHeight;
    if (recorder) {
      if (!m_impl->cachedWrap || m_impl->cachedWrapVersion != result.version ||
          m_impl->cachedWrapRecorder != recorder) {
        m_impl->cachedWrap = m_impl->engine->gpuDriver()->wrapTexture(
            recorder, m_impl->publishedGpuTexture, result.width, result.height);
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

void WebView::draw(SkCanvas& canvas, const SkRect& dst,
                   const SkSamplingOptions& sampling) const {
  Frame current = frame(canvas.recorder());
  if (!current.image) return;
  canvas.drawImageRect(current.image, dst, sampling);
}

bool WebView::peekPixels(SkPixmap* pixmap) const {
  if (!m_impl->engine->onWebThread() || !m_impl->view) return false;
  auto* surface = static_cast<SkiaSurface*>(m_impl->view->surface());
  if (!surface) return false;
  return surface->bitmap().peekPixels(pixmap);
}

void WebView::mouseMove(int x, int y, MouseButton button) {
  auto impl = m_impl;
  m_impl->engine->post([impl, x, y, button] {
    if (!impl->view) return;
    ultralight::MouseEvent event{ultralight::MouseEvent::kType_MouseMoved, x, y,
                                 toUlButton(button)};
    impl->view->FireMouseEvent(event);
  });
}

void WebView::mouseDown(int x, int y, MouseButton button) {
  auto impl = m_impl;
  m_impl->engine->post([impl, x, y, button] {
    if (!impl->view) return;
    ultralight::MouseEvent event{ultralight::MouseEvent::kType_MouseDown, x, y,
                                 toUlButton(button)};
    impl->view->FireMouseEvent(event);
  });
}

void WebView::mouseUp(int x, int y, MouseButton button) {
  auto impl = m_impl;
  m_impl->engine->post([impl, x, y, button] {
    if (!impl->view) return;
    ultralight::MouseEvent event{ultralight::MouseEvent::kType_MouseUp, x, y,
                                 toUlButton(button)};
    impl->view->FireMouseEvent(event);
  });
}

void WebView::scroll(int dx, int dy) {
  auto impl = m_impl;
  m_impl->engine->post([impl, dx, dy] {
    if (!impl->view) return;
    ultralight::ScrollEvent event{ultralight::ScrollEvent::kType_ScrollByPixel,
                                  dx, dy};
    impl->view->FireScrollEvent(event);
  });
}

}  // namespace sigil::scry
