/** @file
 * Images through the pen, in p5's image modes and under its smoothing.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkClipOp.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <include/core/SkVertices.h>
#include <include/effects/SkDashPathEffect.h>
#include <sigildraw/Math.h>
#include <sigildraw/Pen.h>
#include <sigilmaterial/core/Material.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

#include "PenInternal.h"

namespace sigil::draw {

using detail::samplingFor;

// ---- image ------------------------------------------------------------------

void Pen::image(const sk_sp<SkImage>& img, float x, float y) {
  if (!img) return;
  image(img, x, y, (float)img->width(), (float)img->height());
}

void Pen::image(const sk_sp<SkImage>& img, float x, float y, float w, float h) {
  if (!m_canvas || !img || m_clipRecording) return;
  const SkRect box = boxIn(m_style.imageMode, x, y, w, h);
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(img, box, samplingFor(m_style.antiAlias), &paint);
}

void Pen::image(const sk_sp<SkImage>& img, float dx, float dy, float dw,
                float dh, float sx, float sy, float sw, float sh) {
  if (!m_canvas || !img || m_clipRecording) return;
  const SkRect box = boxIn(m_style.imageMode, dx, dy, dw, dh);
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(img, SkRect::MakeXYWH(sx, sy, sw, sh), box,
                          samplingFor(m_style.antiAlias), &paint,
                          SkCanvas::kStrict_SrcRectConstraint);
}

}  // namespace sigil::draw
