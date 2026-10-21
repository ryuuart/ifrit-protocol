#pragma once

/** @file
 * @ingroup sketch-window
 *
 * The render thread's half of a window that shows a sketch: the Graphite
 * context stood up on the device the window's QRhi renders with — or the
 * process's own painter's, where the window renders on that painter's
 * device and queue — the shared device lent that window's queue, a host's
 * captures routed through the same context, and the raster buffer drawn
 * into where no context stands.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

class QRhi;
class QRhiCommandBuffer;
class QRhiTexture;
class SkCanvas;

namespace sigil::core::hardware {
class GpuDevice;
}

namespace sigil::skia {
class GraphiteContext;
class PaintOrderCanvas;
}  // namespace sigil::skia

namespace sigil::sketch {

class Host;

/** HOW A WINDOW DRAWS A SKETCH INTO ITS OWN TEXTURE, on whichever path the
 *  window's backend allows: through Skia Graphite straight into the
 *  texture where the QRhi renders with Metal, and into a raster buffer
 *  uploaded into it everywhere else.
 *
 *  Every call is made on the thread that renders the window, the one Qt
 *  calls an item's renderer on. */
class WindowPainter {
 public:
  WindowPainter();
  ~WindowPainter();
  WindowPainter(const WindowPainter&) = delete;
  WindowPainter& operator=(const WindowPainter&) = delete;

  /** STANDS GRAPHITE UP ON THE DEVICE @p rhi RENDERS WITH, where that is
   *  Metal, and hands the process's shared device that queue and context
   *  to paint its pages through, so the window's frame and the pages
   *  share one set of atlases and pipelines. Where @p rhi renders on the
   *  process's own painter's device and queue (`sketch::ownPainter`), it
   *  draws through that painter's context instead, which outlives it.
   *  Whatever an earlier QRhi made is let go first: a replaced QRhi is a
   *  replaced device, and every image its context minted goes with it.
   *  Answers whether frames now draw on the device. */
  bool initialize(QRhi* rhi);
  /** Takes back from the shared device what `initialize` lent it, and
   *  lets the context go. */
  void release();
  /** The context frames draw through, or null on the raster path. */
  [[nodiscard]] skia::GraphiteContext* graphite() const;
  /** ROUTES @p host's CAPTURES THROUGH THIS CONTEXT. Once live frames draw
   *  on the device, a session's caches hold device images that cannot
   *  replay onto a raster canvas, so every host shown is handed this as
   *  it is presented. Nothing on the raster path. */
  void installCaptureBackend(Host& host);
  /** DRAWS INTO @p texture: @p paint is handed a canvas over it, the
   *  texture itself on the device path, or a raster buffer of the
   *  texture's size uploaded into it on @p commandBuffer once @p paint
   *  returns. False, with nothing drawn, where the texture could not be
   *  wrapped. */
  bool paint(QRhi* rhi, QRhiTexture* texture, QRhiCommandBuffer* commandBuffer,
             const std::function<void(SkCanvas&)>& paint);
  /** DRAWS INTO A RASTER BUFFER and uploads it into @p texture on
   *  @p commandBuffer: the path `paint` takes without a context, open to
   *  a window that wants it on purpose. */
  bool paintRaster(QRhi* rhi, QRhiTexture* texture,
                   QRhiCommandBuffer* commandBuffer,
                   const std::function<void(SkCanvas&)>& paint);

 private:
  /** The window's Metal device and queue, adopted, which the shared device
   *  paints its pages through while this painter stands. Declared before
   *  the context, so the context goes first. */
  std::unique_ptr<core::hardware::GpuDevice> m_windowDevice;
  std::unique_ptr<skia::GraphiteContext> m_graphiteContext;
  /** The process's own painter's context, drawn through and never let go
   *  here, where the QRhi renders on that painter's device and queue. */
  skia::GraphiteContext* m_borrowedContext = nullptr;
  /** Kept alive for the length of a capture: the fence a still on the
   *  device is described through. */
  std::unique_ptr<skia::PaintOrderCanvas> m_captureCanvas;
  std::vector<std::uint32_t> m_rasterPixels;
};

}  // namespace sigil::sketch
