#pragma once
// The global-namespace spelling of sigil::skia::OffscreenSurface, with the
// Qt constructor the sigil::skia::wrapTexture adapter replaces. Nothing
// new includes this: spell <sigilskia/graphite/OffscreenSurface.h> (and
// <sigilskia/qt/QtInterop.h> for a QRhiTexture) and delete this header
// once nothing does.

#include <sigilskia/graphite/OffscreenSurface.h>
#include <sigilskia/qt/QtInterop.h>

class SkiaOffscreenSurface final : public sigil::skia::OffscreenSurface {
 public:
  SkiaOffscreenSurface(sigil::skia::GraphiteContext& context,
                       QRhiTexture* texture, QSize pixelSize)
      : OffscreenSurface(
            sigil::skia::wrapTexture(context, texture, pixelSize)) {}
#ifdef __APPLE__
  SkiaOffscreenSurface(sigil::skia::GraphiteContext& context, void* mtlTexture,
                       int width, int height)
      : OffscreenSurface(context, mtlTexture, width, height) {}
#endif
};
