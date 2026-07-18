#pragma once
#include <include/core/SkRefCnt.h>

class QRhiTexture;
class QSize;
class SkiaGraphiteContext;
class SkCanvas;
class SkSurface;

/**
 * Wraps an existing native texture (e.g. the one owned by a
 * QCanvasOffscreenCanvas, or a CAMetalLayer drawable) in an SkSurface,
 * without copying it, so SkCanvas draw calls land directly in that texture.
 * Construct fresh per use — it's a thin, cheap wrapper around a texture
 * someone else owns.
 *
 * Like SkiaGraphiteContext, the Qt constructor (QRhiTexture) lives in the
 * SpellCircleSkiaQt adapter target and the Qt-free Metal constructor lives
 * in SpellCircleSkia; the wrap is per-API, matching the context-creation TU
 * compiled into each build.
 */
class SkiaOffscreenSurface {
public:
  /** @p texture must be a QRhiTexture created by the same QRhi whose native
   *  device/queue @p context was built from. Qt adapter — link
   *  SpellCircleSkiaQt for this constructor. */
  SkiaOffscreenSurface(SkiaGraphiteContext &context, QRhiTexture *texture,
                       QSize pixelSize);

#ifdef __APPLE__
  /** Qt-free Metal wrap: @p mtlTexture is an id<MTLTexture> bridged to
   *  void*, created on the same device @p context was built from. */
  SkiaOffscreenSurface(SkiaGraphiteContext &context, void *mtlTexture,
                       int width, int height);
#endif

  ~SkiaOffscreenSurface();

  /** Null if wrapping the backend texture failed. */
  SkCanvas *canvas() const;

  /** Snaps the Recorder's accumulated draw commands into a Recording,
   *  inserts it into the Context, and submits it to the GPU asynchronously.
   *  Safe because Graphite shares the host's command queue: the host's
   *  later GPU work is ordered after this submission on the same queue. */
  void submit();

private:
  SkiaGraphiteContext &m_context;
  sk_sp<SkSurface> m_surface;
};
