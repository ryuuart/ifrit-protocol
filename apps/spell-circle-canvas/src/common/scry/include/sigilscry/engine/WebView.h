#pragma once

/** @file
 * @ingroup scry-engine
 * The WebView: one offscreen page, its input, its script, and the
 * frames it publishes — each repaint an immutable snapshot with a
 * version, a raster image on CPU engines or a texture on GPU ones.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <sigilcore/hardware/Handle.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

class SkCanvas;
class SkPixmap;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::scry {

class WebEngine;

/**
 * One offscreen web page ("tab") rendered by the WebEngine, publishing
 * an immutable premultiplied-BGRA snapshot per repaint. Holding one
 * keeps its WebEngine alive.
 *
 * In threaded engines every method is safe from any thread: commands
 * are marshalled to the web thread, and frame() and frameVersion() read
 * the latest published frame. In unthreaded engines everything runs
 * inline on the caller's thread.
 */
class WebView {
 public:
  /**
   * A published page snapshot — everything about the latest repaint in
   * one value. `image` is the drawable frame, always set on CPU engines
   * and on GPU ones when frame() was given a recorder to wrap the
   * texture for. `dirtyBounds` is the page region that changed, in
   * frame pixels, and the full bounds when unknown. `version` increases
   * by one per repaint and is 0 before the first paint, so a
   * default-constructed Frame is falsy.
   */
  struct Frame {
    sk_sp<SkImage> image;
    /** GPU engines: the published texture, named on the engine's
     *  GpuDevice; `exportNative` there hands the native object out.
     *  @trap Stale once the view republishes at a new size — the wrap
     *  in `image` is what keeps a frame's texture alive. */
    sigil::core::hardware::TextureHandle texture;
    int width = 0;
    int height = 0;
    SkIRect dirtyBounds = SkIRect::MakeEmpty();
    uint64_t version = 0;

    /** Whether the view has painted anything yet. */
    explicit operator bool() const { return image != nullptr || bool(texture); }
  };

  /** Which button an input event carries. `None` is the only meaningful
   *  value for a move with nothing held. */
  enum class MouseButton { None, Left, Middle, Right };

  ~WebView();

  /** The layout viewport's width in pixels. */
  int width() const;
  /** The layout viewport's height in pixels. */
  int height() const;

  /** Resizes the page layout viewport (in pixels). */
  void resize(int width, int height);

  /** Loads an HTML string as the main document. Relative resource paths
   *  resolve against WebEngineConfig::fileSystemDirectory. */
  void loadHTML(std::string html);

  /** Navigates to @p url — file:///, http(s)://, or data: . */
  void loadURL(std::string url);

  /** Evaluates JavaScript in the page. @p onResult receives the result,
   *  or the exception text, stringified, on the web thread. */
  void evaluateScript(std::string script,
                      std::function<void(std::string)> onResult = {});

  /** Fires on the web thread when the main frame finishes loading. */
  void setLoadCallback(std::function<void()> callback);

  /** Fires on the web thread after each repaint publishes a new frame
   *  — what schedules a redraw of whatever composites this view. The
   *  Frame carries the metadata and, on CPU engines, the raster image;
   *  a GPU consumer treats it as a signal and acquires through
   *  frame(recorder) on its own render thread. */
  void setFrameCallback(std::function<void(const Frame&)> callback);

  /** Fires on the web thread at the END of every pass the engine makes
   *  over its pages — published or not — carrying how many passes it
   *  has made. It is what a page's STILLNESS is counted in, so a
   *  machine that runs the engine slowly and one that runs it fast call
   *  the same page still on the same repaint.
   *  @trap The count is the ENGINE's own tick, the same number for
   *  every view over it. */
  void setRenderPassCallback(
      std::function<void(uint64_t renderPasses)> callback);

  /**
   * Acquires the latest published frame, falsy until the first repaint.
   * On GPU engines @p recorder — over the engine's device, on its
   * shared context or another over the same queue — is what populates
   * `image` with a zero-copy, per-version-cached wrap; without one you
   * still get `texture` and the metadata. On CPU engines it is ignored.
   * @trap Call it from the thread that owns @p recorder.
   */
  Frame frame(skgpu::graphite::Recorder* recorder = nullptr) const;

  /** Cheap poll: version of the latest published frame (CPU or GPU).
   *  Redraw consumers skip work while this hasn't moved. */
  uint64_t frameVersion() const;

  /** Draws the latest published frame scaled into @p dst. On GPU engines
   *  @p canvas must be Graphite-backed (its recorder wraps the frame
   *  texture). No-op before the first repaint. */
  void draw(SkCanvas& canvas, const SkRect& dst,
            const SkSamplingOptions& sampling = SkSamplingOptions(
                SkFilterMode::kLinear, SkMipmapMode::kNone)) const;

  /**
   * Zero-copy access to the LIVE surface pixels, premultiplied BGRA.
   * False when unavailable.
   * @trap Valid only on the web thread — from an unthreaded engine's
   * caller between renderFrame() calls, or inside a frame callback —
   * and only until the next renderFrame() or resize.
   */
  bool peekPixels(SkPixmap* pixmap) const;

  /** Mouse input, in view pixels. `button` is the button held during the
   *  move, or the button pressed/released. */
  void mouseMove(int x, int y, MouseButton button = MouseButton::None);
  /** A press at @p x, @p y. The page sees no click until the matching
   *  `mouseUp` arrives. */
  void mouseDown(int x, int y, MouseButton button = MouseButton::Left);
  /** The release that completes a press. */
  void mouseUp(int x, int y, MouseButton button = MouseButton::Left);

  /** A wheel of @p dx / @p dy pixels, exactly as an input device
   *  delivers one.
   *  @trap THE DELTA IS WHAT THE CONTENT MOVES BY, not where the
   *  viewport goes, so walking DOWN a page is NEGATIVE: `scroll(0,
   *  -120)` lifts the content and shows what stood below it. Negative
   *  `dx` is the same sideways, revealing what stood to the right. */
  void scroll(int dx, int dy);

  class Impl;

 private:
  WebView(std::shared_ptr<WebEngine> engine, std::shared_ptr<Impl> impl);

  std::shared_ptr<WebEngine> m_engine;
  std::shared_ptr<Impl> m_impl;

  friend class WebEngine;
};

}  // namespace sigil::scry
