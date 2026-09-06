/** @file
 * The transform: p5's translate, rotate, scale, shear, push and pop.
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

// ---- transform --------------------------------------------------------------

void Pen::translate(float x, float y) {
  if (m_canvas) m_canvas->translate(x, y);
}

void Pen::rotate(float angle) {
  if (m_canvas) m_canvas->rotate(toDegrees(angle));
}

void Pen::scale(float s) { scale(s, s); }

void Pen::scale(float sx, float sy) {
  if (m_canvas) m_canvas->scale(sx, sy);
}

void Pen::shearX(float angle) {
  if (m_canvas) m_canvas->skew(std::tan(toRadians(angle)), 0.0f);
}

void Pen::shearY(float angle) {
  if (m_canvas) m_canvas->skew(0.0f, std::tan(toRadians(angle)));
}

void Pen::push() {
  m_stack.push_back(m_style);
  if (m_canvas) m_canvas->save();
}

void Pen::pop() {
  if (m_stack.size() <= m_stackFloor) return;
  m_style = m_stack.back();
  m_stack.pop_back();
  if (m_canvas) m_canvas->restore();
  applyStyle();
}

void Pen::resetMatrix() {
  if (m_canvas) m_canvas->setMatrix(m_base);
}

void Pen::applyMatrix(float a, float b, float c, float d, float e, float f) {
  if (!m_canvas) return;
  // p5's six numbers are the column-major 2D affine: x' = a x + c y + e.
  m_canvas->concat(SkMatrix::MakeAll(a, c, e, b, d, f, 0, 0, 1));
}

}  // namespace sigil::draw
