/** @file
 * The paint values with bodies: a shader Fill, and the element node's
 * copy-on-write handle.
 */

#include <include/core/SkContourMeasure.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathUtils.h>
#include <include/core/SkShader.h>
#include <include/core/SkTypes.h>  // SkDebugf — the slot-rename diagnostic
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/pathops/SkPathOps.h>
#include <sigilmaterial/skia/SkiaCompiler.h>

#include <algorithm>
#include <cmath>  // std::isfinite — the geometry::path::profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

Fill Fill::shader(sk_sp<SkShader> s) {
  Fill f;
  f.kind = Fill::Kind::Shader;
  f.shaderValue = std::move(s);
  return f;
}

Element::Element() : m_node(std::make_shared<ElementNode>()) {}

ElementNode* Element::NodeHandle::operator->() {
  if (!value)
    value = std::make_shared<ElementNode>();
  else if (value.use_count() != 1)
    value = std::make_shared<ElementNode>(*value);
  return value.get();
}

const ElementNode* Element::NodeHandle::operator->() const {
  return value.get();
}

}  // namespace sigil::compose
