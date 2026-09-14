/** @file
 * WebEngine's public surface: the process's one runtime and the engines
 * leased over it, the views and image slots they make on the web thread,
 * and the update() and renderFrame() an unthreaded host pumps.
 */

#include <cstdio>
#include <mutex>

#include "EngineImpl.h"
#include "ImageImpl.h"
#include "ViewImpl.h"

namespace sigil::scry {

namespace {

/** The lock over both of the values below. Never destroyed, for the same
 *  reason the runtime is not: an engine held in a static outlives it
 *  otherwise, and its release would lock a mutex that had been torn
 *  down. */
std::mutex& runtimeMutex() {
  static auto* one = new std::mutex();
  return *one;
}

/** THE PROCESS'S RUNTIME: the web thread, the platform handlers and the
 *  one renderer Ultralight allows, booted by the first engine and handed
 *  to every engine after it.
 *
 *  It is never released, and that is the point. The renderer's own
 *  teardown frees the JavaScript VM and deletes WebCore's resource-usage
 *  singleton while the thread that polls both is still running, and that
 *  thread has no exit a host can reach: an engine that released its
 *  renderer left a live thread reading freed memory, which the next
 *  unrelated work in the process died on. Held through a pointer that is
 *  never deleted so that static destruction does not release it either.
 *
 *  workaround: WebCore's resource-usage thread is started when a page
 *  first loads and is never joined, so releasing the renderer it reads
 *  through is a use-after-free with no API to prevent it. */
std::shared_ptr<WebEngine::Impl>& runtime() {
  static auto* one = new std::shared_ptr<WebEngine::Impl>();
  return *one;
}

/** Whether an engine currently stands over that runtime. Two at once
 *  would each end it under the other. */
bool s_leased = false;

}  // namespace

WebEngine::WebEngine(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}

WebEngine::~WebEngine() {
  m_impl->close();
  const std::lock_guard<std::mutex> lock(runtimeMutex());
  s_leased = false;
}

std::shared_ptr<WebEngine> WebEngine::create(WebEngineConfig config) {
  const std::lock_guard<std::mutex> lock(runtimeMutex());
  if (s_leased) {
    std::fprintf(stderr,
                 "[SigilScry:error] an engine already stands over this "
                 "process's renderer; release it before creating another\n");
    return nullptr;
  }
  std::shared_ptr<Impl>& kept = runtime();
  if (kept) {
    if (!kept->reopen(std::move(config))) return nullptr;
  } else {
    auto impl = std::make_shared<Impl>();
    impl->config = std::move(config);
    // Bring-up that fails creates no renderer, so the next call may try
    // again; one that succeeds is the process's from here on.
    if (!impl->start()) return nullptr;
    kept = std::move(impl);
  }
  s_leased = true;
  return std::shared_ptr<WebEngine>(new WebEngine(kept));
}

std::shared_ptr<WebView> WebEngine::createView(int width, int height,
                                               ViewOptions options) {
  auto viewImpl = std::make_shared<WebView::Impl>();
  viewImpl->engine = m_impl.get();
  viewImpl->width = width;
  viewImpl->height = height;

  m_impl->postAndWait([this, viewImpl, width, height, options] {
    ultralight::ViewConfig viewConfig;
    viewConfig.is_accelerated = m_impl->gpuEnabled();
    viewConfig.is_transparent = options.transparent;
    viewConfig.initial_device_scale = options.deviceScale > 0.0
                                          ? options.deviceScale
                                          : m_impl->config.deviceScale;
    viewImpl->view = m_impl->ultralightRenderer().CreateView(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), viewConfig,
        nullptr);
    viewImpl->view->set_load_listener(viewImpl.get());
    viewImpl->view->set_view_listener(viewImpl.get());
    m_impl->registerView(viewImpl);
  });

  if (!viewImpl->view) return nullptr;
  return std::shared_ptr<WebView>(new WebView(shared_from_this(), viewImpl));
}

std::shared_ptr<WebImage> WebEngine::createImage(std::string name, int width,
                                                 int height) {
  auto imageImpl = std::make_shared<WebImage::Impl>();
  imageImpl->engine = m_impl.get();
  imageImpl->name = std::move(name);
  imageImpl->width = width;
  imageImpl->height = height;

  m_impl->postAndWait([this, imageImpl, width, height] {
    if (GpuDriver* driver = m_impl->gpuDriver()) {
      imageImpl->gpuTexture = driver->createImageTexture(width, height);
      imageImpl->gpuTextureId =
          driver->registerExternalTexture(imageImpl->gpuTexture);
      imageImpl->source = ultralight::ImageSource::CreateFromTexture(
          static_cast<uint32_t>(width), static_cast<uint32_t>(height),
          imageImpl->gpuTextureId, ultralight::Rect{0.0f, 0.0f, 1.0f, 1.0f});
    }
    if (!imageImpl->source) {
      imageImpl->bitmap = ultralight::Bitmap::Create(
          static_cast<uint32_t>(width), static_cast<uint32_t>(height),
          ultralight::BitmapFormat::BGRA8_UNORM_SRGB);
      imageImpl->bitmap->Erase();
      imageImpl->source =
          ultralight::ImageSource::CreateFromBitmap(imageImpl->bitmap);
    }
    ultralight::ImageSourceProvider::instance().AddImageSource(
        imageImpl->name.c_str(), imageImpl->source);
  });

  return std::shared_ptr<WebImage>(new WebImage(shared_from_this(), imageImpl));
}

void WebEngine::update() {
  if (m_impl->config.threaded) return;
  m_impl->pump();
}

bool WebEngine::renderFrame() {
  if (m_impl->config.threaded) return false;
  return m_impl->renderOnce();
}

}  // namespace sigil::scry
