#pragma once

/** @file
 * The render thread's half of the live canvas: the renderer behind the
 * item, the Graphite context it draws the presented session through, and
 * the raster path that stands beside it.
 */

#include <include/core/SkRefCnt.h>
#include <sigilmotion/clock/FrameClock.h>

#include <QtCore/QSize>
#include <QtCore/QSizeF>
#include <QtQuick/QQuickRhiItem>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <vector>

class SkCanvas;
class SkImage;
class SkPixmap;
class SkSurface;
class SketchbookView;

namespace sigil::sketch {
class Host;
class ThumbnailWriter;
}  // namespace sigil::sketch

namespace sigil::io::publish {
class Publisher;
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
  /** Draws the window's own view of the presented sketch into @p canvas:
   *  the dark matte, the sketch letterboxed into it at the resolution
   *  the item stands at, and the zoom that magnifies both. @p published
   *  is the frame that has ALREADY left by the publication door this
   *  tick, drawn here in place of the sketch — a sketch is stateful, so
   *  one tick is one frame and the window shows the same one its
   *  subscribers received. Null draws the sketch itself. */
  void drawSketch(SkCanvas& canvas, QSize pixelSize, const SkImage* published);
  /** Clears @p canvas to the ground the running sketch declared — the
   *  alpha with it, so a sketch grounded in a translucent or fully
   *  transparent colour leaves the pixels under it translucent — and
   *  draws one frame of it, advancing the presented clock by the step it
   *  took. It is THE tick: whoever calls it is the only one that may,
   *  because a second call in the same frame would step the sketch
   *  twice. hostMutex must be held, and the host must be live. */
  void paintFrame(SkCanvas& canvas, sigil::sketch::Host& host);
  /** Draws the frame a subscriber receives — the sketch's own canvas, at
   *  the pixel size the sketch declared, over the ground it declared and
   *  with nothing of this window around it — and hands back that texture
   *  as an image for the window to show. Null while this window is
   *  publishing nothing, and null without drawing anything at all when
   *  the canvas could not be offered, which leaves the tick to
   *  `drawSketch`. hostMutex must be held.
   *  @p window is the texture this frame is presented through, and says
   *  what a pixel of this window looks like: the canvas is offered in
   *  the same format, so nothing about a channel differs between what is
   *  on screen and what leaves. */
  [[nodiscard]] sk_sp<SkImage> renderPublication(const QRhiTexture& window);
  void runPendingCaptures();  // hostMutex must be held
  void refreshThumbnail();    // hostMutex must be held
  /** Passes on the stills the writer has finished, which it reports on
   *  its own thread. The view is the render thread's to name, so the
   *  crossing to the GUI thread is made from here. */
  void reportWrittenThumbnails();
  /** hostMutex must be held. Hands back the session evicted to make
   *  room, for the caller to let go of once the lock is released:
   *  ~Host waits on the build it may be in the middle of. */
  [[nodiscard]] std::unique_ptr<sigil::sketch::Host> openSketch(int index);
  /** Applies selection or replay requests with hostMutex held. A request
   *  for a selection that has since changed is discarded. */
  [[nodiscard]] std::unique_ptr<sigil::sketch::Host> updateSession();
  void resetPresentation();  // hostMutex must be held
  void publishMetrics();     // hostMutex must be held
  /** Stands the publisher up, or says why this window has nothing to
   *  offer and leaves publishing off. */
  void startPublishing();
  void stopPublishing();
  /** Turns publishing off and says why, on the console and on the
   *  window's own button. A publication that cannot offer the sketch's
   *  canvas offers NOTHING: a subscriber handed this window instead
   *  would be composited a matte, a letterbox and whatever the zoom
   *  stood at. */
  void refusePublishing(QString reason);
  /** Hands the frame just drawn to whoever is subscribed, on the
   *  command buffer Qt commits after `render()` returns. Nothing at all
   *  while this window is not publishing. */
  void publishFrame(QRhiTexture* texture, QRhiCommandBuffer* commandBuffer,
                    QSize pixelSize);
  /** Routes a session's captures through this renderer's own context.
   *  Once live frames render on the device, the runtime's caches hold
   *  device-backed images that cannot replay onto a raster canvas, so
   *  every session gets this as it is built rather than only the first. */
  void installCaptureBackend(sigil::sketch::Host& host);

#ifdef SIGILSKETCH_BOOK_GPU
  bool readbackGraphite(SkSurface& surface, const SkPixmap& out);
  std::unique_ptr<sigil::skia::GraphiteContext> m_graphiteContext;
  /** THE DEVICE PROGRAMS STOOD UP BEFORE THE FIRST SKETCH DRAWS, off
   *  this thread — building one costs exactly what waiting for it
   *  would, which is the whole point of not doing it here. Kicked once
   *  the context exists, because it precompiles through that context;
   *  waited for before the context is let go, and declared after it so
   *  this is the first of the two to be destroyed. */
  std::future<void> m_pipelineWarmup;
  /** HOW MANY FRAMES THE CANVAS STAYS EMPTY waiting for that warm-up,
   *  and how many it has stayed. Long enough for a recorded set to be
   *  rebuilt, which is what the frame after it wants; short enough that
   *  a warm-up gone wrong is a late sketch rather than no sketch. */
  static constexpr int kFramesHeldForWarmup = 120;
  int m_framesHeldForWarmup = 0;
  /** Kept alive for the length of a capture: the fence the still on a
   *  device is described through. */
  std::unique_ptr<sigil::skia::PaintOrderCanvas> m_captureCanvas;
#endif
  /** WHAT THIS WINDOW IS OFFERING ITS FRAMES THROUGH, and whether it is
   *  offering them. The flag is the view's, read on every synchronize;
   *  the publisher stands only while it is true, because a publication
   *  that exists is one other applications can already see. */
  std::unique_ptr<sigil::io::publish::Publisher> m_publisher;
  /** THE SKETCH'S OWN CANVAS, IN A TEXTURE OF ITS OWN. What leaves by
   *  the publication door is this and not the window's: the declared
   *  canvas at one texture pixel per canvas unit, cleared to the
   *  declared ground, with no matte around it and no zoom over it, so a
   *  subscriber composites the sketch rather than a picture of this
   *  window. It stands only while something is publishing, and is made
   *  again whenever the declared canvas changes size. */
  std::unique_ptr<QRhiTexture> m_publishedCanvas;
  QSize m_publishedSize;
  bool m_publishing = false;
  std::uint64_t m_publicationRequest = 0;
  SketchbookView* m_view = nullptr;
  QRhi* m_rhi = nullptr;
  bool m_initialized = false;
  /** WHETHER QT WILL MIRROR THE TEXTURED QUAD unless the item says
   *  otherwise: true on a backend whose framebuffers are y-up, which is
   *  the compensation a render pass needs and this item never makes.
   *  Read off the QRhi where reading it is legal — inside initialize()
   *  — and applied to the item on the synchronize that follows it. */
  bool m_presentsYUp = false;
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
  int m_replayIndex = -1;
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
  /** WHICH STILLS HAVE LANDED, written by the writer's worker and drained
   *  by the render thread on the frame after. */
  std::mutex m_writtenMutex;
  std::vector<int> m_written;
  /** THE ENCODE AND THE WRITE OF A STILL, off this thread. Stood up by
   *  the first sketch that reaches its moment, and declared last so it
   *  is the first thing this renderer lets go of: its destructor is
   *  what joins the worker that reports into the two above. */
  std::unique_ptr<sigil::sketch::ThumbnailWriter> m_thumbnailWriter;
};
