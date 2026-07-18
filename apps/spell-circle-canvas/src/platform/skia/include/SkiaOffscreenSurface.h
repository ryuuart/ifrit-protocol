#pragma once
#include <QSize>
#include <include/core/SkRefCnt.h>

class QRhiTexture;
class SkiaGraphiteContext;
class SkCanvas;
class SkSurface;

/**
 * Wraps an existing QRhiTexture's native texture (e.g. the one owned by a
 * QCanvasOffscreenCanvas) in an SkSurface, without copying it, so SkCanvas
 * draw calls land directly in that texture. Construct fresh per use — it's a
 * thin, cheap wrapper around a texture someone else owns. The wrap is
 * per-API (Metal on Apple platforms, Vulkan elsewhere), matching the
 * SkiaGraphiteContext create() TU compiled into this build.
 */
class SkiaOffscreenSurface {
public:
  /** @p texture must be a QRhiTexture created by the same QRhi whose native
   *  device/queue @p context was built from. */
  SkiaOffscreenSurface(SkiaGraphiteContext &context, QRhiTexture *texture,
                       QSize pixelSize);
  ~SkiaOffscreenSurface();

  /** Null if wrapping the backend texture failed. */
  SkCanvas *canvas() const;

  /** Snaps the Recorder's accumulated draw commands into a Recording,
   *  inserts it into the Context, and submits it to the GPU asynchronously.
   *  Safe because Graphite shares Qt's Metal command queue: Qt's later
   *  render pass is ordered after this submission on the same queue. */
  void submit();

private:
  SkiaGraphiteContext &m_context;
  sk_sp<SkSurface> m_surface;
};
