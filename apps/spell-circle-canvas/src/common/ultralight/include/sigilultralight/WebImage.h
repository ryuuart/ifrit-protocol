#pragma once

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>

#include <functional>
#include <memory>
#include <string>

class SkCanvas;
class SkPixmap;

namespace sigil::web {

class WebEngine;

/**
 * Skia content composited *into* web pages — the reverse direction of
 * WebView's output. Created via WebEngine::createImage(name, w, h);
 * reference it from HTML wherever an image URL is accepted:
 *
 *   <img src="name.imgsrc" />
 *
 * (The engine's FileSystem synthesizes the .imgsrc indirection Ultralight
 * expects; any path whose filename is "<name>.imgsrc" resolves to the
 * image registered under <name>.)
 *
 * Ways to supply pixels, safest first:
 *  - paint(painter): hands you an SkCanvas already targeting the image's
 *    pixels — a Graphite surface on the engine's own recorder (GPU) or
 *    the shared bitmap (CPU) — and handles the GPU flush and the
 *    invalidate in the same step. Mode-agnostic; nothing to forget.
 *  - update(pixmap) / update(rasterImage): copies raster pixels in and
 *    invalidates. Works on CPU and GPU engines alike.
 *  - updateTexture(texture): GPU engines — blit-copies a native texture
 *    (e.g. one another renderer produced) into the slot and invalidates.
 *  - GPU engines, expert path: render straight into nativeTexture() with
 *    your own Graphite recorder (must share the engine's device/queue),
 *    submit that work, then call invalidate() — in that order.
 *
 * Holding a WebImage keeps its WebEngine alive; destroying it
 * unregisters the name.
 */
class WebImage {
public:
  ~WebImage();

  const std::string &name() const;
  int width() const;
  int height() const;

  /**
   * Draws into the image via @p painter and republishes it — wrap,
   * flush, and invalidate handled internally, so partial updates can't
   * be observed and no step can be forgotten. Safe from any thread; the
   * callback runs on the web thread (blocking the caller until done), so
   * don't call other engine APIs that post-and-wait from inside it.
   * The canvas is not cleared first; returns false if the backend wrap
   * failed.
   */
  bool paint(const std::function<void(SkCanvas &)> &painter);

  /** Copies @p pixels (converted to premultiplied BGRA) into the image
   *  and invalidates it. Safe from any thread. False if the pixels could
   *  not be converted. */
  bool update(const SkPixmap &pixels);

  /**
   * Updates from an SkImage. Raster-backed images are copied in on any
   * engine. Texture-backed (Graphite) images are recorder-bound and
   * cannot be read from here — pass the underlying native texture to
   * updateTexture(), or draw via paint(); this overload logs a warning
   * and returns false for them.
   */
  bool update(const sk_sp<SkImage> &image);

  /**
   * GPU engines: blit-copies @p texture (a native texture handle on the
   * engine's device — id<MTLTexture> bridged to void* on Metal) into the
   * slot and invalidates it, on the web thread. The copy is clamped to
   * the smaller of the two sizes. Safe from any thread; the texture must
   * stay alive until this returns. False on CPU engines.
   */
  bool updateTexture(void *texture);

  /** GPU engines: the retained native texture backing this image
   *  (id<MTLTexture> bridged to void* on Metal), valid for the
   *  WebImage's lifetime. Null on CPU engines. After rendering into it,
   *  call invalidate(). */
  void *nativeTexture() const;

  /** Notifies pages displaying this image that it changed and should be
   *  redrawn (update() does this automatically). */
  void invalidate();

  class Impl;

private:
  WebImage(std::shared_ptr<WebEngine> engine, std::shared_ptr<Impl> impl);

  std::shared_ptr<WebEngine> m_engine;
  std::shared_ptr<Impl> m_impl;

  friend class WebEngine;
};

} // namespace sigil::web
