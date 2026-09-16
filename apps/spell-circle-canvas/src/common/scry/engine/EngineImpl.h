#pragma once

/** @file
 * WebEngine::Impl — the Ultralight renderer, the platform handlers it
 * was booted with, the web thread and its task queue, the views it
 * publishes and the GPU driver it renders through. One of these is the
 * PROCESS's and outlives every engine handed out over it. Internal: it
 * names Ultralight types, and Ultralight is private to the library.
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
   *  false when platform/renderer bring-up fails. Runs ONCE per process:
   *  the renderer it creates is the only one Ultralight allows. */
  bool start();
  /** Ends the engine standing over this runtime: the views go, the
   *  renderer is given the last update, render and purge its deferred
   *  destroys need, and the web thread parks. The renderer and the
   *  thread stay — see the note over `create()`. */
  void close();
  /** Stands the runtime up again for @p next. False when @p next names
   *  something bring-up fixed for the process — the resource roots, the
   *  session store, the threading, the device — which is stated rather
   *  than quietly ignored. */
  bool reopen(WebEngineConfig next);

  bool onWebThread() const {
    return std::this_thread::get_id() == m_webThreadId;
  }

  /** Runs @p task on the web thread: queued in threaded mode, inline when
   *  already on the web thread. */
  void post(std::function<void()> task);
  void postAndWait(std::function<void()> task);

  /** Web thread only. */
  ultralight::Renderer& ultralightRenderer() { return *m_renderer; }
  void registerView(std::weak_ptr<WebView::Impl> view);
  /** Drops @p view from the publish list. A page that has been torn down
   *  is nothing to publish, and its impl outlives the teardown for as
   *  long as anything still holds it. */
  void forgetView(const WebView::Impl* view);
  bool renderOnce();
  void pump() { m_renderer->Update(); }
  /** Web thread: the last pass a closing engine owes its renderer.
   *  Destroyed views defer their GPU resource teardown to the next
   *  Render(), so one more reaches the driver while it is still alive,
   *  and the purge keeps WebCore's thread-local font cache from holding
   *  GPU glyph textures. */
  void quiet();

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
  /** Whether an engine stands over this runtime. A closed runtime parks:
   *  it holds its renderer and pumps nothing until the next engine. */
  bool m_open = true;
  /** …and whether the web thread has reached that park, which is what
   *  `close()` waits for so a caller that returns from it knows nothing
   *  of its engine is still running. */
  bool m_parked = false;

  ultralight::RefPtr<ultralight::Renderer> m_renderer;  // web thread only
  std::vector<std::weak_ptr<WebView::Impl>> m_views;    // web thread only
  // How many passes over the pages this runtime has made — the engine's
  // own tick, which every page's stillness is counted in. Web thread only.
  uint64_t m_renderPasses = 0;

  std::unique_ptr<PrefixFileSystem> m_fileSystem;
  std::unique_ptr<CallbackLogger> m_logger;
  std::unique_ptr<SkiaSurfaceFactory> m_surfaceFactory;
  // The engine's own Graphite context when the host shared none; declared
  // before the driver, whose web-thread recorder it must outlive.
  std::unique_ptr<sigil::skia::GraphiteContext> m_ownedGraphite;
  std::unique_ptr<GpuDriver> m_gpuDriver;
};

}  // namespace sigil::scry
