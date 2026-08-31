/** @file
 * WebEngine's public surface: the one create() per process, the views
 * and image slots it makes on the web thread, and the update() and
 * renderFrame() an unthreaded host pumps.
 */

#include <atomic>
#include <cstdio>

#include "EngineImpl.h"
#include "ImageImpl.h"
#include "ViewImpl.h"

namespace sigil::scry {

namespace {

// Ultralight permits one Renderer per process for the program's lifetime.
std::atomic<bool> s_engineCreated{false};

}  // namespace

WebEngine::WebEngine(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}

WebEngine::~WebEngine() { m_impl->shutdown(); }

std::shared_ptr<WebEngine> WebEngine::create(WebEngineConfig config) {
  if (s_engineCreated.exchange(true)) {
    std::fprintf(stderr,
                 "[SigilScry:error] only one WebEngine may be created "
                 "per process\n");
    return nullptr;
  }

  auto impl = std::make_shared<Impl>();
  impl->config = std::move(config);
  if (!impl->start()) return nullptr;
  return std::shared_ptr<WebEngine>(new WebEngine(std::move(impl)));
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
    viewImpl->view = m_impl->ulRenderer().CreateView(
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
