/** @file
 * The masking family at paint: the span gates resolved against a node's
 * uncut boundary, and the plane gates opened and closed around the parts
 * they select — a canvas clip for an edge or a shape, a coverage layer for
 * alpha and luma and their complements.
 */

#include "Masking.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkShader.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilshaders/ComposeCore.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

namespace {

// ---- the coverage law, in one place ---------------------------------------
//
// A coverage gate (`by::alpha`, `by::luma` and their complements) draws the
// Material over the masked group's layer and keeps what it covers. The gate
// asks two INDEPENDENT questions and each has exactly one mechanism:
//
//  - WHICH CHANNEL (Gate::Channel) — Alpha is the shader's own alpha and
//    needs no work at all. Luma weights the PREMULTIPLIED colour with
//    Rec. 601 on the ENCODED values, with no linearization, because
//    everything here composites in encoded sRGB. Premultiplied is what
//    makes a TRANSPARENT matte read as black, the way compositing
//    applications do.
//  - WHICH SIDE (Gate::outside) — the complement is `kDstOut` instead of
//    `kDstIn`. `dst * (1 - a)` IS `1 - coverage`, exactly, for any source,
//    so an inverted matte costs one enum value and no shader.

/** Rec. 601 luma of a resolved COLOUR, as a coverage alpha. `Fill`'s colour
 *  is unpremultiplied, so the premultiplied reading is written out:
 *  `a · dot(rgb, k)`. */
material::Color lumaCoverageColor(const material::Color& c) {
  const float y = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
  return {0, 0, 0, std::clamp(c.a * y, 0.0f, 1.0f)};
}

/** …and of a resolved SHADER. A shader's channels arrive PREMULTIPLIED, so
 *  the same law is one dot product: `dot(a·rgb, k) == a · dot(rgb, k)`. The
 *  result `(0,0,0,Y')` is a valid premultiplied colour because the
 *  coefficients sum to 1, so `Y' <= a` always. */
sk_sp<SkShader> lumaCoverageShader(sk_sp<SkShader> src) {
  static const SkRuntimeEffect* effect = [] {
    auto result = SkRuntimeEffect::MakeForShader(
        SkString(shaderSource("LumaCoverage.sksl")));
    return result.effect.release();
  }();
  if (!effect || !src) return src;
  SkRuntimeEffect::ChildPtr child(std::move(src));
  return effect->makeShader(nullptr, {&child, 1});
}

/** An edge gate's half-plane: the region lying before a moving edge at
 *  `angleDegrees` across @p bounds, built in the edge's own frame and
 *  rotated into place — {p : (p - mid)·d <= edge}. */
SkPath edgeRegion(const SkRect& bounds, float angleDegrees, float t01) {
  const float t = std::clamp(t01, 0.0f, 1.0f);
  const float rad = geometry::path::radians(angleDegrees);
  const float c = std::cos(rad), s = std::sin(rad);
  const SkPoint mid{bounds.centerX(), bounds.centerY()};
  const float reach =
      0.5f * (std::abs(bounds.width() * c) + std::abs(bounds.height() * s));
  const float wide =
      SkPoint{bounds.width(), bounds.height()}.length() * 0.5f + 1.0f;
  const float edge = -reach + 2.0f * reach * t;
  SkPathBuilder b;
  b.addRect(SkRect::MakeLTRB(-reach - 1.0f, -wide, edge, wide));
  SkMatrix m = SkMatrix::RotateDeg(angleDegrees);
  m.postTranslate(mid.x(), mid.y());
  return b.detach().makeTransform(m);
}

}  // namespace

BoundaryGates resolveBoundaryGates(Instance& inst,
                                   const std::vector<Mask>* masks,
                                   const std::vector<float>& gateValues,
                                   const SkPath& outline) {
  const ElementNode& node = inst.painted();
  BoundaryGates gates;
  if (node.strokeData && node.strokeData->resolver)
    gates.arithmetic = node.strokeData->resolver.get();
  if (!gates.arithmetic && masks)
    for (const Mask& m : *masks)
      if (m.with.kind == Gate::Kind::Spans && m.with.resolver) {
        gates.arithmetic = m.with.resolver.get();
        break;
      }
  if (!masks) return gates;
  SpanInput gateIn;
  gateIn.outline = &outline;
  gateIn.fitRects = &inst.spanFitRects;
  size_t valueBase = 0;
  for (const Mask& m : *masks) {
    const size_t count = m.with.valueCount();
    if (m.with.kind != Gate::Kind::Spans) {
      valueBase += count;
      continue;
    }
    const std::vector<float> mine(
        gateValues.begin() + (long)std::min(valueBase, gateValues.size()),
        gateValues.begin() +
            (long)std::min(valueBase + count, gateValues.size()));
    valueBase += count;
    gateIn.values = &mine;
    const std::vector<Span> show = m.with.resolver
                                       ? m.with.resolver->plan(m.with, gateIn)
                                       : std::vector<Span>{};
    // THE INTERSECTION LAW: stacked masks both have to pass, so a second
    // gate over the same target narrows the first, never widens it.
    const auto narrow = [&](std::optional<std::vector<Span>>& slot) {
      slot = slot ? gates.intersect(*slot, show) : show;
    };
    if (m.what.selects(Parts::kSurface)) narrow(gates.surfaceShow);
    if (m.what.selects(Parts::kMarks)) narrow(gates.marksShow);
    for (const std::string& label : m.what.names) {
      auto it = std::find_if(gates.namedShow.begin(), gates.namedShow.end(),
                             [&](const auto& e) { return e.first == label; });
      if (it == gates.namedShow.end())
        gates.namedShow.emplace_back(label, show);
      else
        it->second = gates.intersect(it->second, show);
    }
  }
  return gates;
}

