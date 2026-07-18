#pragma once

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class SkCanvas;
class SkPixmap;
struct SkRect;

namespace skgpu::graphite {
class Recorder;
}

namespace ifrit::web {

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
  /** A published page snapshot. `version` increases by one per repaint;
   *  it is 0 before the first paint (with a null image). */
  struct Frame {
    sk_sp<SkImage> image;
    uint64_t version = 0;
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
   *  Use it to schedule a redraw of whatever composites this view. */
  void setFrameCallback(std::function<void(const Frame &)> callback);

  /** Latest published frame; image is null until the first repaint —
   *  and always null on GPU engines, whose frames are MTLTextures (use
   *  gpuFrame() / frameImage() / draw() there). */
  Frame frame() const;

  /** Cheap dirty check: version of the latest published frame (CPU or
   *  GPU). */
  uint64_t frameVersion() const;

  /** A published GPU frame (GPU engines only): the id<MTLTexture> the
   *  page was composited into, bridged to void*. Valid until two more
   *  publishes occur (frames ping-pong between two textures); wrap it
   *  promptly or use frameImage(), which manages lifetime for you. */
  struct GpuFrame {
    void *mtlTexture = nullptr;
    int width = 0;
    int height = 0;
    uint64_t version = 0;
  };
  GpuFrame gpuFrame() const;

  /**
   * The latest published frame as an SkImage usable with @p recorder's
   * Graphite context: on GPU engines this wraps the published MTLTexture
   * zero-copy (the image retains the texture; the engine and recorder
   * must share one Metal device/queue), on CPU engines it returns the
   * raster frame (recorder may be null there). Null before the first
   * repaint.
   */
  sk_sp<SkImage> frameImage(skgpu::graphite::Recorder *recorder) const;

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

} // namespace ifrit::web
