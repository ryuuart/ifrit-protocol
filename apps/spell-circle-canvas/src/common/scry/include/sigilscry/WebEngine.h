#pragma once

#include <functional>
#include <memory>
#include <string>

namespace sigil::scry {

class WebImage;
class WebView;

/** Severity of an engine log message, mirroring ultralight::LogLevel. */
enum class LogLevel { Error, Warning, Info };

/**
 * Engine-wide configuration, fixed at WebEngine::create() time.
 *
 * The defaults run out of the box: resources (ICU tables, CA certs) come
 * from the directory baked in at build time by FindUltralight.cmake, and
 * log output goes to stderr for warnings and errors.
 */
struct WebEngineConfig {
  /** Directory containing icudt67l.dat and cacert.pem. When empty, the
   *  engine uses the "resources" folder next to the executable (staged
   *  by the ultralight_copy_resources() CMake function), falling back to
   *  the SDK install location found at configure time. */
  std::string resourceDir;

  /** Base directory that file:/// URLs resolve against. */
  std::string fileSystemDir = ".";

  /** Writable directory for persistent session data (cookies, local
   *  storage). Empty keeps everything in memory. */
  std::string cachePath;

  /** Page-units-to-pixels scale applied to new views (2.0 for HiDPI
   *  output). Overridable per view. */
  double deviceScale = 1.0;

  /** Target cadence of the render thread. Ignored in unthreaded mode. */
  int framesPerSecond = 60;

  /**
   * true (default): the engine owns a dedicated web thread that pumps
   * Ultralight and publishes frames; every WebView call is safe from any
   * thread.
   *
   * false: the caller owns the loop — create(), all WebView calls,
   * update(), and renderFrame() must all happen on one thread. Use this
   * to drive Ultralight in lockstep with your own render loop.
   */
  bool threaded = true;

  /** Receives Ultralight log and console output plus engine diagnostics.
   *  Called from engine-internal threads (usually the web thread).
   *  Defaults to stderr for Error/Warning. */
  std::function<void(LogLevel, const std::string &)> logCallback;

  /**
   * GPU rendering: native device and command-queue handles for this
   * platform's graphics API — pass the same pair the host's Graphite
   * context is built on so every draw rides one command queue. On Apple
   * these are an id<MTLDevice> / id<MTLCommandQueue> bridged to void*.
   * When both are set, views render through Ultralight's GPU pipeline
   * into native textures and publish texture-backed frames
   * (WebView::frame(recorder) wraps them zero-copy for a Graphite
   * recorder); when null, the CPU renderer publishes raster SkImages
   * instead. Falls back to CPU with a logged warning if driver bring-up
   * fails.
   *
   * The engine internals are backend-neutral (see WebGpuDriver): the
   * Windows/Linux ports add a Vulkan/D3D driver implementation and their
   * own handle fields here without touching the rest of the library.
   */
  void *metalDevice = nullptr;
  void *metalCommandQueue = nullptr;
};

/** Per-view options for WebEngine::createView(). */
struct ViewOptions {
  /** Transparent background (pair with `html,body{background:
   *  transparent}` in the page CSS). */
  bool transparent = true;

  /** Overrides WebEngineConfig::deviceScale when > 0. */
  double deviceScale = 0.0;
};

/**
 * Owns the Ultralight web renderer and the thread that drives it.
 *
 * Ultralight renders HTML/CSS/JS documents (full WebKit layout: flexbox,
 * grid, custom fonts, SVG, canvas, animations) into offscreen pixel
 * buffers on the CPU. This engine wraps it so each WebView's output is a
 * premultiplied-BGRA SkImage, ready to draw onto any SkCanvas — raster or
 * Graphite-backed — exactly like an sigil::image::ImageAsset frame.
 *
 * Integration paths, from least to most coupled:
 *  1. Pull: draw WebView::frame().image whenever you repaint, using
 *     WebView::frameVersion() to skip work when nothing changed.
 *  2. Push: WebView::setFrameCallback() fires on the web thread each time
 *     the page repaints — use it to schedule a redraw of your canvas.
 *  3. Lockstep: create the engine with threaded=false and call update() +
 *     renderFrame() from your own render loop; WebView::draw() then
 *     composites the freshly rendered surface in the same frame, and
 *     WebView::peekPixels() exposes the live surface with zero copies.
 *
 * Ultralight allows exactly one renderer per process, so create() may
 * only be called once for the lifetime of the program; it returns null on
 * a second call and when renderer bring-up fails. Views keep the engine
 * alive: destruction order between WebView and WebEngine handles is free.
 */
class WebEngine : public std::enable_shared_from_this<WebEngine> {
public:
  ~WebEngine();

  /** Boots Ultralight (platform handlers, renderer, web thread). Null on
   *  failure or when an engine was already created in this process. */
  static std::shared_ptr<WebEngine> create(WebEngineConfig config = {});

  /** Creates a view rendering a @p width x @p height page. Blocks briefly
   *  in threaded mode while the view is created on the web thread. */
  std::shared_ptr<WebView> createView(int width, int height,
                                      ViewOptions options = {});

  /**
   * Registers a custom image pages can display as
   * `<img src="<name>.imgsrc">` — the reverse compositing direction:
   * Skia content into web layouts. See WebImage for how to supply
   * pixels. @p name must be unique per engine.
   */
  std::shared_ptr<WebImage> createImage(std::string name, int width,
                                        int height);

  /**
   * Unthreaded mode only: dispatch Ultralight timers, callbacks, and
   * network events. Call at least once per frame, ideally more often.
   */
  void update();

  /**
   * Unthreaded mode only: repaint dirty views and publish their frames.
   * Returns true if any view actually repainted.
   */
  bool renderFrame();

  class Impl;

private:
  explicit WebEngine(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> m_impl;

  friend class WebView;
};

} // namespace sigil::scry
