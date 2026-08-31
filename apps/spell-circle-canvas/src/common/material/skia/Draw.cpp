/** @file
 * A material painted across a path: the shader, a clip, one drawPaint.
 */

#include "sigilmaterial/skia/Draw.h"

#include <include/core/SkPaint.h>

#include "sigilmaterial/skia/SkiaCompiler.h"

namespace sigil::material::skia {

void fill(SkCanvas& canvas, const SkPath& path, const Material& material,
          const FrameData& frame) {
  sk_sp<SkShader> s = shader(material, frame);
  if (!s) return;
  SkPaint paint;
  paint.setAntiAlias(true);
  paint.setShader(std::move(s));
  canvas.save();
  canvas.clipPath(path, true);
  canvas.drawPaint(paint);
  canvas.restore();
}

}  // namespace sigil::material::skia
