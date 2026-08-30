/** @file
 * The stacking painter, whole: paintContent and paint stay together because
 * they are one recursion with its cache tiers — live paint, an automatic
 * picture over provably-static subtrees, a split bake, a memo-held bake and
 * a texture bake — and the masking-at-paint and silhouette helpers only they
 * read. Splitting the recursion would put one cache decision in each half.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkContourMeasure.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPathEffect.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkStrokeRec.h>
#include <include/core/SkSurface.h>
#include <include/effects/SkRuntimeEffect.h>
#include <include/effects/SkTrimPathEffect.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — textFill's cap-height metrics

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "ComposeRuntime.h"
#include "PaintInternal.h"
#include "sigilgeometry/path/Contour.h"
#include "sigilgeometry/path/Skia.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// The masking family, at paint
//
// A mask is (selection, gate). The gates fall into two mechanical classes
// and the split is the whole implementation:
//
//  - SPANS cuts the BOUNDARY. It rewrites the path the selected
//    outline-tracing outputs trace — the surface (fill + echoes) and the
//    marks. Content and children do not trace a boundary, so a spans gate
//    over them is not a picture and does nothing.
//  - EDGE / SHAPE / COVERAGE cut the PLANE. They wrap the selected outputs
//    in a canvas clip (edge, shape) or a kDstIn/kDstOut coverage layer
//    (alpha and luma, each with its complement).
//
// Both classes intersect for free, and across each other: span sets
// intersect as interval arithmetic, nested clips intersect by definition,
// stacked kDstIn layers multiply coverage. Where a mask selects EVERYTHING
// the plane gates are hoisted to wrap the whole node once rather than each
// group — cheaper, and the only way a nested pair of antialiased clips
// cannot compound its own edge.

namespace {

/** Does this span set claim the whole boundary? Then the boundary is
 *  untouched, and returning the source path unchanged is required, not an
 *  optimisation: a fully settled reveal must draw exactly the path it would
 *  have drawn with no mask on it at all, bit for bit, or adding a mask that
 *  is currently showing everything moves pixels. */
bool claimsEverything(const std::vector<Span>& show) {
  return show.size() == 1 && show[0].begin <= 1e-6f &&
         show[0].end >= 1.0f - 1e-6f;
}

/** Apply a resolved SHOW set to a boundary. `cut`, when asked for, says
 *  the geometry actually changed — which only the SURFACE needs, because
 *  only the surface has a cheap rrect to fall out of. Decorations always
 *  draw a path. */
SkPath gateOutline(const SpanArithmeticOps* arith, const SkPath& src,
                   const std::vector<Span>& show, bool* cut = nullptr) {
  if (claimsEverything(show)) return src;
  if (cut) *cut = true;
  if (show.empty()) return SkPath();
  return arith ? arith->spanPath(src, show) : src;
}

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
SkColor4f lumaCoverageColor(const SkColor4f& c) {
  const float y = 0.299f * c.fR + 0.587f * c.fG + 0.114f * c.fB;
  return {0, 0, 0, std::clamp(c.fA * y, 0.0f, 1.0f)};
}

/** …and of a resolved SHADER. A shader's channels arrive PREMULTIPLIED, so
 *  the same law is one dot product: `dot(a·rgb, k) == a · dot(rgb, k)`. The
 *  result `(0,0,0,Y')` is a valid premultiplied colour because the
 *  coefficients sum to 1, so `Y' <= a` always. */
sk_sp<SkShader> lumaCoverageShader(sk_sp<SkShader> src) {
  static const SkRuntimeEffect* effect = [] {
    auto result = SkRuntimeEffect::MakeForShader(SkString(R"(
uniform shader src;
half4 main(float2 p) {
  half4 c = src.eval(p);
  half y = clamp(dot(c.rgb, half3(0.299, 0.587, 0.114)), 0, 1);
  return half4(0, 0, 0, y);
}
)"));
    return result.effect.release();
  }();
  if (!effect || !src) return src;
  SkRuntimeEffect::ChildPtr child(std::move(src));
  return effect->makeShader(nullptr, {&child, 1});
}

}  // namespace

// ---------------------------------------------------------------------------
// Silhouette

const SkPath& Composer::Impl::resolveOutline(Instance& inst,
                                             SkSize size) const {
  if (inst.outlineCacheDesc != inst.desc.get() ||
      inst.outlineCacheSize != size) {
    inst.outlineCache = inst.desc->shapeFn(size);
    inst.outlineCacheDesc = inst.desc.get();
    inst.outlineCacheSize = size;
  }
  return inst.outlineCache;
}

// ---------------------------------------------------------------------------
// textFill()/textStroke(): the one glyph-paint override

std::optional<sigil::weave::PaintStyle> Composer::Impl::metricTextStyle(
    Instance& inst, const PaintContext& paintCtx) {
  const ElementNode& node = *inst.desc;
  const Material* metricMat = metricFillOf(node);
  const bool stroked = node.textData && node.textData->hasTextStroke;
  if (!metricMat && !stroked) return std::nullopt;
  if (!inst.paragraph.has_value()) return std::nullopt;
  const sigil::weave::Paragraph& paragraph = inst.paragraph.value();

  // Chrome type: the material's unit square mapped to the text's metric
  // band — x across the widest line, y from the first line's cap top (real
  // cap height when the face reports one) to the last line's baseline.
  //
  // The override replaces the whole PaintStyle for every run, so it starts
  // as a COPY of the paragraph's own style and swaps only the foreground —
  // textFill supersedes the fill, not the underlays, overlays and
  // decorations around it (a chrome wordmark keeps its cast shadow and dark
  // keyline).
  sigil::weave::PaintStyle metric = paragraph.spans().empty()
                                        ? sigil::weave::PaintStyle{}
                                        : paragraph.spans().front().style.paint;
  metric.foreground.setShader(nullptr);
  bool havePaint = false;
  // textStroke(): a stroke pass on the glyphs, UNDER the fill. It joins the
  // style's own underlays rather than replacing them, so an engraved face
  // keeps its cast shadow.
  if (stroked) {
    sigil::weave::PaintLayer outline;
    outline.paint.setAntiAlias(true);
    outline.paint.setStyle(SkPaint::kStroke_Style);
    outline.paint.setStrokeWidth(node.textData->textStrokeWidth);
    outline.paint.setStrokeJoin(SkPaint::kRound_Join);
    const Fill& sf = node.textData->textStrokeFill;
    if (sf.kind == Fill::Kind::Shader && sf.shaderValue)
      outline.paint.setShader(sf.shaderValue);
    else
      outline.paint.setColor4f(
          sf.kind == Fill::Kind::Color ? sf.colorValue : SkColor4f{0, 0, 0, 1},
          nullptr);
    metric.addUnderlay(outline);
    havePaint = true;
  }
  if (!metricMat) return havePaint ? std::optional(metric) : std::nullopt;

  // Geometry-dependent materials resolve against a UNIT box here, not the
  // node's. The local matrix below already maps the shader's [0,1]² onto
  // the metric band, so uResolution baked from the node's layout size would
  // divide a second time: a `linearUnit` ramp came out at t ≈ 0.003 and
  // every glyph painted the first stop, flat and silently. Material.h
  // advertises textFill and the Unit ramps as the same trick, and this is
  // what makes that true.
  PaintContext metricCtx = paintCtx;
  metricCtx.size = {1.0f, 1.0f};
  const Fill f = (metricMat->isAnimated() || metricMat->geometryDependent())
                     ? metricMat->resolve(metricCtx)
                     : metricMat->toFill();
  if (f.kind == Fill::Kind::Shader && f.shaderValue && !inst.columns.empty()) {
    // A VERTICAL passage has no cap band to hang the ramp on: a column's
    // glyphs centre across its axis rather than standing on a baseline. The
    // unit square maps onto the COLUMN BLOCK instead — x across the columns,
    // y down them — so a ramp authored in [0,1]² still crosses the type,
    // reading down the page rather than across it.
    SkRect block = SkRect::MakeEmpty();
    for (const sigil::weave::ColumnMetrics& column : inst.columns)
      block.join(column.rect());
    SkMatrix map = SkMatrix::Translate(block.left(), block.top());
    map.preScale(std::max(block.width(), 1.0f), std::max(block.height(), 1.0f));
    metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
    havePaint = true;
  } else if (f.kind == Fill::Kind::Shader && f.shaderValue &&
             !inst.lines.empty()) {
    const sigil::weave::ShapedWord* firstFont = nullptr;
    sigil::weave::forEachPlacedGlyph(
        inst.textLayout, paragraph,
        [&](const sigil::weave::PlacedGlyph& placed) {
          if (!firstFont) firstFont = placed.shaped;
        });
    float capH = 0;
    if (firstFont && firstFont->typeface) {
      SkFontMetrics fm;
      sigil::weave::makeFont(firstFont->typeface, firstFont->fontSize)
          .getMetrics(&fm);
      capH = fm.fCapHeight;
    }
    const sigil::weave::LineMetrics& first = inst.lines.front();
    if (capH <= 0) capH = first.ascent;  // face reports none — the ascent band
    float left = first.left, right = first.right;
    for (const sigil::weave::LineMetrics& line : inst.lines) {
      left = std::min(left, line.left);
      right = std::max(right, line.right);
    }
    const float top = first.baseline - capH;
    const float bottom = inst.lines.back().baseline;
    SkMatrix map = SkMatrix::Translate(left, top);
    map.preScale(std::max(right - left, 1.0f), std::max(bottom - top, 1.0f));
    metric.foreground.setShader(f.shaderValue->makeWithLocalMatrix(map));
    havePaint = true;
  } else if (f.kind == Fill::Kind::Color) {
    metric.foreground.setColor4f(f.colorValue, nullptr);
    havePaint = true;
  }
  return havePaint ? std::optional(metric) : std::nullopt;
}

// ---------------------------------------------------------------------------
// The stacking painter

