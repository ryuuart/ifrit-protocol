/** @file
 * The masking family's engine: the `by::` gate constructors, each carrying
 * the MaskResolver the kernel reads its gate through — a spans gate's show
 * set, a shape gate's clip region resolved from its Region, a coverage
 * gate's fill — and the span arithmetic those reads intersect with. The
 * Region value and the `parts::` selections are the kernel's.
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
#include <cmath>  // std::isfinite — the geometry::path::profileOffset non-finite guard
#include <cstdio>  // std::snprintf — variationDrive's effect key
#include <set>

#include "ComposeInternal.h"
#include "SpanArithmetic.h"
#include "sigilgeometry/path/Contour.h"

namespace sigil::compose {

using detail::ElementNode;
using detail::Kind;

// ---- the masking family ---------------------------------------------------

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

namespace by {
Gate spans(Spans where) {
  Gate g;
  g.resolver = detail::maskResolver();
  g.kind = Gate::Kind::Spans;
  g.where = std::move(where);
  return g;
}
Gate edge(float angleDeg, motion::Animatable<float> fraction) {
  Gate g;
  g.resolver = detail::maskResolver();
  g.kind = Gate::Kind::Edge;
  g.angleDeg = angleDeg;
  g.fraction = std::move(fraction);
  return g;
}
Gate shape(Region r) {
  Gate g;
  g.resolver = detail::maskResolver();
  g.kind = Gate::Kind::Shape;
  g.region = std::move(r);
  return g;
}
Gate outside(Region r) {
  Gate g = shape(std::move(r));
  g.outside = true;
  return g;
}
Gate alpha(material::skia::Paint coverage) {
  Gate g;
  g.resolver = detail::maskResolver();
  g.kind = Gate::Kind::Coverage;
  g.coverage =
      std::make_shared<const material::skia::Paint>(std::move(coverage));
  return g;
}
Gate alphaOut(material::skia::Paint coverage) {
  Gate g = alpha(std::move(coverage));
  g.outside = true;
  return g;
}
Gate luma(material::skia::Paint coverage) {
  Gate g = alpha(std::move(coverage));
  g.channel = Gate::Channel::Luma;
  return g;
}
Gate lumaOut(material::skia::Paint coverage) {
  Gate g = luma(std::move(coverage));
  g.outside = true;
  return g;
}
}  // namespace by

namespace {

/** The engine as a MaskResolverOps: a spans gate resolves its terms and
 *  normalizes, a shape gate resolves its region against the node's
 *  outline, a coverage gate resolves its material. One value for every
 *  gate. */
struct MaskEngine final : MaskResolverOps {
  bool operator==(const MaskEngine&) const { return true; }
  std::vector<Span> normalize(const std::vector<Span>& spans) const override {
    return detail::normalizeSpans(spans);
  }
  std::vector<Span> intersect(const std::vector<Span>& a,
                              const std::vector<Span>& b) const override {
    return detail::intersectSpans(a, b);
  }
  std::vector<Span> complement(const std::vector<Span>& spans) const override {
    return detail::complementSpans(spans);
  }
  SkPath spanPath(const SkPath& src,
                  const std::vector<Span>& spans) const override {
    return detail::spanPath(src, spans);
  }
  std::vector<Span> plan(const Gate& gate, const SpanInput& in) const override {
    return detail::normalizeSpans(gate.where.resolve(in));
  }
  SkPath clipRegion(const Gate& gate, const SkPath& ownShape) const override {
    return gate.region.resolve(ownShape);
  }
  Fill coverage(const Gate& gate, const PaintContext& ctx) const override {
    if (!gate.coverage) return {};
    const material::skia::Paint& mat = *gate.coverage;
    return (mat.isAnimated() || mat.geometryDependent()) ? resolveFill(mat, ctx)
                                                         : toFill(mat);
  }
};

}  // namespace

const MaskResolver& detail::maskResolver() {
  static const MaskResolver kResolver{MaskEngine{}};
  return kResolver;
}

}  // namespace sigil::compose
