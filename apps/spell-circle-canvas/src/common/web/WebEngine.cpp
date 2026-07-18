#include "WebInternal.h"

#ifdef __APPLE__
#include "UltralightMetalDriver.h"
#endif

#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/Platform.h>

#include <chrono>

namespace ifrit::web {

namespace {

// Ultralight permits one Renderer per process for the program's lifetime.
std::atomic<bool> s_engineCreated{false};

} // namespace

bool WebEngine::Impl::setupPlatform() {
  // Resolution order: explicit config, the resources/ folder staged next
  // to the executable by ultralight_copy_resources(), then the SDK
  // install location found at configure time.
  std::string resourceDir = config.resourceDir;
  if (resourceDir.empty())
    resourceDir = executableAdjacentResourceDir();
  if (resourceDir.empty())
    resourceDir = IFRIT_WEB_DEFAULT_RESOURCE_DIR;

  m_logger = std::make_unique<CallbackLogger>(config.logCallback);
  m_fileSystem = std::make_unique<PrefixFileSystem>(
      resourceDir, config.fileSystemDir, m_logger.get());
  m_surfaceFactory = std::make_unique<SkiaSurfaceFactory>();

  ultralight::Config ulConfig;
  ulConfig.cache_path = config.cachePath.c_str();

  ultralight::Platform &platform = ultralight::Platform::instance();
  platform.set_config(ulConfig);
  platform.set_logger(m_logger.get());
  platform.set_file_system(m_fileSystem.get());
  platform.set_font_loader(ultralight::GetPlatformFontLoader());
  platform.set_surface_factory(m_surfaceFactory.get());

  // The only backend-specific seam: pick the WebGpuDriver implementation
  // for this platform. Everything downstream sees the neutral interface.
  if (config.metalDevice && config.metalCommandQueue) {
#ifdef __APPLE__
    m_gpuDriver = UltralightMetalDriver::create(config.metalDevice,
                                                config.metalCommandQueue);
#endif
    if (m_gpuDriver)
      platform.set_gpu_driver(m_gpuDriver.get());
    else
      m_logger->log(LogLevel::Warning,
                    "GPU driver bring-up failed (no backend for this "
                    "platform yet, or shader compile failed); falling back "
                    "to the CPU renderer");
  }

  // Touch the ImageSourceProvider singleton before the renderer exists:
  // it is destroyed in reverse construction order, and engine teardown
  // (WebCore MemoryCache cleanup) locks its mutex — constructed lazily on
  // first AddImageSource it would die before an engine held in a static.
  ultralight::ImageSourceProvider::instance();

  m_renderer = ultralight::Renderer::Create();
  if (!m_renderer) {
    m_logger->log(LogLevel::Error, "ultralight::Renderer::Create() failed");
    return false;
  }
  return true;
}

void WebEngine::Impl::threadMain(std::promise<bool> &ready) {
  m_webThreadId = std::this_thread::get_id();
  bool ok = setupPlatform();
  ready.set_value(ok);
  if (!ok)
    return;

  using Clock = std::chrono::steady_clock;
  const auto frameInterval = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(1.0 /
                                    std::max(1, config.framesPerSecond)));
  auto nextFrame = Clock::now();

  std::unique_lock<std::mutex> lock(m_taskMutex);
  while (true) {
    while (!m_tasks.empty()) {
      auto task = std::move(m_tasks.front());
      m_tasks.pop_front();
      lock.unlock();
      task();
      lock.lock();
    }
    if (!m_running)
      break;
    lock.unlock();

    m_renderer->Update();
    auto now = Clock::now();
    if (now >= nextFrame) {
      renderOnce();
      nextFrame = now + frameInterval;
    }

    lock.lock();
    m_taskCv.wait_until(lock, nextFrame,
                        [this] { return !m_tasks.empty() || !m_running; });
  }

  // Drain shutdown tasks (view teardown) before the renderer goes away.
  while (!m_tasks.empty()) {
    auto task = std::move(m_tasks.front());
    m_tasks.pop_front();
    lock.unlock();
    task();
    lock.lock();
  }
  lock.unlock();

  m_views.clear();
  // Destroyed views defer their GPU resource teardown to the next
  // Render(); give the renderer one so those Destroy* calls reach the
  // driver while it's still alive, and purge caches so WebCore's
  // thread-local FontCache doesn't hold GPU glyph textures into pthread
  // TSD cleanup.
  m_renderer->Update();
  m_renderer->Render();
  m_renderer->PurgeMemory();
  if (m_gpuDriver)
    m_gpuDriver->flush();
  m_renderer = nullptr;
}

