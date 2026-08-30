/** @file
 * The masking family as values: a Region, the `parts::` selections, the
 * `by::` gates and how many scalars a gate carries.
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

#include <algorithm>
#include <cmath>   // std::isfinite — the profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

// ---- the masking family ---------------------------------------------------

Region Region::own() { return Region{}; }
Region Region::rect(const SkRect& r) {
  Region out;
  out.m_kind = Kind::Rect;
  out.m_rect = r;
  return out;
}
Region Region::oval(const SkRect& bounds) {
  Region out;
  out.m_kind = Kind::Oval;
  out.m_rect = bounds;
  return out;
}
Region Region::path(SkPath p) {
  Region out;
  out.m_kind = Kind::Path;
  out.m_path = std::move(p);
  return out;
}
bool Region::operator==(const Region& other) const {
  if (m_kind != other.m_kind) return false;
  switch (m_kind) {
    case Kind::Own:
      return true;
    case Kind::Rect:
    case Kind::Oval:
      return m_rect == other.m_rect;
    case Kind::Path:
      return m_path == other.m_path;
  }
  return false;
}
SkPath Region::resolve(const SkPath& ownShape) const {
  switch (m_kind) {
    case Kind::Own:
      return ownShape;
    case Kind::Rect: {
      SkPathBuilder b;
      b.addRect(m_rect);
      return b.detach();
    }
    case Kind::Oval: {
      SkPathBuilder b;
      b.addOval(m_rect);
      return b.detach();
    }
    case Kind::Path:
      return m_path;
  }
  return ownShape;
}

namespace parts {
Parts all() { return Parts{Parts::kAll, {}}; }
Parts marks() { return Parts{Parts::kMarks, {}}; }
Parts surface() { return Parts{Parts::kSurface, {}}; }
Parts content() { return Parts{Parts::kContent, {}}; }
Parts children() { return Parts{Parts::kChildren, {}}; }
Parts named(std::string_view name) {
  Parts p;
  p.names.emplace_back(name);
  return p;
}
}  // namespace parts

namespace by {
Gate spans(Spans where) {
  Gate g;
  g.kind = Gate::Kind::Spans;
  g.where = std::move(where);
  return g;
}
Gate edge(float angleDeg, Animatable<float> fraction) {
  Gate g;
  g.kind = Gate::Kind::Edge;
  g.angleDeg = angleDeg;
  g.fraction = std::move(fraction);
  return g;
}
Gate shape(Region r) {
  Gate g;
  g.kind = Gate::Kind::Shape;
  g.region = std::move(r);
  return g;
}
Gate outside(Region r) {
  Gate g = shape(std::move(r));
  g.outside = true;
  return g;
}
Gate alpha(Material coverage) {
  Gate g;
  g.kind = Gate::Kind::Coverage;
  g.coverage = std::make_shared<const Material>(std::move(coverage));
  return g;
}
Gate alphaOut(Material coverage) {
  Gate g = alpha(std::move(coverage));
  g.outside = true;
  return g;
}
Gate luma(Material coverage) {
  Gate g = alpha(std::move(coverage));
  g.channel = Gate::Channel::Luma;
  return g;
}
Gate lumaOut(Material coverage) {
  Gate g = luma(std::move(coverage));
  g.outside = true;
  return g;
}
}  // namespace by

size_t Gate::valueCount() const {
  switch (kind) {
    case Kind::Spans:
      return where.valueCount();
    case Kind::Edge:
      return 1;
    case Kind::Shape:
    case Kind::Coverage:
      return 0;
  }
  return 0;
}

}  // namespace sigil::compose
