#pragma once

/** @file
 * @ingroup scry-engine
 * The WebImage: a named slot a page displays as `<img src="name.imgsrc">`
 * and native code fills — painted through an SkCanvas, copied from
 * raster pixels, or blitted from a texture on the engine's device.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRefCnt.h>
#include <sigilcore/hardware/Handle.h>

#include <functional>
#include <memory>
#include <string>

class SkCanvas;
class SkPixmap;

namespace sigil::scry {

class WebEngine;

/**
 * Skia content composited *into* web pages — the reverse direction of
 * WebView's output. WebEngine::createImage registers one under a name,
 * and a page displays it as `<img src="name.imgsrc" />`: any path whose
 * filename is `<name>.imgsrc` resolves to the image registered under
 * `<name>`. Holding one keeps its WebEngine alive, and destroying it
 * unregisters the name.
 *
 * Pixels arrive through paint(), update() or updateTexture(), in that
 * order of safety.
 */
class WebImage {
 public:
  ~WebImage();

  /** The name a page addresses this image by. */
  const std::string& name() const;
  /** The image's width in pixels. */
  int width() const;
  /** The image's height in pixels. */
  int height() const;

  /**
   * Draws into the image via @p painter and republishes it — the wrap,
   * the flush and the invalidate handled internally, so a partial
   * update cannot be observed. Safe from any thread, and the canvas is
   * not cleared first. False if the backend wrap failed.
   * @trap The callback runs on the web thread, blocking the caller, so
   * nothing inside it may call an engine door that posts and waits.
   */
  bool paint(const std::function<void(SkCanvas&)>& painter);

  /** Copies @p pixels, converted to premultiplied BGRA, into the image
   *  and invalidates it. Safe from any thread; false if the pixels
   *  could not be converted. */
  bool update(const SkPixmap& pixels);

  /**
   * Updates from an SkImage, copying a raster-backed one in on any
   * engine.
   * @silent @p image is texture-backed: a Graphite image is
   * recorder-bound and cannot be read from here, so this warns and
   * answers false. Pass its texture to updateTexture(), or draw through
   * paint().
   */
  bool update(const sk_sp<SkImage>& image);

  /**
   * GPU engines: blit-copies @p texture, named on the engine's
   * GpuDevice, into the slot and invalidates it, on the web thread. The
   * copy is clamped to the smaller of the two sizes, and it is safe
   * from any thread. False on a CPU engine or for a stale handle.
   * @trap @p texture must stay alive until this returns.
   */
  bool updateTexture(sigil::core::hardware::TextureHandle texture);

  /** GPU engines: the texture backing this image, named on the engine's
   *  GpuDevice and valid for the WebImage's lifetime; `exportNative`
   *  there hands the native object out. Null on CPU engines.
   *  @trap Rendering into it directly means submitting that work and
   *  then calling invalidate(), in that order. */
  sigil::core::hardware::TextureHandle texture() const;

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

}  // namespace sigil::scry
