#pragma once
// Internal plumbing shared by WebEngine.cpp / WebView.cpp — not installed.

#include "ifritweb/WebEngine.h"
#include "ifritweb/WebImage.h"
#include "ifritweb/WebView.h"

#include <Ultralight/AppCore/Platform.h>
#include <Ultralight/Ultralight.h>

#include "WebGpuDriver.h"

#include <include/core/SkBitmap.h>
#include <include/core/SkData.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace ifrit::web {

inline std::string toUtf8(const ultralight::String &str) {
  const ultralight::String8 &utf8 = str.utf8();
  return std::string(utf8.data(), utf8.length());
}

/** The "resources" directory next to the running executable, if it holds
 *  the Ultralight runtime data (ultralight_copy_resources() stages it
 *  there at build time); empty otherwise. */
std::string executableAdjacentResourceDir();

/**
 * ultralight::Surface whose pixel store is an SkBitmap, so Ultralight's
 * CPU renderer paints directly into memory Skia can read without a
 * conversion step. Pixels are premultiplied BGRA in sRGB — exactly what
 * Ultralight writes and what the scene pipeline draws.
 */
class SkiaSurface final : public ultralight::Surface {
public:
  SkiaSurface(uint32_t width, uint32_t height) { Resize(width, height); }

  uint32_t width() const override { return m_bitmap.width(); }
  uint32_t height() const override { return m_bitmap.height(); }
  uint32_t row_bytes() const override {
    return static_cast<uint32_t>(m_bitmap.rowBytes());
  }
  size_t size() const override { return m_bitmap.computeByteSize(); }

  void *LockPixels() override { return m_bitmap.getPixels(); }
  void UnlockPixels() override {}

  void Resize(uint32_t width, uint32_t height) override;

  const SkBitmap &bitmap() const { return m_bitmap; }

private:
  SkBitmap m_bitmap;
};

class SkiaSurfaceFactory final : public ultralight::SurfaceFactory {
public:
  ultralight::Surface *CreateSurface(uint32_t width, uint32_t height) override {
    return new SkiaSurface(width, height);
  }
  void DestroySurface(ultralight::Surface *surface) override { delete surface; }
};

/**
 * Disk file system with two roots: paths under Ultralight's
 * resource_path_prefix ("resources/") map into the SDK resource dir
 * (ICU data, CA certs), everything else resolves against the
 * user-facing base dir for file:/// content.
 */
class CallbackLogger;

class PrefixFileSystem final : public ultralight::FileSystem {
public:
  PrefixFileSystem(std::string resourceDir, std::string baseDir,
                   CallbackLogger *logger)
      : m_resourceDir(std::move(resourceDir)), m_baseDir(std::move(baseDir)),
        m_logger(logger) {}

  bool FileExists(const ultralight::String &path) override;
  ultralight::String GetFileMimeType(const ultralight::String &path) override;
  ultralight::String GetFileCharset(const ultralight::String &path) override;
  ultralight::RefPtr<ultralight::Buffer>
  OpenFile(const ultralight::String &path) override;

private:
  std::string resolve(const ultralight::String &path) const;

  std::string m_resourceDir;
  std::string m_baseDir;
  CallbackLogger *m_logger;
};

class CallbackLogger final : public ultralight::Logger {
public:
  explicit CallbackLogger(
      std::function<void(LogLevel, const std::string &)> callback)
      : m_callback(std::move(callback)) {}

  void LogMessage(ultralight::LogLevel level,
                  const ultralight::String &message) override;

  void log(LogLevel level, const std::string &message);

private:
  std::function<void(LogLevel, const std::string &)> m_callback;
};

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
  ultralight::Renderer &ulRenderer() { return *m_renderer; }
  void registerView(std::weak_ptr<WebView::Impl> view);
  bool renderOnce();
  void pump() { m_renderer->Update(); }

  CallbackLogger *logger() { return m_logger.get(); }

  WebGpuDriver *gpuDriver() { return m_gpuDriver.get(); }
  bool gpuEnabled() const { return m_gpuDriver != nullptr; }

