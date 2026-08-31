#pragma once
#include <sigilskia/graphite/GraphiteContext.h>
#include <sigilskia/graphite/OffscreenSurface.h>

#include <memory>

class QRhi;
class QRhiTexture;
class QSize;

namespace sigil::skia {

/** Stands Graphite up on the native device and queue @p rhi renders with,
 *  so Graphite's submissions and Qt's own render passes share one queue.
 *  Exactly one graphics API is served per build — Metal on Apple, Vulkan
 *  elsewhere — and any other QRhi backend returns null. Null is a normal
 *  outcome a caller handles by drawing another way, never a signal to
 *  assume a particular API. */
std::unique_ptr<GraphiteContext> createGraphiteContext(QRhi* rhi);

/** Wraps the native texture behind @p texture, which must have been
 *  created by the same QRhi @p context was built from, at @p pixelSize.
 *  The result's `canvas()` is null when the wrap failed. */
OffscreenSurface wrapTexture(GraphiteContext& context, QRhiTexture* texture,
                             QSize pixelSize);

}  // namespace sigil::skia
