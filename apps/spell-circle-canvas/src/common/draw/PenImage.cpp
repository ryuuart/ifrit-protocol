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
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/advanced/Skia.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <string>
#include <string_view>

#include "PenInternal.h"

namespace sigil::draw {

using detail::samplingFor;

// ---- image ------------------------------------------------------------------

void Pen::image(const sk_sp<SkImage>& source, float x, float y) {
  if (!source) return;
  image(source, x, y, (float)source->width(), (float)source->height());
}

void Pen::image(const sk_sp<SkImage>& source, float x, float y, float width,
                float height) {
  if (!m_canvas || !source || m_clipRecording) return;
  const SkRect box = boxIn(m_style.imageMode, x, y, width, height);
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(source, box, samplingFor(m_style.antiAlias), &paint);
}

void Pen::image(const sk_sp<SkImage>& source, float destinationX,
                float destinationY, float destinationWidth,
                float destinationHeight, float sourceX, float sourceY,
                float sourceWidth, float sourceHeight) {
  if (!m_canvas || !source || m_clipRecording) return;
  const SkRect box = boxIn(m_style.imageMode, destinationX, destinationY,
                           destinationWidth, destinationHeight);
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(
      source, SkRect::MakeXYWH(sourceX, sourceY, sourceWidth, sourceHeight),
      box, samplingFor(m_style.antiAlias), &paint,
      SkCanvas::kStrict_SrcRectConstraint);
}

void Pen::image(const media::Frame& frame, float x, float y, float width,
                float height, material::Fit fit) {
  if (!m_canvas || m_clipRecording) return;
  const sk_sp<SkImage> picture =
      media::deviceImage(frame, m_canvas->recorder());
  // Both sides are guarded: a picture with no extent divides by its own
  // height below exactly as an empty box divides by its own.
  if (!picture || picture->width() <= 0 || picture->height() <= 0) return;
  const SkRect box = boxIn(m_style.imageMode, x, y, width, height);
  if (box.isEmpty()) return;
  const SkRect whole = SkRect::MakeIWH(picture->width(), picture->height());
  SkRect source = whole;
  SkRect destination = box;
  const float sourceAspect = whole.width() / whole.height();
  const float boxAspect = box.width() / box.height();
  if (fit == material::Fit::Native) {
    destination = whole.makeOffset(box.left(), box.top());
  } else if (fit == material::Fit::Cover) {
    if (sourceAspect > boxAspect) {
      const float kept = whole.height() * boxAspect;
      source = SkRect::MakeXYWH((whole.width() - kept) * 0.5f, 0, kept,
                                whole.height());
    } else {
      const float kept = whole.width() / boxAspect;
      source = SkRect::MakeXYWH(0, (whole.height() - kept) * 0.5f,
                                whole.width(), kept);
    }
  } else if (fit == material::Fit::Contain) {
    if (sourceAspect > boxAspect) {
      const float height = box.width() / sourceAspect;
      destination = SkRect::MakeXYWH(box.left(),
                                     box.top() + (box.height() - height) * 0.5f,
                                     box.width(), height);
    } else {
      const float width = box.height() * sourceAspect;
      destination = SkRect::MakeXYWH(box.left() + (box.width() - width) * 0.5f,
                                     box.top(), width, box.height());
    }
  }
  SkPaint paint;
  blendInto(paint);
  m_canvas->drawImageRect(picture, source, destination,
                          samplingFor(m_style.antiAlias), &paint,
                          SkCanvas::kStrict_SrcRectConstraint);
}

}  // namespace sigil::draw
