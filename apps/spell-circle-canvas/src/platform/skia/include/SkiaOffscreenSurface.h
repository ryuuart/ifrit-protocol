#pragma once
#include <QSize>
#include <include/core/SkRefCnt.h>

class QRhiTexture;
class SkiaGraphiteContext;
class SkCanvas;
class SkSurface;

/**
 * Wraps an existing QRhiTexture's native Metal texture (e.g. the one owned by
 * a QCanvasOffscreenCanvas) in an SkSurface, without copying it, so SkCanvas
 * draw calls land directly in that texture. Construct fresh per use — it's a
 * thin, cheap wrapper around a texture someone else owns.
 */
class SkiaOffscreenSurface {
public:
  /** @p texture must be a Metal-backed QRhiTexture created by the same QRhi
   *  whose device/queue @p context was built from. */
  SkiaOffscreenSurface(SkiaGraphiteContext &context, QRhiTexture *texture,
                       QSize pixelSize);
  ~SkiaOffscreenSurface();

  /** Null if wrapping the backend texture failed. */
  SkCanvas *canvas() const;

  /** Snaps the Recorder's accumulated draw commands into a Recording,
   *  inserts it into the Context, and submits it to the GPU synchronously
   *  (blocks until the GPU has finished executing it). */
  void submit();

private:
  SkiaGraphiteContext &m_context;
  sk_sp<SkSurface> m_surface;
};