bool WebEngine::Impl::start() {
  if (config.threaded) {
    std::promise<bool> ready;
    std::future<bool> readyFuture = ready.get_future();
    m_thread = std::thread([this, &ready] { threadMain(ready); });
    bool ok = readyFuture.get();
    if (!ok) {
      m_thread.join();
      m_thread = std::thread();
    }
    return ok;
  }

  m_webThreadId = std::this_thread::get_id();
  return setupPlatform();
}

void WebEngine::Impl::shutdown() {
  if (config.threaded) {
    if (!m_thread.joinable())
      return;
    {
      std::lock_guard<std::mutex> lock(m_taskMutex);
      m_running = false;
    }
    m_taskCv.notify_all();
    m_thread.join();
    return;
  }

  m_views.clear();
  m_renderer = nullptr;
}

void WebEngine::Impl::post(std::function<void()> task) {
  if (onWebThread() || !config.threaded) {
    task();
    return;
  }
  {
    std::lock_guard<std::mutex> lock(m_taskMutex);
    m_tasks.push_back(std::move(task));
  }
  m_taskCv.notify_all();
}

void WebEngine::Impl::postAndWait(std::function<void()> task) {
  if (onWebThread() || !config.threaded) {
    task();
    return;
  }
  std::promise<void> done;
  std::future<void> doneFuture = done.get_future();
  post([&task, &done] {
    task();
    done.set_value();
  });
  doneFuture.get();
}

void WebEngine::Impl::registerView(std::weak_ptr<WebView::Impl> view) {
  m_views.push_back(std::move(view));
}

bool WebEngine::Impl::renderOnce() {
  m_renderer->RefreshDisplay(0);
  m_renderer->Render();

  if (m_gpuDriver) {
    std::unordered_set<uint32_t> dirty = m_gpuDriver->flush();
    bool published = false;
    for (auto it = m_views.begin(); it != m_views.end();) {
      if (std::shared_ptr<WebView::Impl> view = it->lock()) {
        published = view->publishGpuIfDirty(*m_gpuDriver, dirty) || published;
        ++it;
      } else {
        it = m_views.erase(it);
      }
    }
    return published;
  }

  bool published = false;
  for (auto it = m_views.begin(); it != m_views.end();) {
    if (std::shared_ptr<WebView::Impl> view = it->lock()) {
      published = view->publishIfDirty() || published;
      ++it;
    } else {
      it = m_views.erase(it);
    }
  }
  return published;
}

WebEngine::WebEngine(std::shared_ptr<Impl> impl) : m_impl(std::move(impl)) {}

WebEngine::~WebEngine() { m_impl->shutdown(); }

std::shared_ptr<WebEngine> WebEngine::create(WebEngineConfig config) {
  if (s_engineCreated.exchange(true)) {
    std::fprintf(stderr, "[IfritWeb:error] only one WebEngine may be created "
                         "per process\n");
    return nullptr;
  }

  auto impl = std::make_shared<Impl>();
  impl->config = std::move(config);
  if (!impl->start())
    return nullptr;
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
        static_cast<uint32_t>(width), static_cast<uint32_t>(height),
        viewConfig, nullptr);
    viewImpl->view->set_load_listener(viewImpl.get());
    viewImpl->view->set_view_listener(viewImpl.get());
    m_impl->registerView(viewImpl);
  });

  if (!viewImpl->view)
    return nullptr;
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
    if (WebGpuDriver *driver = m_impl->gpuDriver()) {
      imageImpl->gpuTexture = driver->createImageTexture(width, height);
      imageImpl->gpuTextureId =
          driver->registerExternalTexture(imageImpl->gpuTexture);
      imageImpl->source = ultralight::ImageSource::CreateFromTexture(
          static_cast<uint32_t>(width), static_cast<uint32_t>(height),
          imageImpl->gpuTextureId,
          ultralight::Rect{0.0f, 0.0f, 1.0f, 1.0f});
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

  return std::shared_ptr<WebImage>(
      new WebImage(shared_from_this(), imageImpl));
}

void WebEngine::update() {
  if (m_impl->config.threaded)
    return;
  m_impl->pump();
}

bool WebEngine::renderFrame() {
  if (m_impl->config.threaded)
    return false;
  return m_impl->renderOnce();
}

} // namespace ifrit::web
