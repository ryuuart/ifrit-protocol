/** @file
 * The clip: a shape recorded, applied, inverted, and undone by the pop.
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

// ---- the clip ---------------------------------------------------------------

void Pen::recordClip() {
  m_clipBuilder = SkPathBuilder();
  m_clipBase = m_canvas ? m_canvas->getLocalToDevice() : SkM44();
  m_clipRecording = true;
}

bool Pen::recordShape(const SkPath& path) {
  if (!m_clipRecording) return false;
  SkM44 inverse;
  // The shape is in the space it was DRAWN in, and the mask is applied
  // in the space the recording began in, so a translate or a rotate
  // inside the shape function carries the mask with it.
  if (m_canvas && m_clipBase.invert(&inverse))
    m_clipBuilder.addPath(
        path.makeTransform((inverse * m_canvas->getLocalToDevice()).asM33()));
  else
    m_clipBuilder.addPath(path);
  return true;
}

void Pen::applyClip(ClipOptions options) {
  m_clipRecording = false;
  const SkPath mask = m_clipBuilder.detach();
  if (!m_canvas) return;
  // No save of its own: the clip rides the canvas's stack, so `pop`
  // takes it off and the end of the frame takes it off, which is where
  // p5 ends a mask too.
  m_canvas->clipPath(
      mask, options.invert ? SkClipOp::kDifference : SkClipOp::kIntersect,
      m_style.antiAlias);
}

SkRect Pen::rectBox(float x, float y, float w, float h) const {
  return boxIn(m_style.rectMode, x, y, w, h);
}

float Pen::toDegrees(float angle) const {
  return m_style.angleMode == DEGREES ? angle : degrees(angle);
}

float Pen::toRadians(float angle) const {
  return m_style.angleMode == DEGREES ? radians(angle) : angle;
}

}  // namespace sigil::draw