void Composer::Impl::paintContent(Instance& inst, SkCanvas& canvas,
                                  float contentScale, SkBlendMode leafBlend,
                                  float leafOpacity, Phase phase) {
  const ElementNode& node = *inst.desc;
  // The two halves of a node's paint, split at the children loop. A
  // split bake is only ever offered to a node with no layer effect — that
  // one WRAPS BOTH HALVES and a bake of the prefix alone would have to
  // reproduce it. A clip and a whole-node mask wrap both halves too, but
  // both are opened and closed inside EACH phase, so the phases stay a pair
  // of skips over an otherwise untouched function; the granular mask scopes
  // below are each opened and closed inside the half they belong to.
  const bool emitOwn = phase != Phase::ChildrenOnly;
  const bool emitChildren = phase != Phase::OwnOnly;
  const SkRect ownRect = instanceRect(inst);
  const SkRect bounds = SkRect::MakeWH(ownRect.width(), ownRect.height());
  const SkRRect rrect = cornersRRect(bounds, node.corners);

  // The node's shape: routed connector/rail path, custom outline(), or the
  // corner-rounded box.
  const bool routed =
      node.deriveData && (!node.deriveData->connectFrom.empty() ||
                          !node.deriveData->railAnchors.empty());
  const Across* bandWidth = node.bandWidth();
  const bool customShape = (node.shapeFn || bandWidth) && !routed;
  SkPath outlinePath;
  if (routed) {
    outlinePath = inst.connectorPath;  // derive phase routed it
  } else if (bandWidth) {
    // A BAND's shape is derived: the region its spine sweeps at the
    // profile's width, on the declared side. The spine is guide data
    // (authored here) or borrowed geometry (derive resolved it).
    const SkPath spine =
        node.deriveData->bandSpine
            ? node.deriveData->bandSpine({bounds.width(), bounds.height()})
            : inst.bandSpine;
    outlinePath = bandWidth->resolver
                      ? bandWidth->resolver->bandRegion(
                            spine, *bandWidth, node.deriveData->bandFormation)
                      : SkPath();
  } else if (customShape) {
    outlinePath = resolveOutline(inst, {bounds.width(), bounds.height()});
  } else {
    SkPathBuilder outlineBuilder;
    outlineBuilder.addRRect(rrect);
    outlinePath = outlineBuilder.detach();
  }

  // (clip() applies AFTER the decorations' outline is settled — see below:
  // decorations dress the outline and stay unclipped; fill/content/children
  // clip. The clip keeps the UNMASKED shape — a mask is a paint reveal.)
  const SkPath clipShape = outlinePath;

  // ---- the masking family, part 1: the BOUNDARY gates ---------------------
  //
  // Resolve each mask's gate once, then hand every paint output the version
  // of the boundary its selection earns. Two outputs trace a boundary — the
  // SURFACE (fill + echo re-stamps) and the MARKS (every decoration, every
  // span pass) — so at most two cut paths exist, and in the overwhelmingly
  // common case (`mask(by::spans(…))`, the whole node) they are the same
  // path and are computed once.
  //
  // The claim ledger is deliberately resolved against the UNCUT boundary
  // below: an overlap between two span passes is a description-level
  // mistake, and it must not be a mistake that blinks in and out between
  // 0.3 and 0.7 of a transition because a gate was shrinking one of them.
  const std::vector<Mask>* masks =
      node.hasMasks() ? &node.fxData->masks : nullptr;
  const std::vector<float> gateValues =
      masks ? inst.resolveGateValues() : std::vector<float>{};
  // THE SPAN ARITHMETIC the boundary is cut and intersected with — the
  // brush engine the node's stroke passes carry, or failing that the one
  // its first spans gate carries. Absent only on a node that has neither,
  // which then has nothing to cut.
  const SpanArithmeticOps* arith = nullptr;
  if (node.strokeData && node.strokeData->resolver)
    arith = node.strokeData->resolver.get();
  if (!arith && masks)
    for (const Mask& m : *masks)
      if (m.with.kind == Gate::Kind::Spans && m.with.resolver) {
        arith = m.with.resolver.get();
        break;
      }
  const auto intersect = [&](const std::vector<Span>& a,
                             const std::vector<Span>& b) {
    return arith ? arith->intersect(a, b) : a;
  };
  // The SHOW set each of surface / marks is left with, as fractions of the
  // whole boundary — absent when no spans gate selects it.
  std::optional<std::vector<Span>> surfaceShow, marksShow;
  // …and the per-NAMED-mark refinement, for masks that address one label.
  std::vector<std::pair<std::string, std::vector<Span>>> namedShow;
  if (masks) {
    SpanInput gateIn;
    gateIn.outline = &outlinePath;
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
        slot = slot ? intersect(*slot, show) : show;
      };
      if (m.what.selects(Parts::kSurface)) narrow(surfaceShow);
      if (m.what.selects(Parts::kMarks)) narrow(marksShow);
      for (const std::string& label : m.what.names) {
        auto it = std::find_if(namedShow.begin(), namedShow.end(),
                               [&](const auto& e) { return e.first == label; });
        if (it == namedShow.end())
          namedShow.emplace_back(label, show);
        else
          it->second = intersect(it->second, show);
      }
    }
  }
  // `trimmed` says the SURFACE's geometry is no longer the corner box,
  // which is what decides whether the fill draws a path or the cheap rrect.
  bool cut = false;
  const SkPath fullOutline = outlinePath;
  SkPath surfacePath = surfaceShow
                           ? gateOutline(arith, fullOutline, *surfaceShow, &cut)
                           : fullOutline;
  // …and the marks' boundary, which is the SAME OBJECT whenever one mask
  // gates both — the overwhelmingly common case, and the reason a whole-node
  // spans gate walks the boundary once rather than twice.
  SkPath marksPath = !marksShow ? fullOutline
                     : (surfaceShow && *marksShow == *surfaceShow)
                         ? surfacePath
                         : gateOutline(arith, fullOutline, *marksShow);
  const bool trimmed = cut;

  // The MARKS' boundary is what a decoration receives: every decoration
  // dresses the outline, and a spans gate over the marks is a cut of that
  // outline. The surface keeps its own (they are the same path, and the
  // same object, whenever one mask gates both — the common case).
  //
  // Built BEFORE the effect's saveLayer because an effect's child Material
  // resolves against it — the node's box, the node's clock, exactly what
  // Material::child hands a fill's children.
  const PaintContext paintCtx{
      {bounds.width(), bounds.height()},
      std::move(marksPath),
      elapsed(),
      contentScale,
      ticker.active(),
      &fonts,
      inst.borrowedPaths.empty() ? nullptr : &inst.borrowedPaths,
      &inst.stampCache,
      curToRoot,        // node→root, as paint() stacked it
      rootLayoutSize};  // …and the canvas it maps into

  // The node's own layer effect wraps everything painted here, so it is
  // captured by picture recordings and BAKED by texture snapshots. A LIVE
  // effect (bound uniforms, a live child material) resolves here per paint,
  // and computeVolatile has declared such a node volatile, so this
  // recording is never cached stale.
  const Effect* layerFx = layerEffectOf(node);
  const sk_sp<SkImageFilter> layerFilter =
      layerFx ? layerFx->resolvedImageFilter(&paintCtx) : nullptr;
  const bool hasEffect = (bool)layerFilter;
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(layerFilter);
    // BOUNDED: with nullptr bounds the layer allocates at the CLIP size, so
    // a small icon's drop shadow on a root-level canvas would filter the
    // whole canvas. recordBounds is what the subtree actually paints; Skia
    // expands it for the filter's own reach.
    const SkRect content = recordBounds(inst);
    canvas.saveLayer(&content, &effectPaint);
  }

  // ---- the masking family, part 2: the PLANE gates ------------------------
  //
  // `by::edge` and `by::shape` are canvas clips; `by::alpha` is a kDstIn
  // coverage layer. All three intersect for free — nested clips by
  // definition, stacked kDstIn layers by multiplication.
  //
  // A gate whose selection is EVERYTHING is hoisted to wrap the whole node
  // once. That is not only the cheap path: applying one antialiased clip
  // per paint group would compound its own edge coverage wherever the
  // groups overlap, so the hoisted form is also the only one whose edge is
  // the clip's own.
  struct PlaneGate {
    const Mask* mask = nullptr;
    float fraction = 1.0f;  // Edge
  };
  std::vector<PlaneGate> plane;
  bool granularPlane = false;
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

  /** An edge gate's half-plane: the region lying before a moving edge at
   *  `angleDeg`, built in the edge's own frame and rotated into place —
   *  {p : (p - mid)·d <= edge}. */
  const auto edgeRegion = [&](float angleDeg, float t01) {
    const float t = std::clamp(t01, 0.0f, 1.0f);
    const float rad = angleDeg * SK_FloatPI / 180.0f;
    const float c = std::cos(rad), s = std::sin(rad);
    const SkPoint mid{bounds.centerX(), bounds.centerY()};
    const float reach =
        0.5f * (std::abs(bounds.width() * c) + std::abs(bounds.height() * s));
    const float wide =
        SkPoint{bounds.width(), bounds.height()}.length() * 0.5f + 1.0f;
    const float edge = -reach + 2.0f * reach * t;
    SkPathBuilder b;
    b.addRect(SkRect::MakeLTRB(-reach - 1.0f, -wide, edge, wide));
    SkMatrix m = SkMatrix::RotateDeg(angleDeg);
    m.postTranslate(mid.x(), mid.y());
    return b.detach().makeTransform(m);
  };

  // One entry/exit pair. Region gates go on first (they commute with the
  // coverage layers, so the order between the two kinds is free and this
  // one keeps the layer pops simple); coverage layers are popped in
  // reverse, each drawing its Material's alpha through kDstIn.
  std::vector<SkPaint> coverStack;
  const auto enterGates = [&](bool wholeNode, Parts::Bits cls,
                              std::string_view label) -> int {
    if (plane.empty()) return -1;
    int base = -1;
    const auto hit = [&](const Mask& m) {
      if (m.what.isEverything() != wholeNode) return false;
      if (wholeNode) return true;
      return cls == Parts::kMarks ? m.what.selectsMark(label)
                                  : m.what.selects(cls);
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
        canvas.clipPath(edgeRegion(m.with.angleDeg, g.fraction), true);
      } else {  // Shape — and its complement, the missing clipOut()
        canvas.save();
        canvas.clipPath(
            m.with.resolver ? m.with.resolver->clipRegion(m.with, fullOutline)
                            : fullOutline,
            m.with.outside ? SkClipOp::kDifference : SkClipOp::kIntersect,
            true);
      }
    }
    for (const PlaneGate& g : plane) {
      const Mask& m = *g.mask;
      if (!hit(m) || m.with.kind != Gate::Kind::Coverage) continue;
      if (base < 0) base = canvas.getSaveCount();
      const SkRect layerBox = recordBounds(inst);
      canvas.saveLayer(&layerBox, nullptr);
      SkPaint cover;
      cover.setAntiAlias(true);
      // The complement is the blend mode and nothing else: kDstOut is
      // dst·(1 - a), which is 1 - coverage exactly, for any source.
      cover.setBlendMode(m.with.outside ? SkBlendMode::kDstOut
                                        : SkBlendMode::kDstIn);
      if (m.with.coverage) {
        const Fill f = m.with.resolver
                           ? m.with.resolver->coverage(m.with, paintCtx)
                           : Fill{};
        const bool luma = m.with.channel == Gate::Channel::Luma;
        if (f.kind == Fill::Kind::Shader && f.shaderValue)
          cover.setShader(luma ? lumaCoverageShader(f.shaderValue)
                               : f.shaderValue);
        else if (f.kind == Fill::Kind::Color)
          cover.setColor4f(
              luma ? lumaCoverageColor(f.colorValue) : f.colorValue, nullptr);
        else
          cover.setColor4f({0, 0, 0, 0},
                           nullptr);  // Fill::none() shows nothing
      }
      coverStack.push_back(std::move(cover));
    }
    return base;
  };
  const auto leaveGates = [&](int base, size_t coverBase) {
    while (coverStack.size() > coverBase) {
      canvas.drawRect(recordBounds(inst), coverStack.back());
      coverStack.pop_back();
      canvas.restore();
    }
    if (base >= 0) canvas.restoreToCount(base);
  };
  // The whole-node hoist, in wipe()'s old position.
  const size_t hoistCover = coverStack.size();
  const int hoistSaves = enterGates(true, Parts::kAll, {});

  // Span-qualified passes, resolved ONCE per paint however many halves
  // read them: the claim ledger is one ledger (StrokePass), and resolving
  // it twice would also re-walk the boundary three or four times for
  // nothing.
  //
  // THE CLAIM LEDGER READS THE UNMASKED BOUNDARY — `fullOutline`, not the
  // cut path. A claim is a statement about where a mark goes; a gate is a
  // statement about how much of it exists yet. Resolving claims against a
  // shrinking boundary would make the no-overlap diagnostic a function of
  // the clock.
  std::optional<std::vector<std::vector<Span>>> spanClaims;
  auto paintSpanHalf = [&](detail::StrokePass::Half half) {
    if (!node.hasStrokePasses()) return;
    if (!spanClaims)
      spanClaims = node.strokeData->resolver
                       ? node.strokeData->resolver->claims(inst, fullOutline)
                       : std::vector<std::vector<Span>>{};
    const std::vector<detail::StrokePass>& passes = node.strokeData->passes;
    for (size_t i = 0; i < passes.size() && i < spanClaims->size(); ++i) {
      if (passes[i].half != half || (*spanClaims)[i].empty()) continue;
      // …and the gate intersects the claim, which is the whole of
      // `.stroke(spans::corners(18), brk).mask(parts::marks(), upTo(t))`:
      // reticle brackets that light up as a sweep reaches them.
      std::vector<Span> run = (*spanClaims)[i];
      if (marksShow) run = intersect(run, *marksShow);
      if (!passes[i].name.empty())
        for (const auto& [label, show] : namedShow)
          if (label == passes[i].name) run = intersect(run, show);
      if (run.empty()) continue;
      const size_t cover = coverStack.size();
      const int saves =
          granularPlane ? enterGates(false, Parts::kMarks, passes[i].name) : -1;
      const PaintContext passCtx{
          paintCtx.size,
          arith ? arith->spanPath(fullOutline, run) : fullOutline,
          paintCtx.elapsedSeconds,
          paintCtx.contentScale,
          paintCtx.animating,
          paintCtx.fonts,
          paintCtx.borrowed,
          nullptr,  // stamps: deliberately not shared with a span pass
          paintCtx.toRoot,
          paintCtx.rootSize};
      passes[i].what.paint(canvas, passCtx);
      if (granularPlane) leaveGates(saves, cover);
    }
  };

  /** Paint one unqualified mark, under whatever gates address it by name.
   *  The common case — no named mask, no granular plane gate — is the
   *  decoration's own paint call and nothing else. */
  const auto paintMark = [&](const Decoration& d, detail::MarkSlot slot,
                             size_t index) {
    std::string_view label;
    if (node.fxData)
      for (const detail::MarkLabel& l : node.fxData->markNames)
        if (l.slot == slot && l.index == index) {
          label = l.name;
          break;
        }
    const std::vector<Span>* refine = nullptr;
    if (!label.empty())
      for (const auto& [name, show] : namedShow)
        if (name == label) {
          refine = &show;
          break;
        }
    const size_t cover = coverStack.size();
    const int saves =
        granularPlane ? enterGates(false, Parts::kMarks, label) : -1;
    if (refine) {
      std::vector<Span> run = *refine;
      if (marksShow) run = intersect(run, *marksShow);
      const PaintContext markCtx{paintCtx.size,
                                 gateOutline(arith, fullOutline, run),
                                 paintCtx.elapsedSeconds,
                                 paintCtx.contentScale,
                                 paintCtx.animating,
                                 paintCtx.fonts,
                                 paintCtx.borrowed,
                                 nullptr,  // stamps: not shared with a mark
                                 paintCtx.toRoot,
                                 paintCtx.rootSize};
      d.paint(canvas, markCtx);
    } else {
      d.paint(canvas, paintCtx);
    }
    if (granularPlane) leaveGates(saves, cover);
  };

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow and pattern layers first, then the surface.
  // Decorations are NEVER clipped — they dress the outline, so shadows keep
  // their reach and an outer stroke survives on a node that clips its
  // content.
  if (emitOwn) {
    for (size_t i = 0; i < node.backgrounds.size(); ++i)
      paintMark(node.backgrounds[i], detail::MarkSlot::Background, i);
    // Span-qualified BACKGROUND passes land here, in the background half,
    // under the fill and therefore under the content and the children —
    // the z-slot the deleted trim() revealed and a stroke pass could not
    // reach.
    paintSpanHalf(detail::StrokePass::Half::Background);
  }

  // clip() bounds the fill, the content, and the children — not the
  // decorations (above and below), which trace the outline itself.
  if (node.clipContent) {
    canvas.save();
    if (customShape || routed)
      canvas.clipPath(clipShape, true);
    else
      canvas.clipRRect(rrect, true);
  }

  // Fill (background): a live material resolves per frame from its bound
  // uniforms + the PaintContext; otherwise the stored Fill (binding, lerp, or
  // plain).
  std::optional<Fill> resolvedFill;
  if (!emitOwn) {
    // ChildrenOnly: the prefix above already ran into the bake. Skip
    // straight past the fill, the echoes, the overlays and the leaf
    // content to the children loop. (The outline and the clip/wipe/effect
    // wrappers above are recomputed rather than skipped — they are cheap,
    // they must stay balanced against their restores below, and the
    // foregrounds still trace the outline.)
  } else if (const Material* live = liveMaterialOf(node)) {
    resolvedFill = inst.hasPendingLiveFill ? inst.pendingLiveFill
                                           : live->resolve(paintCtx);
  } else if (node.paint.fill) {
    Fill fill;
    if (const choreograph::Output<Fill>* binding = node.paint.fill->binding())
      fill = binding->value();
    else if (inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->value.isConnected()) {
      const float t = inst.anims[Instance::kFillLerp]->value.value();
      fill = inst.fillTo;
      for (int i = 0; i < 4; ++i)
        fill.colorValue.vec()[i] = inst.fillFrom.colorValue.vec()[i] +
                                   (inst.fillTo.colorValue.vec()[i] -
                                    inst.fillFrom.colorValue.vec()[i]) *
                                       t;
      fill.kind = Fill::Kind::Color;
    } else {
      ResolvedProp<Fill> resolved =
          resolveProp(*node.paint.fill, node.nodeTransition);
      fill = resolved.target;
    }
    resolvedFill = fill;
  }

  // The SURFACE — the fill and its echo re-stamps — under whatever gates
  // select `parts::surface()`.
  const size_t surfaceCover = coverStack.size();
  const int surfaceSaves =
      granularPlane && emitOwn ? enterGates(false, Parts::kSurface, {}) : -1;

  // Misprint echoes of the FILL SHAPE, under the real pass (bottom first).
  if (!echoesOf(node).empty() && resolvedFill &&
      resolvedFill->kind != Fill::Kind::None) {
    for (const Echo& e : echoesOf(node)) {
      SkPaint stamp;
      stamp.setAntiAlias(true);
      stamp.setColor4f(e.color, nullptr);
      canvas.save();
      canvas.translate(e.offset.fX, e.offset.fY);
      if (customShape || trimmed)
        canvas.drawPath(surfacePath, stamp);
      else
        canvas.drawRRect(rrect, stamp);
      canvas.restore();
    }
  }

  if (resolvedFill && resolvedFill->kind != Fill::Kind::None) {
    const Fill& fill = *resolvedFill;
    SkPaint paint;
    paint.setAntiAlias(true);
    if (fill.kind == Fill::Kind::Color)
      paint.setColor4f(fill.colorValue, nullptr);
    else
      paint.setShader(fill.shaderValue);
    // Leaf fast path: paint() proved a layer is unnecessary and routed the
    // node's blend/opacity straight onto the fill.
    paint.setBlendMode(leafBlend);
    if (leafOpacity < 1.0f) paint.setAlphaf(paint.getAlphaf() * leafOpacity);
    if (customShape || trimmed)
      canvas.drawPath(surfacePath, paint);
    else
      canvas.drawRRect(rrect, paint);
  }
  if (granularPlane && emitOwn) leaveGates(surfaceSaves, surfaceCover);

  // Overlays: over the fill, under the content and children. The slot a
  // textured button needs so its own hazard stripe does not grey out its
  // label. Unclipped like the other decorations — they dress the outline.
  if (emitOwn && node.fxData)
    for (size_t i = 0; i < node.fxData->overlays.size(); ++i)
      paintMark(node.fxData->overlays[i], detail::MarkSlot::Overlay, i);

  // Content, under whatever gates select parts::content().
  const size_t contentCover = coverStack.size();
  const int contentSaves =
      granularPlane && emitOwn ? enterGates(false, Parts::kContent, {}) : -1;
  if (emitOwn) switch (node.kind) {
      case Kind::Text:
        if (inst.paragraph) {
          // Yoga skips the measure callback when both dimensions are fully
          // determined (absolute + all four insets); lay out on demand at the
          // resolved width so such text still paints. Aligned text (center/
          // end/justify) additionally must be laid out at its FINAL width —
          // lines place within the flow width, so a measure-time constraint
          // that differs from the resolved box would push them off target.
          const bool onPathRun = node.textData && node.textData->onPath;
          // Vertical columns hang off the RIGHT edge of the measure, so the
          // resolved width decides WHERE the first column stands and not
          // only where the text wraps — the same reason aligned text is
          // re-laid here, one axis over.
          const bool verticalRun =
              inst.paragraph && inst.paragraph->writingMode() !=
                                    sigil::weave::WritingMode::kHorizontal;
          if (inst.measuredRev != inst.contentRev ||
              (!onPathRun && node.textData &&
               (verticalRun || node.textData->alignment() !=
                                   sigil::weave::TextAlignment::kStart) &&
               (inst.measuredForWidth != bounds.width() ||
                (verticalRun && inst.measuredForHeight != bounds.height()))))
            layoutText(inst, bounds.width(),
                       verticalRun ? bounds.height() : 1.0e6f);
          // Misprint echoes of the TEXT, under the real pass (fx() text
          // draws its own buckets — echoes skip it by contract).
          if (!echoesOf(node).empty() && !hasTextFx(inst)) {
            for (const Echo& e : echoesOf(node)) {
              sigil::weave::PaintStyle stamp;
              stamp.foreground.setColor4f(e.color, nullptr);
              canvas.save();
              canvas.translate(e.offset.fX, e.offset.fY);
              inst.textLayout.drawBatched(&canvas, *inst.paragraph, &stamp);
              canvas.restore();
            }
          }
          const TextPath* onPath = nullptr;
          if (onPathRun) {
            const std::optional<TextPath>& path = node.textData->onPath;
            if (path.has_value()) onPath = &path.value();
          }
          // textFill()/textStroke() resolve to ONE glyph-paint override,
          // and every draw that takes one takes the SAME one: a letter in
          // flight, and a letter on a curve, are painted exactly as a
          // resting letter is.
          const std::optional<sigil::weave::PaintStyle> metric =
              metricTextStyle(inst, paintCtx);
          const sigil::weave::PaintStyle* glyphPaint =
              metric ? &*metric : nullptr;
          // One draw for both: the baseline places the glyph and the tracks
          // deviate from that placement. Neither wins over the other.
          // Dressed type draws through the painter its description carries;
          // a description that dresses its text without one (built from the
          // data blocks directly, never through a text verb) draws at rest.
          const TextPainterOps* painter = textPainterOf(inst);
          if ((hasTextFx(inst) || onPath) && painter) {
            painter->paint(inst, canvas, glyphPaint, onPath,
                           {bounds.width(), bounds.height()}, paintCtx);
          } else {
            inst.textLayout.drawBatched(&canvas, *inst.paragraph, glyphPaint);
          }
        }
        break;
      case Kind::Image:
        if (imageAssetOf(node) && !imageAssetOf(node)->frames().empty()) {
          const auto& frame = imageAssetOf(node)->frameAt(elapsed() * 1000.0);
          if (frame.image) {
            const SkSamplingOptions sampling = node.imageData->sampling;
            if (node.imageData->region)
              canvas.drawImageRect(frame.image, *node.imageData->region, bounds,
                                   sampling, nullptr,
                                   SkCanvas::kStrict_SrcRectConstraint);
            else
              canvas.drawImageRect(frame.image, bounds, sampling);
          }
        }
        break;
      case Kind::Custom:
        if (node.customData && node.customData->program)
          node.customData->program(canvas, paintCtx);
        break;
      case Kind::Box:
      case Kind::Stack:
      case Kind::Slot:
        break;
    }

  if (granularPlane && emitOwn) leaveGates(contentSaves, contentCover);

  // Children in stacking order (each clean static child replays its own nested
  // picture — ancestor re-records don't repaint clean subtrees).
  const size_t kidsCover = coverStack.size();
  const int kidsSaves = granularPlane && emitChildren
                            ? enterGates(false, Parts::kChildren, {})
                            : -1;
  if (emitChildren)
    for (size_t index : inst.paintOrder) paint(*inst.children[index], canvas);
  if (granularPlane && emitChildren) leaveGates(kidsSaves, kidsCover);

  if (node.clipContent) canvas.restore();  // decorations below stay unclipped

  // FOREGROUNDS PAINT AFTER THE CHILDREN, so they belong to the children
  // half and can never be in an own-paint bake. The own half is the
  // contiguous PREFIX up to the children loop, which is not the same thing
  // as "everything except the children".
  if (emitChildren)
    for (size_t i = 0; i < node.foregrounds.size(); ++i)
      paintMark(node.foregrounds[i], detail::MarkSlot::Foreground, i);

  // Span-qualified stroke passes, in declaration order, in the same slot
  // as the unqualified strokes they append to. Each one paints against
  // the sub-geometry it CLAIMED, so a brush that knows nothing about
  // spans (a PathFormat, a Brush, a brush::Pattern) dresses part of a
  // boundary with no new vocabulary.
  if (emitChildren) paintSpanHalf(detail::StrokePass::Half::Foreground);

  leaveGates(hoistSaves, hoistCover);

  if (hasEffect) canvas.restore();
}

