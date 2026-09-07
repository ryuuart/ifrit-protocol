/** @file
 * The web thread: its loop of tasks, updates and paced renders, the
 * post() and postAndWait() that marshal work onto it, the render pass
 * that publishes every dirty view, and the teardown order that lets
 * deferred GPU destroys reach a live driver.
 */

#include <algorithm>
#include <chrono>

#include "EngineImpl.h"
#include "ViewImpl.h"

namespace sigil::scry {

void WebEngine::Impl::threadMain(std::promise<bool>& ready) {
  m_webThreadId = std::this_thread::get_id();
  bool ok = setupPlatform();
  ready.set_value(ok);
  if (!ok) return;

  using Clock = std::chrono::steady_clock;
  const auto frameInterval = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(1.0 / std::max(1, config.framesPerSecond)));
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
    if (!m_running) break;
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
  if (m_gpuDriver) m_gpuDriver->flush();
  m_renderer = nullptr;
}

bool WebEngine::Impl::start() {
  if (config.threaded) {
    std::promise<bool> ready;
    std::future<bool> readyFuture = ready.get_future();
    m_thread = std::thread([this, &ready] { threadMain(ready); });
    const bool ok = readyFuture.get();
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
    if (!m_thread.joinable()) return;
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

void WebEngine::Impl::forgetView(const WebView::Impl* view) {
  std::erase_if(m_views, [view](const std::weak_ptr<WebView::Impl>& entry) {
    const std::shared_ptr<WebView::Impl> live = entry.lock();
    return !live || live.get() == view;
  });
}

bool WebEngine::Impl::renderOnce() {
  m_renderer->RefreshDisplay(0);
  m_renderer->Render();

  // The pass holds the pages it is about to publish and walks that, not
  // the registry: publishing calls the consumer's frame callback on this
  // thread, and a callback that opens a page reaches registerView() while
  // one that releases a WebView reaches forgetView() — either of which
  // moves the registry under an iterator standing in it.
  std::vector<std::shared_ptr<WebView::Impl>> pages;
  pages.reserve(m_views.size());
  for (auto it = m_views.begin(); it != m_views.end();) {
    if (std::shared_ptr<WebView::Impl> page = it->lock()) {
      pages.push_back(std::move(page));
      ++it;
    } else {
      it = m_views.erase(it);
    }
  }

  bool published = false;
  if (m_gpuDriver) {
    const boost::unordered_flat_set<uint32_t> dirty = m_gpuDriver->flush();
    for (const std::shared_ptr<WebView::Impl>& page : pages)
      published = page->publishGpuIfDirty(*m_gpuDriver, dirty) || published;
    return published;
  }

  for (const std::shared_ptr<WebView::Impl>& page : pages)
    published = page->publishIfDirty() || published;
  return published;
}

}  // namespace sigil::scry
