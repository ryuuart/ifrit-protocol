#pragma once

/** @file
 * The render thread's half of the live canvas: the renderer behind the
 * item, the Graphite context it draws the presented session through, and
 * the raster path that stands beside it.
 */

#include <sigilmotion/clock/FrameClock.h>

#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtQuick/QQuickRhiItem>
#include <cstdint>
#include <memory>
#include <vector>

class SkCanvas;
class SkPixmap;
class SkSurface;
class SketchbookView;

namespace sigil::sketch {
class Host;
}

namespace sigil::skia {
class GraphiteContext;
class PaintOrderCanvas;
}  // namespace sigil::skia

class SketchbookRenderer final : public QQuickRhiItemRenderer {
 public:
  /** Declared out of line so the Graphite handles below stay forward
   *  declarations: nothing that only creates a renderer is made to see
   *  the whole of Skia's device surface. */
  SketchbookRenderer();
  void initialize(QRhiCommandBuffer* commandBuffer) override;
  void synchronize(QQuickRhiItem* item) override;
  void render(QRhiCommandBuffer* commandBuffer) override;
  ~SketchbookRenderer() override;

 private:
  void drawSketch(SkCanvas& canvas, QSize pixelSize);
  void runPendingCaptures();  // hostMutex must be held
  void refreshThumbnail();    // hostMutex must be held
  /** hostMutex must be held. Hands back the session evicted to make
   *  room, for the caller to let go of once the lock is released:
   *  ~Host waits on the build it may be in the middle of. */
  [[nodiscard]] std::unique_ptr<sigil::sketch::Host> openSketch(int index);
  void publishMetrics();  // hostMutex must be held
  /** Routes a session's captures through this renderer's own context.
   *  Once live frames render on the device, the runtime's caches hold
   *  device-backed images that cannot replay onto a raster canvas, so
   *  every session gets this as it is built rather than only the first. */
  void installCaptureBackend(sigil::sketch::Host& host);

#ifdef SIGILSKETCH_BOOK_GPU
  bool readbackGraphite(SkSurface& surface, const SkPixmap& out);
  std::unique_ptr<sigil::skia::GraphiteContext> m_graphiteContext;
  /** Kept alive for the length of a capture: the fence the still on a
   *  device is described through. */
  std::unique_ptr<sigil::skia::PaintOrderCanvas> m_captureCanvas;
#endif
  SketchbookView* m_view = nullptr;
  QRhi* m_rhi = nullptr;
  bool m_initialized = false;
  std::vector<uint32_t> m_rasterPixels;
  /** THE ITEM'S OWN RECTANGLE, in its own units and not rounded to
   *  them: what the texture will be stretched over, which is only the
   *  same shape as the texture while the render size is settled on it. */
  QSizeF m_logicalSize;
  /** THE DENSITY A SKETCH'S CACHED RASTERS ARE BAKED AT: the screen's,
   *  and not the viewport's. A sketch declares a canvas and this window
   *  magnifies it, so a raster taken at the screen's density is the
   *  picture of that canvas — taken once, and blitted through the zoom
   *  the way a bitmap the sketch loaded would be. Read off the window
   *  rather than off the frame's own scale, which is what a zoom moves. */
  float m_deviceRatio = 1.0f;
  int m_requestedIndex = 0;
  int m_index = -1;
  int m_pendingCaptures = 0;
  int m_frameCount = 0;
  /** How far into its own clock the presented session has run, and
   *  whether it has already been photographed for the thumbnail store.
   *  Both start over when a sketch is opened. */
  double m_sceneSeconds = 0.0;
  bool m_thumbnailTaken = false;
  bool m_paused = false;
  bool m_metricsDirty = true;
  double m_timeScale = 1.0;
  double m_submitMsAverage = 0.0;
  /** The clock the presented frame advances by. The pause and the time
   *  scale are the view's published properties, pushed into it on every
   *  synchronize; what the clock adds on top is the stall clamp, so
   *  dragging the window or stopping in a debugger resumes at the next
   *  frame instead of jumping every animation forward by the length of
   *  the pause. */
  sigil::motion::FrameClock m_clock;
};
