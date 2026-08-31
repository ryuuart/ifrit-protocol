#pragma once

/** @file
 * WebEngine::Impl — the Ultralight renderer, the platform handlers it
 * was booted with, the web thread and its task queue, the views it
 * publishes and the GPU driver it renders through. Internal: it names
 * Ultralight types, and Ultralight is private to the library.
 */

#include <Ultralight/Ultralight.h>
#include <sigilskia/graphite/GraphiteContext.h>

#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "FileSystem.h"
#include "GpuDriver.h"
#include "Logger.h"
#include "SkiaSurface.h"
#include "sigilscry/engine/WebEngine.h"
#include "sigilscry/engine/WebView.h"

namespace sigil::scry {

class WebEngine::Impl {
 public:
  WebEngineConfig config;

  /** Spawns the web thread (threaded) or boots inline (unthreaded);
   *  false when platform/renderer bring-up fails. */
  bool start();
  void shutdown();

  bool onWebThread() const {
    return std::this_thread::get_id() == m_webThreadId;
  }

  /** Runs @p task on the web thread: queued in threaded mode, inline when
   *  already on the web thread. */
  void post(std::function<void()> task);
  void postAndWait(std::function<void()> task);

  /** Web thread only. */
  ultralight::Renderer& ulRenderer() { return *m_renderer; }
  void registerView(std::weak_ptr<WebView::Impl> view);
  bool renderOnce();
  void pump() { m_renderer->Update(); }

  CallbackLogger* logger() { return m_logger.get(); }

  GpuDriver* gpuDriver() { return m_gpuDriver.get(); }
  bool gpuEnabled() const { return m_gpuDriver != nullptr; }

 private:
  /** Hands the platform its handlers, picks the GPU driver for this
   *  device, and creates the renderer; web thread. */
  bool setupPlatform();
  void threadMain(std::promise<bool>& ready);

  std::thread m_thread;
  std::thread::id m_webThreadId;

  std::mutex m_taskMutex;
  std::condition_variable m_taskCv;
  std::deque<std::function<void()>> m_tasks;
  bool m_running = true;

  ultralight::RefPtr<ultralight::Renderer> m_renderer;  // web thread only
  std::vector<std::weak_ptr<WebView::Impl>> m_views;    // web thread only

  std::unique_ptr<PrefixFileSystem> m_fileSystem;
  std::unique_ptr<CallbackLogger> m_logger;
  std::unique_ptr<SkiaSurfaceFactory> m_surfaceFactory;
  // The engine's own Graphite context when the host shared none; declared
  // before the driver, whose web-thread recorder it must outlive.
  std::unique_ptr<sigil::skia::GraphiteContext> m_ownedGraphite;
  std::unique_ptr<GpuDriver> m_gpuDriver;
};

}  // namespace sigil::scry