PlaneGates::PlaneGates(Composer::Impl& impl, Instance& inst, SkCanvas& canvas,
                       const std::vector<Mask>* masks,
                       const std::vector<float>& gateValues,
                       const SkRect& bounds, const SkPath& outline,
                       const PaintContext& paintContext)
    : impl(impl),
      inst(inst),
      canvas(canvas),
      bounds(bounds),
      fullOutline(outline),
      paintContext(paintContext) {
  if (masks) {
    size_t valueBase = 0;
    for (const Mask& m : *masks) {
      if (m.with.kind != Gate::Kind::Spans) {
        PlaneGate g;
        g.mask = &m;
        if (m.with.kind == Gate::Kind::Edge)
          g.fraction =
              valueBase < gateValues.size() ? gateValues[valueBase] : 1.0f;
        plane.push_back(g);
        granularPlane |= !m.what.isEverything();
      }
      valueBase += m.with.valueCount();
    }
  }
}

int PlaneGates::enter(bool wholeNode, Parts::Bits part,
                      std::string_view label) {
  if (plane.empty()) return -1;
  int base = -1;
  const auto hit = [&](const Mask& m) {
    if (m.what.isEverything() != wholeNode) return false;
    if (wholeNode) return true;
    return part == Parts::kMarks ? m.what.selectsMark(label)
                                 : m.what.selects(part);
  };
  for (const PlaneGate& g : plane) {
    const Mask& m = *g.mask;
    if (!hit(m) || m.with.kind == Gate::Kind::Coverage) continue;
    if (base < 0) base = canvas.getSaveCount();
    if (m.with.kind == Gate::Kind::Edge) {
      // A container of absolutely-positioned children measures ZERO, and
      // a half-plane built from an empty box is empty — so clipping to it
      // would hide the entire subtree even at a full reveal. A reveal at 1
      // must never hide anything, and an empty box has no axis to reveal
      // along in the first place.
      if (bounds.isEmpty()) continue;
      canvas.save();
      canvas.clipPath(edgeRegion(bounds, m.with.angleDegrees, g.fraction),
                      true);
    } else {  // Shape — and its complement, the missing clipOut()
      canvas.save();
      canvas.clipPath(
          m.with.resolver ? m.with.resolver->clipRegion(m.with, fullOutline)
                          : fullOutline,
          m.with.outside ? SkClipOp::kDifference : SkClipOp::kIntersect, true);
    }
  }
  for (const PlaneGate& g : plane) {
    const Mask& m = *g.mask;
    if (!hit(m) || m.with.kind != Gate::Kind::Coverage) continue;
    if (base < 0) base = canvas.getSaveCount();
    const SkRect layerBox = impl.recordBounds(inst);
    canvas.saveLayer(&layerBox, nullptr);
    SkPaint cover;
    cover.setAntiAlias(true);
    // The complement is the blend mode and nothing else: kDstOut is
    // dst·(1 - a), which is 1 - coverage exactly, for any source.
    cover.setBlendMode(m.with.outside ? SkBlendMode::kDstOut
                                      : SkBlendMode::kDstIn);
    if (m.with.coverage) {
      const Fill f = m.with.resolver
                         ? m.with.resolver->coverage(m.with, paintContext)
                         : Fill{};
      const bool luma = m.with.channel == Gate::Channel::Luma;
      if (sk_sp<SkShader> shader =
              f.kind == Fill::Kind::Paint
                  ? material::skia::staticShader(detail::paintOf(f))
                  : nullptr)
        cover.setShader(luma ? lumaCoverageShader(std::move(shader))
                             : std::move(shader));
      else if (f.kind == Fill::Kind::Color)
        cover.setColor4f(
            material::skia::toSkColor(luma ? lumaCoverageColor(f.colorValue)
                                           : f.colorValue),
            nullptr);
      else
        cover.setColor4f({0, 0, 0, 0},
                         nullptr);  // Fill::none() shows nothing
    }
    coverStack.push_back(std::move(cover));
  }
  return base;
}

void PlaneGates::leave(int base, size_t coverBase) {
  while (coverStack.size() > coverBase) {
    canvas.drawRect(impl.recordBounds(inst), coverStack.back());
    coverStack.pop_back();
    canvas.restore();
  }
  if (base >= 0) canvas.restoreToCount(base);
}

}  // namespace sigil::compose
