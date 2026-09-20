#pragma once

/** @file
 * @ingroup scry-engine
 * The WebEngine: Ultralight booted once per process, the thread that
 * drives it, and the factory for the views and image slots it renders.
 * WebEngineConfig fixes the engine's resources, threading, GPU device
 * and logging at creation — the first configuration fixes them for the
 * process, since the renderer they bring up is the only one there will
 * be.
 */

#include <sigilscry/platform/LogLevel.h>

#include <functional>
#include <memory>
#include <string>

namespace sigil::core::hardware {
class GpuDevice;
}  // namespace sigil::core::hardware

namespace sigil::skia {
class GraphiteContext;
}  // namespace sigil::skia

/** A HEADLESS WEB ENGINE WHOSE OUTPUT IS A SKIA IMAGE. HTML, CSS and
 *  JavaScript laid out and painted with no window anywhere, each view's
 *  pixels arriving as an image a canvas can draw. It presents nothing
 *  and owns no window; where the image goes is the caller's business. */
namespace sigil::scry {

class WebImage;
class WebView;

/** Engine-wide configuration, fixed at WebEngine::create() time. The
 *  defaults run out of the box: resources — ICU tables, CA certificates
 *  — come from the directory baked in at build time, and log output
 *  goes to stderr for warnings and errors. */
struct WebEngineConfig {
  /** Directory containing icudt67l.dat and cacert.pem. Empty uses the
   *  "resources" folder next to the executable, falling back to the SDK
   *  install location found at configure time. */
  std::string resourceDirectory;

  /** Base directory that file:/// URLs resolve against. */
  std::string fileSystemDirectory = ".";

  /** Writable directory for persistent session data — cookies, local
   *  storage. Empty keeps everything in memory. */
  std::string cachePath;

  /** Page-units-to-pixels scale applied to new views (2.0 for HiDPI
   *  output). Overridable per view. */
  double deviceScale = 1.0;

  /** Target cadence of the render thread. Ignored in unthreaded mode. */
  int framesPerSecond = 60;

  /** True, the default, gives the engine a dedicated web thread that
   *  pumps Ultralight and publishes frames, and every WebView call is
   *  then safe from any thread.
   *  @trap False hands the loop to the caller: create(), every WebView
   *  call, update() and renderFrame() must all happen on ONE thread. */
  bool threaded = true;

  /** Receives Ultralight log and console output plus engine diagnostics.
   *  Called from engine-internal threads (usually the web thread).
   *  Defaults to stderr for Error/Warning. */
  std::function<void(LogLevel, const std::string&)> logCallback;

  /** GPU rendering: the device the host draws with, adopted or owned.
   *  Set, views render through Ultralight's GPU pipeline into textures
   *  named by this device's handles; null, the CPU renderer publishes
   *  raster images. Driver bring-up that fails falls back to CPU with a
   *  logged warning.
   *  @trap The host keeps the device alive for the engine's lifetime. */
  sigil::core::hardware::GpuDevice* gpuDevice = nullptr;

  /** The Graphite context the engine's own drawing shares with the
   *  host. Null makes the engine create one of its own over
   *  `gpuDevice`, which stays correct and costs nothing but a second
   *  context; it is ignored without one.
   *  @trap A host that shares the context and uses it from its own
   *  thread makes every context call under lockContext() too. */
  sigil::skia::GraphiteContext* graphite = nullptr;
};

/** Per-view options for WebEngine::createView(). */
struct ViewOptions {
  /** Transparent background; pair it with `html,body{background:
   *  transparent}` in the page's own CSS. */
  bool transparent = true;

  /** Overrides WebEngineConfig::deviceScale when > 0. */
  double deviceScale = 0.0;
};

/**
 * Owns the Ultralight web renderer and the thread that drives it, so
 * each WebView's output is a premultiplied-BGRA SkImage ready to draw
 * onto any SkCanvas, raster or Graphite-backed. Views keep the engine
 * alive, so destruction order between handles is free.
 *
 * @trap Ultralight allows exactly ONE renderer per process and its
 * teardown cannot be run, so the runtime the first create() boots is
 * the process's and is never released; only one engine may be held at
 * a time, and the first configuration fixes what bring-up built.
 */
class WebEngine : public std::enable_shared_from_this<WebEngine> {
 public:
  ~WebEngine();

  /** Boots Ultralight (platform handlers, renderer, web thread) the
   *  first time, and stands the process's runtime back up every time
   *  after. Null while another engine is still held, when @p config
   *  names something the first bring-up fixed, and when bring-up
   *  fails. */
  static std::shared_ptr<WebEngine> create(WebEngineConfig config = {});

  /** Creates a view rendering a @p width x @p height page. Blocks briefly
   *  in threaded mode while the view is created on the web thread. */
  std::shared_ptr<WebView> createView(int width, int height,
                                      ViewOptions options = {});

  /** Registers a custom image pages display as
   *  `<img src="<name>.imgsrc">` — Skia content into web layouts.
   *  @p name must be unique per engine. */
  std::shared_ptr<WebImage> createImage(std::string name, int width,
                                        int height);

  /** Unthreaded mode only: dispatches Ultralight's timers, callbacks
   *  and network events. Call it at least once per frame, ideally more
   *  often. */
  void update();

  /** Unthreaded mode only: repaints dirty views and publishes their
   *  frames, answering whether any view actually repainted. */
  bool renderFrame();

  class Impl;

 private:
  explicit WebEngine(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> m_impl;

  friend class WebView;
};

}  // namespace sigil::scry