namespace {

/** Promotion thresholds. A node must cost more than this to replay, for
 *  this many consecutive frames, before the library re-bakes it. 1 ms is
 *  ~6% of a 60 FPS frame — well above noise, far below the point where a
 *  sketch is in trouble; 8 frames keeps a one-off stall from promoting
 *  anything. */
constexpr double kPromoteMs = 1.0;
constexpr uint8_t kPromoteFrames = 8;

/** Temporal promotion. A node whose only volatility is a live material may
 *  hold a bake while the material is provably holding still, and re-bakes
 *  when it ticks — which is only a win if it ticks slower than the frame
 *  rate. A bake costs about what the replay it replaces costs, so the
 *  break-even stable fraction is around a half: promote at 0.5, keep until
 *  0.3. A material bound to a continuous output sits at 0 and never gets
 *  close; one quantized to a step slower than the frame rate sits well
 *  above the promote bar. */
constexpr float kStablePromote = 0.5f;
constexpr float kStableKeep = 0.3f;

/** A readable, ACTIONABLE identity for a profile row: the author's own
 *  key() when there is one (that is what they will search for), else the
 *  node kind and its painted size, which is usually enough to find it. */
std::string profileLabel(const detail::Instance& inst, const SkRect& rect) {
  const detail::ElementNode& node = *inst.desc;
  const char* kind = "box";
  switch (node.kind) {
    case detail::Kind::Box:
      kind = "box";
      break;
    case detail::Kind::Text:
      kind = "text";
      break;
    case detail::Kind::Image:
      kind = "image";
      break;
    case detail::Kind::Custom:
      kind = "custom";
      break;
    default:
      break;
  }
  char buf[96];
  std::snprintf(buf, sizeof buf, "%s %.0fx%.0f", kind, rect.width(),
                rect.height());
  if (!node.key.empty()) return node.key + " (" + buf + ")";
  return buf;
}

/** Scoped per-node timer. RAII because paint() has several early returns
 *  and a half-written row would be worse than no row at all. */
struct ProfileScope {
  Composer::Impl* impl = nullptr;
  size_t row = SIZE_MAX;
  double savedChildren = 0;
  std::chrono::steady_clock::time_point start;