private:
  bool setupPlatform();
  void threadMain(std::promise<bool> &ready);

  std::thread m_thread;
  std::thread::id m_webThreadId;

  std::mutex m_taskMutex;
  std::condition_variable m_taskCv;
  std::deque<std::function<void()>> m_tasks;
  bool m_running = true;

  ultralight::RefPtr<ultralight::Renderer> m_renderer; // web thread only
  std::vector<std::weak_ptr<WebView::Impl>> m_views;   // web thread only

  std::unique_ptr<PrefixFileSystem> m_fileSystem;
  std::unique_ptr<CallbackLogger> m_logger;
  std::unique_ptr<SkiaSurfaceFactory> m_surfaceFactory;
  std::unique_ptr<WebGpuDriver> m_gpuDriver;
};

class WebImage::Impl {
public:
  WebEngine::Impl *engine = nullptr;
  std::string name;
  int width = 0;
  int height = 0;

  // Web-thread-only Ultralight state.
  ultralight::RefPtr<ultralight::ImageSource> source;
  ultralight::RefPtr<ultralight::Bitmap> bitmap; // CPU engines

  // GPU engines: immutable after creation, readable from any thread.
  void *gpuTexture = nullptr; // retained id<MTLTexture>
  uint32_t gpuTextureId = 0;
};

class WebView::Impl final : public ultralight::LoadListener,
                            public ultralight::ViewListener {
public:
  ~Impl() override = default;

  WebEngine::Impl *engine = nullptr;
  ultralight::RefPtr<ultralight::View> view; // web thread only
  std::atomic<int> width{0};
  std::atomic<int> height{0};

  // Latest published frame, readable from any thread. CPU engines fill
  // latestImage; GPU engines ping-pong the two retained native textures
  // and expose publishedGpuTexture.
  mutable std::mutex frameMutex;
  sk_sp<SkImage> latestImage;
  void *publishedGpuTexture = nullptr;
  void *spareGpuTexture = nullptr;
  int gpuTextureWidth = 0;
  int gpuTextureHeight = 0;
  SkIRect lastDirtyBounds = SkIRect::MakeEmpty();
  std::atomic<uint64_t> version{0};

  // Per-version cache of the Graphite wrap handed out by frame(): keeps
  // the SkImage identity stable across draws of one frame (Skia caches
  // key on it) and makes repeat acquisition free. Guarded by frameMutex;
  // the wrap itself happens on the recorder owner's thread.
  mutable sk_sp<SkImage> cachedWrap;
  mutable uint64_t cachedWrapVersion = 0;
  mutable skgpu::graphite::Recorder *cachedWrapRecorder = nullptr;

  // CPU publish buffers, recycled once consumers release the SkImages
  // that reference them (web thread only).
  std::vector<sk_sp<SkData>> publishPool;

  // Web-thread-only state (set via posted tasks, invoked on the web thread).
  std::function<void(const WebView::Frame &)> frameCallback;
  std::function<void()> loadCallback;

  /** Web thread: snapshots the surface into an immutable SkImage if the
   *  page repainted since the last publish. Returns true on publish. */
  bool publishIfDirty();

  /** Web thread: blit-copies the view's render target into the spare
   *  publish texture and swaps it in, when this view's render buffer is
   *  in @p dirtyRenderBuffers. Returns true on publish. */
  bool publishGpuIfDirty(WebGpuDriver &driver,
                         const std::unordered_set<uint32_t> &dirtyRenderBuffers);

  /** Web thread: releases the ping-pong publish textures. */
  void releaseGpuTextures();

  // ultralight::LoadListener
  void OnFinishLoading(ultralight::View *caller, uint64_t frameId,
                       bool isMainFrame, const ultralight::String &url) override;

  // ultralight::ViewListener
  void OnAddConsoleMessage(ultralight::View *caller,
                           const ultralight::ConsoleMessage &message) override;
};

} // namespace ifrit::web
