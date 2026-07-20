#pragma once

#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class SkCanvas;
class SkPixmap;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::web {

class WebEngine;

/**
 * One offscreen web page ("tab") rendered by the WebEngine.
 *
 * In threaded engines every method is safe to call from any thread:
 * commands (loads, input, script) are marshalled to the web thread, and
 * frame() / frameVersion() read the latest published frame. In unthreaded
 * engines everything runs inline on the caller's thread.
 *
 * A published frame is an immutable raster SkImage (premultiplied BGRA,
 * sRGB) — draw it like any other image; a Graphite recorder uploads it on
 * first use. Holding a WebView keeps its WebEngine alive.
 */
class WebView {
public:
  /**
   * A published page snapshot — everything about the latest repaint in
   * one value (the acquire-latest-frame shape of Android's ImageReader
   * and CAMetalLayer, rather than separate accessors per fact).
   *
   * `image` is the frame as a drawable SkImage: always set on CPU
   * engines; on GPU engines it is set when frame() was given a recorder
   * to wrap `nativeTexture` for. Wraps are cached per version, so the
   * SkImage identity is stable across draws of the same frame and
   * Skia-side caches keyed on it stay warm.
   *
   * `dirtyBounds` is the page region that changed in this repaint (CEF
   * OnPaint-style), in frame pixels — full bounds when unknown.
   * Consumers that blend the frame over live content can use it to
   * limit their own repaint area.
   *
   * `version` increases by one per repaint and is 0 before the first
   * paint. A default-constructed Frame is falsy.
   */
  struct Frame {
    sk_sp<SkImage> image;
    void *nativeTexture = nullptr;
    int width = 0;
    int height = 0;
    SkIRect dirtyBounds = SkIRect::MakeEmpty();
    uint64_t version = 0;

    explicit operator bool() const {
      return image != nullptr || nativeTexture != nullptr;
    }
  };

  enum class MouseButton { None, Left, Middle, Right };

  ~WebView();

  int width() const;
  int height() const;

  /** Resizes the page layout viewport (in pixels). */
  void resize(int width, int height);

  /** Loads an HTML string as the main document. Relative resource paths
   *  resolve against WebEngineConfig::fileSystemDir. */
  void loadHTML(std::string html);

  /** Navigates to @p url — file:///, http(s)://, or data: . */
  void loadURL(std::string url);

  /**
   * Evaluates JavaScript in the page. If @p onResult is set it receives
   * the result (or the exception text) stringified, on the web thread.
   */
  void evaluateScript(std::string script,
                      std::function<void(std::string)> onResult = {});

  /** Fires on the web thread when the main frame finishes loading. */
  void setLoadCallback(std::function<void()> callback);

  /** Fires on the web thread after each repaint publishes a new frame.
   *  Use it to schedule a redraw of whatever composites this view. The
   *  Frame carries metadata and (CPU engines) the raster image; GPU
   *  consumers treat it as a signal and acquire via frame(recorder) on
   *  their own render thread. */
  void setFrameCallback(std::function<void(const Frame &)> callback);

  /**
   * Acquires the latest published frame. Falsy until the first repaint.
   *
   * On GPU engines pass the Graphite recorder you will draw with (it
   * must share the engine's device/queue) to get `image` populated with
   * a zero-copy, per-version-cached wrap of the frame texture; without
   * a recorder you still get `nativeTexture` + metadata. On CPU engines
   * the recorder is ignored and `image` is the raster frame.
   *
   * Call from the thread that owns @p recorder.
   */
  Frame frame(skgpu::graphite::Recorder *recorder = nullptr) const;

  /** Cheap poll: version of the latest published frame (CPU or GPU).
   *  Redraw consumers skip work while this hasn't moved. */
  uint64_t frameVersion() const;

  /** Draws the latest published frame scaled into @p dst. On GPU engines
   *  @p canvas must be Graphite-backed (its recorder wraps the frame
   *  texture). No-op before the first repaint. */
  void draw(SkCanvas &canvas, const SkRect &dst,
            const SkSamplingOptions &sampling = SkSamplingOptions(
                SkFilterMode::kLinear, SkMipmapMode::kNone)) const;

  /**
   * Zero-copy access to the live surface pixels (premultiplied BGRA).
   * Only valid on the web thread — i.e. from unthreaded-engine callers
   * between renderFrame() calls, or inside a frame callback — and only
   * until the next renderFrame()/resize. Returns false when unavailable.
   */
  bool peekPixels(SkPixmap *pixmap) const;

  /** Mouse input, in view pixels. `button` is the button held during the
   *  move, or the button pressed/released. */
  void mouseMove(int x, int y, MouseButton button = MouseButton::None);
  void mouseDown(int x, int y, MouseButton button = MouseButton::Left);
  void mouseUp(int x, int y, MouseButton button = MouseButton::Left);

  /** Scrolls by @p dx / @p dy pixels. */
  void scroll(int dx, int dy);

  class Impl;

private:
  WebView(std::shared_ptr<WebEngine> engine, std::shared_ptr<Impl> impl);

  std::shared_ptr<WebEngine> m_engine;
  std::shared_ptr<Impl> m_impl;

  friend class WebEngine;
};

} // namespace sigil::web