  ProfileScope(Composer::Impl* i, const detail::Instance& inst,
               const SkRect& rect)
      : impl(i) {
    if (!impl->profileEnabled) return;
    row = impl->profileRows.size();
    impl->profileRows.push_back(Composer::NodeCost{profileLabel(inst, rect), 0,
                                                   0, impl->profDepth,
                                                   Composer::CacheState::Live});
    savedChildren = impl->profChildMs;
    impl->profChildMs = 0;
    ++impl->profDepth;
    start = std::chrono::steady_clock::now();
  }
  ~ProfileScope() {
    if (row == SIZE_MAX) return;
    const double total = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - start)
                             .count();
    impl->profileRows[row].totalMs = total;
    impl->profileRows[row].selfMs = total - impl->profChildMs;
    // Hand our whole cost up to the parent's child accumulator.
    impl->profChildMs = savedChildren + total;
    --impl->profDepth;
  }
};

}  // namespace

void Composer::Impl::paint(Instance& inst, SkCanvas& canvas) {
  const ElementNode& node = *inst.desc;
  const SkRect rect = instanceRect(inst);
  ProfileScope profileScope(this, inst, rect);

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f) return;

  // (Size-change invalidation for recordings — including geometry-dependent
  // materials' baked uResolution — happens in ensureLayout's
  // syncLayoutRects pass, which sees every relayout; paint() may never reach
  // a node whose ancestor replays a cached picture.)

  canvas.save();
  canvas.translate(rect.left(), rect.top());

  // ONE transform producer for the resolver's lanes: concatTo() is
  // matrix()'s op list applied as elementary canvas ops — byte-exactness
  // demands that sequence, see its comment — while recordBounds()'s child
  // union and hitInstance()'s inverse map and invert the composed matrix()
  // itself.
  const NodeTransform tf = transformOf(inst);
  tf.concatTo(canvas, node.paint, rect.width(), rect.height());

  // Accumulate the node→root matrix alongside the canvas ops — the same
  // T(rect)·matrix() product hitInstance() inverts, so a world-space
  // material draws its field exactly where the hit test says the node is.
  // NOT canvas.getTotalMatrix(): that includes the HOST's transform and any
  // bake-layer offset, and this matrix must stop at the composer root. RAII
  // because paint() returns from several places.
  if (!inst.parent) rootLayoutSize = SkSize{rect.width(), rect.height()};
  struct ToRootScope {
    SkMatrix* slot;
    SkMatrix saved;
    explicit ToRootScope(SkMatrix* s) : slot(s), saved(*s) {}
    ~ToRootScope() { *slot = saved; }
  } toRootScope(&curToRoot);
  curToRoot.preTranslate(rect.left(), rect.top());
  curToRoot.preConcat(
      tf.matrix({0, 0}, node.paint, rect.width(), rect.height()));

  const Effect* backdropFx = backdropEffectOf(node);
  sk_sp<SkImageFilter> backdropFilter;
  if (backdropFx) {
    // A backdrop effect's child materials resolve against the node's box
    // too — the same context the node's own paint builds, minus the marks
    // outline, because nothing of the node has been painted yet. Built
    // INSIDE the branch: every node reaches this line and only a few carry
    // a backdrop.
    const PaintContext backdropCtx{{rect.width(), rect.height()},
                                   SkPath(),
                                   elapsed(),
                                   hostScale,
                                   ticker.active(),
                                   &fonts,
                                   nullptr,
                                   &inst.stampCache,
                                   curToRoot,  // this node→root
                                   rootLayoutSize};
    backdropFilter = backdropFx->resolvedImageFilter(&backdropCtx);
  }
  const bool hasBackdrop = (bool)backdropFilter;
  if (hasBackdrop) {
    // The filtered backdrop composites as a CLOSED pass clipped to the
    // node's shape — the node's own decorations and overflowing children
    // then paint unclipped above it (CSS clips the FILTER REGION to the
    // element, not the element's overflow).
    canvas.save();
    if (node.shapeFn)
      canvas.clipPath(resolveOutline(inst, {rect.width(), rect.height()}),
                      true);
    else
      canvas.clipRRect(cornersRRect(SkRect::MakeWH(rect.width(), rect.height()),
                                    node.corners),
                       true);
    SkCanvas::SaveLayerRec rec(nullptr, nullptr, backdropFilter.get(), 0);
    canvas.saveLayer(rec);
    canvas.restore();  // composite the filtered backdrop through the clip
    canvas.restore();  // release the clip — content is NOT bounded by it
  }

  // The live-material resolve probe: when the node's only volatility is its
  // live material, resolve NOW — an unchanged shader means the cached
  // picture is still exact and simply replays, so the node repaints at the
  // material's own rate rather than the frame rate.
  bool liveStable = false;
  inst.hasPendingLiveFill = false;
  if (inst.liveMatOnly && liveMaterialOf(node)) {
    PaintContext probe{{rect.width(), rect.height()},
                       SkPath(),
                       elapsed(),
                       hostScale,
                       ticker.active(),
                       &fonts,
                       nullptr,
                       nullptr,
                       curToRoot,  // so the memo digest sees this move
                       rootLayoutSize};
    inst.pendingLiveFill = liveMaterialOf(node)->resolve(probe);
    inst.hasPendingLiveFill = true;
    liveStable = (inst.picture || inst.textureImage) && !inst.paintDirty &&
                 inst.pendingLiveFill.shaderValue == inst.bakedLiveShader;
    // The temporal-stability estimate. Material::resolve() memoizes on the
    // byte-identical digest of every varying input, so a stable shader
    // POINTER is a proof that the quantized inputs have not ticked — and
    // therefore that the pixels of the last bake are still the pixels this
    // frame wants. EMA, so one tick does not cost the promotion.
    inst.liveStableRate =
        inst.liveStableRate * 0.75f + (liveStable ? 0.25f : 0.0f);
  }

  // The scalar memo's probe: the animated content scalars AS OF THIS FRAME.
  // Same argument as the material's — identical inputs mean identical
  // pixels, so a recording made with these numbers is still exact while
  // they hold.
  Instance::ContentScalars scalarsNow;
  if (inst.scalarMemo) {
    // Every mask gate's animated numbers, as a bounded per-node list, so a
    // masked node can take this memo at all.
    scalarsNow.gates = inst.resolveGateValues();
    scalarsNow.tracks = inst.resolveTrackValues();
    // The node→root matrix as of THIS paint — curToRoot is exactly it here,
    // and the walk-side compares (release, scan) recompute it
    // bit-identically.
    if (inst.hasWorldSpaceMaterial)
      scalarsNow.world = {curToRoot.getScaleX(),     curToRoot.getSkewX(),
                          curToRoot.getTranslateX(), curToRoot.getSkewY(),
                          curToRoot.getScaleY(),     curToRoot.getTranslateY()};
    // The bound fill, through the SAME body the walk and scan call — the
    // value the recording below bakes is this frame's binding read, so the
    // memo compares exactly the Fill the recording was baked with.
    scalarsNow.fill = inst.resolveBoundFill();
    // The bound tile pan, under the same one-body rule: the recording bakes
    // the fill shader translated by exactly this read, so the memo compares
    // the pan it was baked with.
    scalarsNow.pattern = inst.resolvePatternOffset();
    // …and onPath()'s phase, on the same rule: the recording bakes the
    // glyph positions this phase produced.
    scalarsNow.pathAt = inst.resolvePathAt();
  }
  const bool scalarsStable = inst.scalarMemo && !inst.paintDirty &&
                             (inst.picture || inst.textureImage) &&
                             scalarsNow == inst.bakedScalars;
  // The settle warmup, write side: count consecutive stable paints, and on
  // crossing the bar request ONE volatility recompute — that walk performs
  // the actual release and registers the node for the per-draw movement
  // scan. Any instability resets the warmup, so a binding that is genuinely
  // moving pays nothing for this machinery beyond the compare.
  if (inst.scalarMemo) {
    if (scalarsStable) {
      inst.settledScalars = scalarsNow;
      if (inst.settleFrames < Instance::kScalarSettleFrames) {
        ++inst.settleFrames;
        if (inst.settleFrames == Instance::kScalarSettleFrames)
          volatileDirty = true;
      }
    } else {
      inst.settleFrames = 0;
    }
  }
  // "May this node keep its cached pixels?" — either nothing about it is
  // volatile, or every input it reads is memoized and provably unchanged.
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  const bool cacheHolds = !inst.subtreeVolatile || memoized;
  // …and "are they still the RIGHT pixels?" — the two memos answer for
  // their own input and abstain on the other.
  const bool memoStale =
      (inst.liveMatOnly && !liveStable) || (inst.scalarMemo && !scalarsStable);

  // Fill-only leaves route blend/opacity straight onto the fill paint instead
  // of a (device-clip-sized!) saveLayer — a field of plus-blended shapes costs
  // path draws, not full-canvas layers. Excluded: live opacity (must stay
  // outside any cached recording) and texture bakes (blending must hit the
  // real destination, not the bake's transparent surface).
  const bool opacityLive =
      node.paint.opacity.binding() != nullptr ||
      (inst.anims[Instance::kOpacity] &&
       inst.anims[Instance::kOpacity]->value.isConnected());
  const bool leafDirectBlend =
      (node.kind == Kind::Box || node.kind == Kind::Stack) &&
      inst.children.empty() && node.backgrounds.empty() &&
      node.foregrounds.empty() && !node.hasStrokePasses() &&
      (!node.fxData ||
       (node.fxData->overlays.empty() && node.fxData->masks.empty())) &&
      !layerEffectOf(node) && !backdropEffectOf(node) && !node.clipContent &&
      !opacityLive && node.cacheMode != Cache::Texture &&
      node.cacheMode != Cache::Group;  // (same reason: bakes isolate)
  // A texture-cached node composites exactly ONE draw — its blit — so its
  // blend and opacity can ride that draw's paint instead of a
  // device-clip-sized saveLayer. Cheaper, and slightly more exact: no
  // full-canvas intermediate, and one fewer 8-bit requantisation.
  //
  // The predicate is EXACT by construction: it is the texture branch's own
  // entry condition (the memo probes above are hoisted so cacheHolds is
  // known here), and every exit of that branch ends in a single image draw
  // — the device blit, or the quantized-local blit it falls back to. A node
  // that fails the entry keeps the layer, so nothing can lose its blend.
  const bool deferBlendToBlit =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend && !liveOnly && cacheHolds &&
      node.cacheMode == Cache::Texture && !backdropEffectOf(node);
  const bool needsLayer =
      (opacity < 1.0f || node.paint.blendMode != SkBlendMode::kSrcOver) &&
      !leafDirectBlend && !deferBlendToBlit;
  if (needsLayer) {
    SkPaint layerPaint;
    layerPaint.setAlphaf(opacity);
    layerPaint.setBlendMode(node.paint.blendMode);
    // BOUNDED like the effect layer: nullptr would allocate a clip-sized
    // (often full-canvas) layer for every fading container, so an entrance
    // opacity ramp would cost a fullscreen composite per animated group.
    const SkRect content = recordBounds(inst);
    canvas.saveLayer(&content, &layerPaint);
  }
  const SkBlendMode leafBlend =
      leafDirectBlend ? node.paint.blendMode : SkBlendMode::kSrcOver;
  const float leafOpacity = leafDirectBlend ? opacity : 1.0f;

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target pixel
  // win — replaying a picture re-rasterizes, blitting doesn't).
  // COMPOSE_PROF=<ms> prints any draw above the threshold — cached-texture
  // blits, picture replays (which re-EXECUTE recorded ops on raster), and
  // live paints. Nested lines overlap (inclusive of children); any
  // unparsable value means 4ms.
  static const double kProfMs = [] {
    const char* env = getenv("COMPOSE_PROF");
    if (!env) return -1.0;
    const double v = std::strtod(env, nullptr);
    return v > 0.0 ? v : 4.0;
  }();
  const auto profDraw = [&](const char* what, auto&& draw) {
    if (kProfMs < 0.0) {
      draw();
      return;
    }
    const auto t0 = std::chrono::steady_clock::now();
    draw();
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    if (ms > kProfMs)
      SkDebugf("[prof] %s %s kind=%d rect=%.0fx%.0f %.1fms\n", what,
               node.key.empty() ? "(anon)" : node.key.c_str(), (int)node.kind,
               rect.width(), rect.height(), ms);
  };

  // ---- automatic texture promotion -----------------------------------
  // Eligibility is deliberately narrow. Everything here is a condition
  // under which a device-aligned bake is provably the same pixels as the
  // replay; anything else keeps replaying. See
  // Composer::setAutoTexturePromotion.
  const SkMatrix& totalM = canvas.getTotalMatrix();
  // Upright, unmirrored, unrotated and unskewed. It is tempting to drop
  // this: a device-space bake concatenates the full matrix into the layer
  // and blits with the matrix reset at an integer offset, so it cannot
  // resample and ought to be exact at any angle. It is not, for two
  // separate reasons, and automatic promotion is held to exact agreement
  // because the author never asked for it.
  //
  //  - A shader's local coordinates come from INVERTING the CTM, and the
  //    layer's CTM differs from the canvas's by an integer device
  //    translation. Inverting a rotation maps that integer offset through
  //    irrational entries, so the cancellation is only approximate, while
  //    an axis-aligned matrix maps it through ±1 and 0 and cancels exactly.
  //    Off-axis, shaded pixels land about one least-significant bit away.
  //  - A bake rect larger than the device clip hands Skia a different clip
  //    to rasterize the antialiased edges against. That one is worth many
  //    levels, not one, and it shows up wherever a rotated node's bounds
  //    overflow the canvas.
  //
  // A test that rotates a plain colour fill small enough to fit exercises
  // neither effect and will pass with this gate removed. `Cache::Texture`
  // is opt-in and does accept the trade, which is why the refusal message
  // points an author there rather than describing the geometry — a node
  // held off promotion by a constant fraction-of-a-degree tilt needs to be
  // told what to do about it.
  const bool upright = totalM.getSkewX() == 0 && totalM.getSkewY() == 0 &&
                       totalM.getScaleX() > 0 && totalM.getScaleY() > 0 &&
                       !totalM.hasPerspective();
  // recordBounds() walks the whole subtree, and three tiers below ask for
  // it. Memoised per paint() so the walk happens at most once; lazy so a
  // node that reaches none of them never pays for it at all.
  SkRect localPaintBounds = SkRect::MakeEmpty();
  bool localBoundsDone = false;
  const auto localBoundsOf = [&]() -> const SkRect& {
    if (!localBoundsDone) {
      localBoundsDone = true;
      localPaintBounds = recordBounds(inst);
    }
    return localPaintBounds;
  };
  const auto deviceRectOf = [&] {
    const SkRect f = totalM.mapRect(localBoundsOf());
    return SkIRect::MakeLTRB(
        (int)std::floor(f.left()), (int)std::floor(f.top()),
        (int)std::ceil(f.right()), (int)std::ceil(f.bottom()));
  };
  // The temporal rule: a node whose ONLY volatility is a live material is
  // promotable while that material is provably holding still, and re-bakes
  // when it ticks. Sticky, with hysteresis, so a material sitting near the
  // threshold does not promote and demote on alternate frames.
  const bool temporallyStable =
      inst.liveMatOnly &&
      inst.liveStableRate >= (inst.autoTexture ? kStableKeep : kStablePromote);
  const bool contentStable = !inst.subtreeVolatile || temporallyStable;

  // Every refusal below is a condition under which a bake would produce
  // DIFFERENT PIXELS, or would not pay for itself. Naming them is not
  // decoration: a node reported as expensive live paint with no reason
  // beside it gives an author nothing to act on.
  //
  // A BIT MASK, so ALL refusals are reported, not the first. A first-match
  // chain would report only `Volatile` for a node that is both volatile and
  // clipped, and an author who fixed the volatility would then meet a
  // second refusal nobody had mentioned. `why` below is derived FROM this
  // mask rather than computed alongside it, so the summary and the full set
  // cannot disagree.
  using Prom = Composer::Promotion;
  uint16_t refusals = 0;
  const auto flag = [&](Prom p) { refusals |= (uint16_t)(1u << (unsigned)p); };
  // autoPromoteEffective, not autoPromote: the backend-aware default (off on
  // GPU unless the host asked) is applied in draw(). See ComposeRuntime.h.
  const bool optedOut = !autoPromoteEffective || node.cacheMode != Cache::Auto;
  if (optedOut) flag(Prom::OptedOut);
  if (!contentStable) flag(Prom::Volatile);
  if (leafBlend != SkBlendMode::kSrcOver || leafOpacity < 1.0f)
    flag(Prom::Composited);
  if (layerEffectOf(node) || node.clipContent) flag(Prom::Filtered);
  if (inst.subtreeReadsBackdrop)  // incl. this node's own backdrop()
    flag(Prom::ReadsBackdrop);
  if (rect.width() < 0.5f || rect.height() < 0.5f)
    flag(Prom::TooBig);  // degenerate, not large — same "cannot bake" bucket
  if (!upright) flag(Prom::Transformed);

  // The PRIMARY verdict: the first refusal in the order an author should
  // address them (their own switches first, then content, then geometry).
  static constexpr Prom kRefusalOrder[] = {
      Prom::OptedOut, Prom::Volatile,      Prom::Composited, Prom::Transformed,
      Prom::Filtered, Prom::ReadsBackdrop, Prom::TooBig};
  Prom why = Prom::Cheap;
  for (Prom p : kRefusalOrder)
    if (refusals & (uint16_t)(1u << (unsigned)p)) {
      why = p;
      break;
    }

  // recordingDepth == 0, for the SAME reason the Cache::Texture device path
  // and the split bake check it: a device-space bake blits with
  // canvas.resetMatrix() + drawImage() at an ABSOLUTE device rect, and a
  // picture can be replayed under a different matrix than it was recorded
  // at. Recorded into an ancestor's picture and replayed at a different
  // capture scale, such a blit draws a texture baked for one scale at the
  // coordinates of another — wrong size, wrong place.
  const bool promotable =
      why == Prom::Cheap && !liveOnly && recordingDepth == 0;
  if (!promotable) inst.autoTexture = false;
  const auto note = [&](Prom p) {
    if (profileScope.row != SIZE_MAX) {
      profileRows[profileScope.row].promotion = p;
      profileRows[profileScope.row].refusals = refusals;
    }
  };
  note(why);

  /** What this node cost to paint the way it painted — a picture replay for
   *  a cached subtree, the live draw for a leaf — folded into the rolling
   *  estimate, and the promotion decision taken from it. */
  const auto accrue = [&](double cost) {
    // EMA so one scheduling hiccup neither promotes nor un-promotes.
    inst.replayMs = inst.replayMs * 0.6f + (float)cost * 0.4f;
    if (promotable && inst.replayMs > kPromoteMs) {
      if (inst.hotFrames < 255) ++inst.hotFrames;
      if (inst.hotFrames >= kPromoteFrames) {
        inst.autoTexture = true;
        inst.paintDirty = true;  // force the first bake
      } else {
        note(Prom::Warming);
      }
    } else if (inst.hotFrames > 0) {
      --inst.hotFrames;
    }
  };

  if (promotable && inst.autoTexture) {
    // Bake in DEVICE space, snapped OUT to whole device pixels, then blit
    // with the matrix reset. An integer device translation cannot change
    // rasterisation for an AXIS-ALIGNED matrix — which is what `upright`
    // above is guarding, and why that guard is about exactness rather than
    // about resampling.
    const SkIRect device = deviceRectOf();
    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    // The bake this frame would ADD to what the previous frame already held.
    // A node keeping a bake it already has is never refused for budget —
    // dropping it would only make the next frame re-bake it.
    // max(), not a sum: promotedBytesLast is the previous frame's FULL
    // total and promotedBytes is this frame so far, and the two overlap.
    const bool affordable =
        inst.textureImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    if (device.width() > 0 && device.height() > 0 &&
        area <= int64_t{16} * 1024 * 1024 && affordable) {
      // Re-bake when the recording is stale, when the device rect moved or
      // resized (which is how a transform-SCALE change arrives here), or —
      // the temporal case — when the live material has actually ticked and
      // the baked shader is no longer the one this frame resolves to.
      if (!inst.textureImage || inst.paintDirty || memoStale ||
          inst.textureBakeRect != SkRect::Make(device)) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale, leafBlend, leafOpacity);
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = SkRect::Make(device);
          inst.bakedLiveShader = inst.hasPendingLiveFill
                                     ? inst.pendingLiveFill.shaderValue
                                     : nullptr;
          inst.bakedScalars = scalarsNow;
          inst.paintDirty = false;
          stats.picturesRecorded++;
          stats.texturesBaked++;
        }
      }
      if (inst.textureImage) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX)
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Promoted;
        note(Prom::Promoted);
        canvas.save();
        canvas.resetMatrix();
        canvas.drawImage(inst.textureImage, (float)device.left(),
                         (float)device.top(), SkSamplingOptions());
        canvas.restore();
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    inst.autoTexture = false;  // could not bake — fall through to the picture
    note(Prom::TooBig);
  }

  // ---- the SPLIT bake -----------------------------------------------------
  //
  // Volatility is declared per NODE, and a node gets one verdict. So a
  // static full-canvas ground plane carrying one small child on a bound
  // output is `subtreeVolatile`, nothing about it is cached, and the whole
  // plane is re-rasterized every frame purely so the child can be redrawn
  // on top of it. The node reports "its content changes every frame" when
  // what changes is a child's.
  //
  // THE PIXEL-IDENTITY ARGUMENT, which is NOT promotion's argument.
  // Promotion bakes a whole subtree and blits it in place of everything the
  // node contains; the claim there is "an integer device translation cannot
  // change rasterisation". Here the bake replaces only PART of what the
  // node paints and the children are drawn over the blit afterwards, so the
  // claim needed is:
  //
  //   painting the own layer into a transparent device-aligned surface,
  //   blitting it, then painting the children over the result must produce
  //   the same pixels as painting own-then-children directly.
  //
  //  - The own paint is a run of srcOver draws into a transparent layer,
  //    blitted srcOver at an integer device offset. srcOver is associative,
  //    so the composite is the same composite. (The 8-bit double-rounding
  //    of the intermediate is the one real risk and it is asserted, not
  //    argued — see SplitBakeIsPixelIdenticalAcrossTheChildsMotion, whose
  //    own paint deliberately OVERLAPS itself so intra-layer compositing
  //    actually happens.)
  //  - A child with a non-srcOver blend is FINE here, and this is the one
  //    place the split is safer than promotion: the blit lands BEFORE the
  //    children, so the child resolves against the same destination bytes
  //    either way. Under promotion the child was inside the bake and would
  //    have resolved against transparent black. Same for a child with a
  //    backdrop filter — it samples a blitted copy of identical pixels.
  //  - The real failure is the node's OWN paint reading the backdrop, where
  //    the bake would resolve against transparent black. That is
  //    `ownReadsBackdrop`, which is why the flag was split from
  //    `subtreeReadsBackdrop` before any of this existed.
  //  - A LAYER EFFECT is the exclusion, and it is the only one of the three
  //    wrappers that is. An image filter applies to the UNION of own paint
  //    and children; filtering the own half alone and drawing the children
  //    over the result is a different picture.
  //  - clipContent and a whole-node mask() are NOT excluded, though it
  //    looks as though they should be. They wrap both halves, and the phase
  //    flag skips only the CONTENT — the clip is opened and closed inside
  //    EACH phase, so both halves get the identical clip in the identical
  //    device geometry and the composition is unchanged. A GRANULAR mask is
  //    narrower still: its scope is entered and left around one paint
  //    group, inside the half that group belongs to. Excluding clips would
  //    refuse the most common shape this feature exists for, since a
  //    backdrop that clips its moving child to an outline is exactly why
  //    that child is a separate node.
  //
  // And the promotion is judged on the OWN paint alone. A split candidate
  // paints in two phases from the first eligible frame precisely so that
  // half can be timed by itself: judging it by the node's total would
  // promote a cheap ground plane because it carries an expensive child, and
  // would leave an expensive plane unpromoted under a cheap one.
  const bool splitCandidate =
      !optedOut && !liveOnly && inst.subtreeVolatile &&
      !inst.ownContentVolatile &&  // the CHILDREN are what block this node
      !inst.children.empty() && !inst.ownReadsBackdrop &&
      !layerEffectOf(node) && leafBlend == SkBlendMode::kSrcOver &&
      leafOpacity >= 1.0f && rect.width() >= 0.5f && rect.height() >= 0.5f &&
      recordingDepth == 0 && !inst.transformLive &&
      // `upright` for the same reason promotion needs it, and it is the
      // SAME construction: an integer device offset concatenated onto the
      // node's matrix. Under rotation a shader's local coordinates come
      // back through an inverse that cannot cancel that offset exactly, and
      // the antialiased edges land about a least-significant bit apart.
      // Leaving it out would hold the split to a weaker standard than the
      // promoter beside it.
      upright;
  if (!splitCandidate) inst.splitBake = false;
  if (splitCandidate) {
    // ownPaintBounds, NOT recordBounds. recordBounds unions the children
    // in, so it moves every frame a child moves — and a bake rect that
    // moves every frame is a bake remade every frame, which is the one
    // failure mode that would make this feature cost more than it saves on
    // precisely the scenes it exists for. The own paint's extent does not
    // depend on the children at all.
    const SkRect ownF = totalM.mapRect(ownPaintBounds(inst));
    const SkIRect device = SkIRect::MakeLTRB(
        (int)std::floor(ownF.left()), (int)std::floor(ownF.top()),
        (int)std::ceil(ownF.right()), (int)std::ceil(ownF.bottom()));
    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    const bool affordable =
        inst.ownImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    bool blitted = false;
    if (inst.splitBake && device.width() > 0 && device.height() > 0 &&
        area <= int64_t{16} * 1024 * 1024 && affordable) {
      // `ownPaintDirty`, NOT `paintDirty`. markPaintDirtyUp() propagates a
      // descendant's patch to every ancestor, which is right for a
      // recording (it baked the child's draw calls) and wrong here: the
      // children were never in this bake, and the whole point is that they
      // change. If that ever inverts, the feature silently does nothing and
      // still passes every pixel test.
      const SkRect want = SkRect::Make(device);
      if (!inst.ownImage || inst.ownPaintDirty || inst.ownBakeRect != want) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale, leafBlend, leafOpacity,
                       Phase::OwnOnly);
          inst.ownImage = layer->makeImageSnapshot();
          inst.ownBakeRect = want;
          inst.ownPaintDirty = false;
          stats.texturesBaked++;
          // A bake per frame costs MORE than the live draw it replaced, so
          // a node whose own paint really is being invalidated every frame
          // must not hold the promotion on the strength of a measurement
          // taken while it was still cheap. Three consecutive re-bakes and
          // it goes live and has to earn it again over the full warmup.
          if (inst.ownRebakes < 255) ++inst.ownRebakes;
          if (inst.ownRebakes > 3) {
            inst.splitBake = false;
            inst.ownHotFrames = 0;
            inst.ownRebakes = 0;
          }
        }
      } else {
        inst.ownRebakes = 0;
      }
      if (inst.ownImage && inst.splitBake) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX)
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::SplitOwn;
        note(Prom::SplitBaked);
        canvas.save();
        canvas.resetMatrix();
        profDraw("split blit", [&] {
          canvas.drawImage(inst.ownImage, (float)device.left(),
                           (float)device.top(), SkSamplingOptions());
        });
        canvas.restore();
        blitted = true;
      }
    }
    if (!blitted) {
      // The own half, live and TIMED. This is the number the split is
      // promoted on — the node's own paint, with its children excluded by
      // construction rather than by subtraction.
      const auto ownStart = std::chrono::steady_clock::now();
      profDraw("live own", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity,
                     Phase::OwnOnly);
      });
      const double ownMs = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - ownStart)
                               .count();
      inst.ownPaintMs = inst.ownPaintMs * 0.6f + (float)ownMs * 0.4f;
      if (inst.ownPaintMs > kPromoteMs) {
        if (inst.ownHotFrames < 255) ++inst.ownHotFrames;
        if (inst.ownHotFrames >= kPromoteFrames) {
          inst.splitBake = true;
          inst.ownPaintDirty = true;  // force the first bake
        } else {
          note(Prom::Warming);
        }
      } else if (inst.ownHotFrames > 0) {
        --inst.ownHotFrames;
      }
    }
    // The children and the foregrounds over them — always live, whatever
    // happened above. Foregrounds paint AFTER the children, so they are in
    // this half and never in the bake.
    paintContent(inst, canvas, hostScale, leafBlend, leafOpacity,
                 Phase::ChildrenOnly);
    stats.nodesPainted++;
    inst.paintDirty = false;
    if (needsLayer) canvas.restore();
    canvas.restore();
    return;
  }

  // ---- Cache::Group — the whole subtree, held by a VALUE memo -------------
  //
  // The shape of the problem this exists for: MANY SMALL ROTATED PIECES
  // FORMING ONE STATIC ASSEMBLY, each piece carrying a bound entrance that
  // runs for a while and then holds. Nothing in that description is
  // cacheable by the volatility rule, because the bindings never
  // disconnect, and everything in it is cacheable for every frame the
  // entrance is not running.
  //
  // WHY THE BAKE IS THE EASY HALF. This is the same construction the device
  // path below and whole-subtree promotion already use: paintContent into a
  // transparent layer whose canvas carries the node's exact matrix offset by
  // an INTEGER device translation, then blit with the matrix reset. The
  // children's rotations, their bevels and their mutual compositing all
  // happen INSIDE that bake at full precision, which is what makes it
  // pixel-safe where per-piece Cache::Texture is not: baking each piece
  // separately isolates it into its own layer, so every shared edge and
  // abutment resolves against transparent black instead of against its
  // neighbour.
  //
  // WHY THE INVALIDATION IS THE HARD HALF, AND THE WHOLE FEATURE. A group
  // may hold a bake only while it is provably not changing, and "not
  // changing" cannot be read off the volatility verdict — that verdict says
  // Volatile forever, correctly. So the group compares VALUES, the same way
  // the per-node scalar memo does, generalised to a whole subtree's bound
  // transforms and opacities. Every frame: gather them, compare with last
  // frame's, and on any difference at all DROP THE BAKE and paint live. A
  // bake taken while the entrance is running would freeze the entrance, and
  // would look completely correct in any still frame.
  //
  // The refusals are in computeVolatile (`groupRootOK`), because they are
  // about what the memo can SEE, not about this frame.
  if (!liveOnly && inst.groupRootOK && recordingDepth == 0) {
    // Gather, compare, and become last frame — in that order. The swap is
    // what makes a settled group allocate nothing: `groupScratch` comes back
    // holding the vector that was `groupPrev`, at the right capacity.
    groupScratch.clear();
    collectGroupScalars(inst, /*root=*/true, groupScratch);
    const bool settled = inst.groupPrevSeen && groupScratch == inst.groupPrev;
    std::swap(inst.groupPrev, groupScratch);
    inst.groupPrevSeen = true;

    // The device rect, and the two "is it holding still" questions the
    // device path below asks for its own reasons — they are the same
    // questions here. `transformLive` is the node's own declared motion; the
    // rect comparison catches the motions no declaration can see (a resizing
    // host, a pinch zoom, an uncached ancestor's live transform). A bake
    // pinned to a rect that moves is a bake remade every frame, which costs
    // strictly more than the paint it replaces.
    // THE BAKE RECT IS CLIPPED TO THE CANVAS, and this is not an
    // optimisation — it is a correctness condition. A bake rect LARGER than
    // the device clip hands Skia a different clip to rasterize antialiased
    // edges against, and the resulting difference is many levels deep, not
    // the single least-significant bit an integer offset under rotation
    // costs. A lattice of rotated pieces with any bleed overruns its own
    // canvas on all four sides, so this fires on exactly the content the
    // feature exists for.
    //
    // Nothing visible is lost — content outside the device clip does not
    // reach the canvas either way — and `getDeviceClipBounds()` is in base
    // device coordinates, the same space the blit's resetMatrix() draws in,
    // including inside the saveLayer an opacity/blend group opens.
    SkIRect device = deviceRectOf();
    const SkIRect clip = canvas.getDeviceClipBounds();
    if (!device.intersect(clip)) device = SkIRect::MakeEmpty();
    const bool rectStable =
        !inst.deviceRectSeen || device == inst.lastDeviceRect;
    inst.lastDeviceRect = device;
    inst.deviceRectSeen = true;

    // THE DROP. Not "re-bake": a group whose bindings are ticking is
    // ticking for a while, and re-baking each of those frames would pay the
    // bake on top of the paint. Hold the pixels only while they are right.
    if (!settled || inst.paintDirty) inst.textureImage.reset();

    const int64_t area = (int64_t)device.width() * device.height();
    const size_t bytes = (size_t)std::max<int64_t>(area, 0) * 4;
    const bool affordable =
        inst.textureImage ||
        std::max(promotedBytesLast, promotedBytes) + bytes <= kPromotedBudget;
    if (settled && !inst.paintDirty && !inst.transformLive && rectStable &&
        !totalM.hasPerspective() && device.width() > 0 && device.height() > 0 &&
        area <= int64_t{16} * 1024 * 1024 && affordable) {
      const SkRect want = SkRect::Make(device);
      if (!inst.textureImage || !inst.textureDeviceSpace ||
          inst.textureBakeRect != want) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          // No leaf blend and no leaf opacity: bakes isolate, and the node's
          // own blend/opacity are applied by the saveLayer wrapping the blit
          // — which is why leafDirectBlend excludes Cache::Group.
          paintContent(inst, *lc, hostScale);
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = want;
          inst.textureScale = maxScaleOf(totalM, localBoundsOf());
          inst.paintDirty = false;
          // A group root never replays a recording. It can have made one on
          // its very first frame — before it had a previous frame to compare
          // with, a group with a fully static subtree falls through to the
          // picture branch once — and holding it after that is bytes nobody
          // will ever read.
          inst.picture.reset();
          stats.picturesRecorded++;
          stats.texturesBaked++;
        }
      }
      if (inst.textureImage) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX) {
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Group;
          profileRows[profileScope.row].promotion =
              Composer::Promotion::AskedFor;
        }
        canvas.save();
        canvas.resetMatrix();
        profDraw("group blit", [&] {
          canvas.drawImage(inst.textureImage, (float)device.left(),
                           (float)device.top(), SkSamplingOptions());
        });
        canvas.restore();
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    // Falls through: `cacheHolds` is false for a volatile group root, so the
    // picture branch below cannot take it either and the node paints LIVE.
    // That is the intended outcome on a ticking frame — the same paint the
    // scene did before this feature existed.
  }

  if (!liveOnly && cacheHolds && node.cacheMode == Cache::Texture &&
      !backdropEffectOf(node)) {
    // ---- the exact bake -------------------------------------------------
    // A bake held in LOCAL space and blitted through the node's transform
    // is resampled by whatever that transform is: at a quarter turn the
    // texel grid lands half a texel off the device grid and every sample
    // interpolates two texels, which softens edges and flattens gradients
    // across the axis whose device edge falls on a half pixel. Baking at
    // the correct scale is necessary for sharpness but not sufficient.
    //
    // Baking in DEVICE space, snapped OUT to whole device pixels and
    // blitted with the matrix reset, has nothing left to resample: the
    // texel grid IS the device grid, at any angle. Two conditions gate it,
    // and both are about not throwing away what the local bake is FOR:
    //
    //  - bakeScale must be 1. Its whole purpose is to rasterize BELOW
    //    device resolution and let the blit stretch it back.
    //  - we must not be inside a picture recording, because a device rect
    //    is not matrix-independent and a picture can replay elsewhere.
    //    This condition is also what makes the next one SOUND: every node
    //    that reaches the device path is painted every frame, so it has
    //    the history the next condition reads. A node painted once, into
    //    an ancestor's recording, is excluded before we get there.
    //  - the node must be HOLDING STILL, by both available measures, which
    //    are not the same measure:
    //      * `transformLive` — its own transform is declared as animating.
    //        A spinning ornament must keep the local bake and ride it,
    //        even on a frame where it happens to land on the same rect.
    //      * the device rect it lands on has not moved since last frame.
    //        A node with no animated property of its own still moves under
    //        a resizing window, a pinch zoom, a pan, or an uncached
    //        ancestor's live transform — none of which any per-node
    //        DECLARATION can see, and all of which would re-bake a
    //        device-pinned texture every frame.
    //    While either says "moving", the quantized local bake is correct
    //    and cheap: one bake per coarse scale step, reused across the rest.
    const SkRect localBounds = localBoundsOf();
    bool deviceRectStable = false;
    SkIRect deviceR = SkIRect::MakeEmpty();
    if (recordingDepth == 0) {
      deviceR = deviceRectOf();
      deviceRectStable = !inst.deviceRectSeen || deviceR == inst.lastDeviceRect;
      inst.lastDeviceRect = deviceR;
      inst.deviceRectSeen = true;
    }
    const int64_t deviceArea = (int64_t)deviceR.width() * deviceR.height();
    if (!inst.transformLive && deviceRectStable && recordingDepth == 0 &&
        node.bakeScale >= 1.0f && !totalM.hasPerspective() &&
        deviceR.width() > 0 && deviceR.height() > 0 &&
        deviceArea <= int64_t{16} * 1024 * 1024) {
      const SkRect bakeRect = SkRect::Make(deviceR);
      if (!inst.textureImage || inst.paintDirty || !inst.textureDeviceSpace ||
          memoStale || inst.textureBakeRect != bakeRect) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(deviceR.width(), deviceR.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(deviceR.width(), deviceR.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          lc->translate(-(float)deviceR.left(), -(float)deviceR.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          paintContent(inst, *lc, hostScale);  // no leaf blend: bakes isolate
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureDeviceSpace = true;
          inst.textureBakeRect = bakeRect;
          inst.textureScale = maxScaleOf(totalM, localBounds);
          inst.bakedLiveShader = inst.hasPendingLiveFill
                                     ? inst.pendingLiveFill.shaderValue
                                     : nullptr;
          inst.bakedScalars = scalarsNow;
          inst.paintDirty = false;
          stats.picturesRecorded++;
          stats.texturesBaked++;
        }
      }
      if (inst.textureImage && inst.textureDeviceSpace) {
        if (profileScope.row != SIZE_MAX) {
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Texture;
          profileRows[profileScope.row].promotion =
              Composer::Promotion::AskedFor;
        }
        // Identity CTM is global canvas space even inside a saveLayer (the
        // layer device carries its own origin), so an opacity/blend bake
        // still composites through the layer above.
        canvas.save();
        canvas.resetMatrix();
        profDraw("blit", [&] {
          if (deferBlendToBlit) {
            // The node's blend and opacity on the ONE draw it composites
            // as — cheaper and slightly MORE exact than the layer it
            // replaces: no full-canvas intermediate, one less rounding.
            SkPaint blit;
            blit.setAlphaf(opacity);
            blit.setBlendMode(node.paint.blendMode);
            canvas.drawImage(inst.textureImage, (float)deviceR.left(),
                             (float)deviceR.top(), SkSamplingOptions(), &blit);
          } else {
            canvas.drawImage(inst.textureImage, (float)deviceR.left(),
                             (float)deviceR.top(), SkSamplingOptions());
          }
        });
        canvas.restore();
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    // Rasterize at the canvas's current scale so zoomed hosts stay crisp — but
    // quantized UP to a coarse step, so a continuously changing scale (window
    // resize, pinch zoom) reuses one bake per step instead of re-rasterizing
    // every frame. Between steps the draw minifies slightly, which stays sharp.
    SkMatrix total = canvas.getTotalMatrix();
    // maxScaleOf, NOT the matrix diagonal: a quarter-turned node's diagonal
    // is (0, 0) and would clamp to the 0.25 floor, baking at a quarter
    // resolution to be upscaled by the blit (see maxScaleOf in
    // ComposeRuntime.h). The node's local bounds locate the Jacobian
    // samples when the CTM carries a host perspective. This ladder feeds
    // the re-bake test below, so an underestimate here means a stale,
    // blurry bake rather than a wasted one.
    const float raw = std::clamp(maxScaleOf(total, localBounds), 0.25f, 4.0f);
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f,  2.0f, 3.0f,  4.0f};
    float scale = kBakeSteps[std::size(kBakeSteps) - 1];
    for (float step : kBakeSteps)
      if (step >= raw) {
        scale = step;
        break;
      }
    // bakeScale(): opt-in reduced raster scale — the bake evaluates fewer
    // pixels and the blit below linear-upscales through the same dst rect.
    scale = std::max(0.1f, scale * node.bakeScale);
    // Bake the full PAINT bounds, not just the box — decoration bleed and
    // overflowing children truncate otherwise (same rule as the picture
    // cull).
    const SkRect bake = localBounds;
    if (!inst.textureImage || inst.paintDirty || inst.textureScale != scale ||
        inst.textureDeviceSpace || memoStale || inst.textureBakeRect != bake) {
      const int pw = std::max(1, (int)std::ceil(bake.width() * scale));
      const int ph = std::max(1, (int)std::ceil(bake.height() * scale));
      sk_sp<SkSurface> layer =
          canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
      if (!layer)
        layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
      layer->getCanvas()->scale(scale, scale);
      layer->getCanvas()->translate(-bake.left(), -bake.top());
      paintContent(inst, *layer->getCanvas(), scale);  // no leaf blend:
      inst.textureImage = layer->makeImageSnapshot();  // bakes isolate
      inst.textureScale = scale;
      inst.textureDeviceSpace = false;
      inst.textureBakeRect = bake;
      inst.bakedLiveShader =
          inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
      inst.bakedScalars = std::move(scalarsNow);
      inst.paintDirty = false;
      stats.picturesRecorded++;
      stats.texturesBaked++;
    }
    if (profileScope.row != SIZE_MAX) {
      profileRows[profileScope.row].cacheState = Composer::CacheState::Texture;
      profileRows[profileScope.row].promotion = Composer::Promotion::AskedFor;
    }
    // Blit through the rect the bake ACTUALLY covers, not `bake`: pw/ph were
    // rounded UP, so stretching an image of ceil(w·s) texels across w local
    // units resamples the whole node by up to one texel's worth of scale.
    // The overshoot is transparent padding, so nothing new becomes visible.
    const SkRect dst = SkRect::MakeXYWH(
        bake.left(), bake.top(),
        (float)inst.textureImage->width() / inst.textureScale,
        (float)inst.textureImage->height() / inst.textureScale);
    profDraw("blit", [&] {
      if (deferBlendToBlit) {
        SkPaint blit;  // same rule as the device blit above
        blit.setAlphaf(opacity);
        blit.setBlendMode(node.paint.blendMode);
        canvas.drawImageRect(inst.textureImage, dst,
                             SkSamplingOptions(SkFilterMode::kLinear), &blit);
      } else {
        canvas.drawImageRect(inst.textureImage, dst,
                             SkSamplingOptions(SkFilterMode::kLinear));
      }
    });
  } else if (!liveOnly && cacheHolds && node.cacheMode != Cache::None &&
             // A zero-sized node (auto-height layout() containers, spacer
             // shims) must NOT record. NOT because an empty cull rect
             // rejects ops — it does not, see the note on ownPaintBounds —
             // but because the recording is pure overhead and it opens a
             // recordingDepth scope around the subtree, which is an input
             // to promotion. Control: delete this size
             // test and ComposeCache.{PromotionRefusesASubtreeThatBlends
             // WithTheCanvas, TheBlendingChildIsWhatCausesTheRefusal,
             // PromotionRefusesABackdropFilter} all fail. Painted live
             // instead — its children keep their own per-node caches, so
             // the cost is one traversal shim.
             rect.width() >= 0.5f && rect.height() >= 0.5f &&
             (node.cacheMode == Cache::Picture || !inst.children.empty() ||
              node.kind == Kind::Text || node.kind == Kind::Custom ||
              !node.backgrounds.empty() || !node.foregrounds.empty() ||
              node.hasStrokePasses() ||
              (node.fxData && !node.fxData->overlays.empty()) ||
              layerEffectOf(node) || memoized)) {
    // (liveMatOnly bare boxes DO record — the memo's point is replaying
    // the rasterized shader while resolve() stays stable.)
    // (Childless Image leaves deliberately absent: one drawImageRect is
    // cheaper than a nested picture indirection — tile maps stay flat inside
    // their chunk's recording. Cache::Picture opts back in.)
    if (!inst.picture || inst.paintDirty || memoStale ||
        inst.bakedLeafOpacity != leafOpacity ||
        inst.bakedLeafBlend != leafBlend) {
      // The same rect the layers and bakes use. Its job HERE is only to be
      // an honest bounds advertisement (SkPicture::cullRect) — this path
      // attaches no BBH, so nothing is culled against it either at record
      // or at playback; see the note on ownPaintBounds for the measurement.
      const SkRect cull = recordBounds(inst);
      SkPictureRecorder recorder;
      SkCanvas* rec = recorder.beginRecording(cull);
      // A picture can be replayed under a DIFFERENT matrix than it was
      // recorded at (an ancestor with a live transform keeps its picture
      // and replays it under the motion). Anything inside must therefore
      // be matrix-independent — which a device-space bake, snapped to one
      // particular device rect, is not.
      ++recordingDepth;
      paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
      --recordingDepth;
      inst.picture = recorder.finishRecordingAsPicture();
      inst.bakedLeafOpacity = leafOpacity;  // a settled transition re-bakes
      inst.bakedLeafBlend = leafBlend;      // (the recording froze them in)
      inst.bakedLiveShader =
          inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
      inst.bakedScalars = std::move(scalarsNow);
      inst.paintDirty = false;
      stats.picturesRecorded++;
    }
    if (profileScope.row != SIZE_MAX)
      profileRows[profileScope.row].cacheState = Composer::CacheState::Picture;
    // The measurement that drives promotion. Two clock reads per candidate
    // node per frame, against a full rasterisation — the overhead is not
    // close to material.
    const auto replayStart = std::chrono::steady_clock::now();
    profDraw("replay", [&] { canvas.drawPicture(inst.picture); });
    accrue(std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - replayStart)
               .count());
  } else {
    stats.nodesPainted++;
    // A LEAF never records a picture — one draw call beats a nested
    // recording — so without this it would never be timed at all, and the
    // most expensive single object a scene can hold, a full-canvas box
    // carrying one shader, would be structurally invisible to the promoter.
    // So the live draw is timed too, but ONLY for a node that could
    // actually be promoted: that keeps two clock reads per frame off every
    // ineligible node in the tree, of which there are usually thousands.
    if (!promotable) {
      profDraw("live", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
      });
    } else {
      const auto liveStart = std::chrono::steady_clock::now();
      profDraw("live", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
      });
      accrue(std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - liveStart)
                 .count());
    }
    inst.paintDirty = false;
  }

  if (needsLayer) canvas.restore();
  canvas.restore();
}

}  // namespace sigil::compose
