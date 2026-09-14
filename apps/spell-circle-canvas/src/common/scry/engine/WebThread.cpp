/** @file
 * The web thread: its loop of tasks, updates and paced renders, the
 * post() and postAndWait() that marshal work onto it, the render pass
 * that publishes every dirty view, and the park an engine's end leaves
 * the runtime in — with the last pass that lets deferred GPU destroys
 * reach a live driver.
 */

#include <algorithm>
#include <chrono>
#include <string>

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

  // THE LOOP DOES NOT END. The renderer it drives is the process's and is
  // never released, so there is no teardown to run out to: an engine's
  // end parks the thread instead, and the next engine wakes it.
  std::unique_lock<std::mutex> lock(m_taskMutex);
  while (true) {
    while (!m_tasks.empty()) {
      auto task = std::move(m_tasks.front());
      m_tasks.pop_front();
      lock.unlock();
      task();
      lock.lock();
    }
    if (!m_open) {
      // PARKED. Nothing is pumped and nothing is published; the renderer
      // and every handler it was booted with stand exactly as they are.
      m_parked = true;
      m_taskCv.notify_all();
      m_taskCv.wait(lock, [this] { return !m_tasks.empty() || m_open; });
      m_parked = false;
      nextFrame = Clock::now();
      continue;
    }
    lock.unlock();

    m_renderer->Update();
    auto now = Clock::now();
    if (now >= nextFrame) {
      renderOnce();
      nextFrame = now + frameInterval;
    }

    lock.lock();
    m_taskCv.wait_until(lock, nextFrame,
                        [this] { return !m_tasks.empty() || !m_open; });
  }
}

void WebEngine::Impl::quiet() {
  m_renderer->Update();
  m_renderer->Render();
  m_renderer->PurgeMemory();
  if (m_gpuDriver) m_gpuDriver->flush();
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

void WebEngine::Impl::close() {
  if (!config.threaded) {
    m_views.clear();
    quiet();
    return;
  }
  if (!m_thread.joinable()) return;
  // The views go on the web thread, where the render pass also runs, and
  // the pass they were dropped from is the one that must reach the
  // driver — so both happen in the task, before the park.
  postAndWait([this] {
    m_views.clear();
    quiet();
  });
  {
    const std::lock_guard<std::mutex> lock(m_taskMutex);
    m_open = false;
  }
  m_taskCv.notify_all();
  // A caller returning from here knows nothing of its engine is still
  // running, which is what the join used to say. An engine released ON
  // the web thread — the last handle dropped inside a frame callback —
  // cannot wait for a park it is itself standing in the way of: it has
  // already been quieted above, and the thread parks as soon as the
  // callback returns.
  if (onWebThread()) return;
  std::unique_lock<std::mutex> lock(m_taskMutex);
  m_taskCv.wait(lock, [this] { return m_parked; });
}

bool WebEngine::Impl::reopen(WebEngineConfig next) {
  // WHAT BRING-UP FIXED IS THE PROCESS'S. The platform was handed these
  // roots, the renderer was created over this session store and this
  // thread, and the driver was built over this device; none of it can be
  // done again, so a configuration naming a different one is refused and
  // says which, rather than being silently answered with the first.
  const auto refuse = [this](const char* field) {
    m_logger->log(LogLevel::Error,
                  std::string("this process booted its renderer with a "
                              "different ") +
                      field +
                      "; that is fixed for the life of the process, so the "
                      "engine was not created");
    return false;
  };
  if (next.resourceDirectory != config.resourceDirectory)
    return refuse("resourceDirectory");
  if (next.fileSystemDirectory != config.fileSystemDirectory)
    return refuse("fileSystemDirectory");
  if (next.cachePath != config.cachePath) return refuse("cachePath");
  if (next.threaded != config.threaded) return refuse("threaded");
  if (next.gpuDevice != config.gpuDevice) return refuse("gpuDevice");
  if (next.graphite != config.graphite) return refuse("graphite");
  // An UNTHREADED runtime's web thread is whoever booted it, and the
  // renderer may only be driven from there.
  if (!config.threaded && !onWebThread())
    return refuse("thread to drive it from");

  // The rest is the engine's own and is simply taken: nothing was built
  // out of it. The runtime is parked here, so the logger's callback is
  // rewritten with nobody reading it.
  config.deviceScale = next.deviceScale;
  config.framesPerSecond = next.framesPerSecond;
  config.logCallback = std::move(next.logCallback);
  m_logger->setCallback(config.logCallback);
  if (config.threaded) {
    {
      const std::lock_guard<std::mutex> lock(m_taskMutex);
      m_open = true;
    }
    m_taskCv.notify_all();
  }
  return true;
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
