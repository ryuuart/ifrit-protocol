/** @file
 * paintContent: what one node emits, in stacking order — backgrounds, the
 * clip, the fill, the echoes and overlays, the leaf content, the children,
 * the foregrounds — with the masking family applied over it, the silhouette
 * the marks are dressed along, and the one glyph-paint override
 * textFill()/textStroke() ask for.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypes.h>  // SkASSERT
#include <include/effects/SkRuntimeEffect.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilimage/asset/ImageAsset.h>
#include <sigilshaders/ComposeCore.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — textFill's cap-height metrics

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ComposeRuntime.h"
#include "PaintInternal.h"

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
SkPath gateOutline(const SpanArithmeticOperations* arith, const SkPath& src,
                   const std::vector<Span>& show, bool* cut = nullptr) {
  if (claimsEverything(show)) return src;
  if (cut) *cut = true;
  if (show.empty()) return SkPath();
  return arith ? arith->spanPath(src, show) : src;
}

/** The shape a gated boundary was cut FROM — `PaintContext::silhouette` —
 *  or an empty path when the gate cut nothing and the boundary IS that
 *  shape. A span gate narrows the boundary to the run shown so far, and a
 *  partial run is OPEN: it bounds no area. An Inner- or Outer-aligned
 *  stroke clips to what the shape encloses, so on the run alone it clips
 *  to nothing while the run is one straight segment and to whatever the
 *  run's implicit closure encloses once it turns a corner. Carrying the
 *  uncut shape keeps an alignment meaning one thing at every fraction of a
 *  reveal: the half of the stroke's width inside — or outside — the shape,
 *  along the part of the boundary that is shown. */
SkPath gateSilhouette(const SkPath& src, const std::vector<Span>& show) {
  return claimsEverything(show) ? SkPath() : src;
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
    auto result = SkRuntimeEffect::MakeForShader(
        SkString(shaderSource("LumaCoverage.sksl")));
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
  if (!(inst.outlineCacheShape == inst.description->shapeFn) ||
      inst.outlineCacheSize != size) {
    inst.outlineCache = inst.description->shapeFn(size);
    inst.outlineCacheShape = inst.description->shapeFn;
    inst.outlineCacheSize = size;
  }
  return inst.outlineCache;
}

// ---------------------------------------------------------------------------
// textFill()/textStroke(): the one glyph-paint override

std::optional<sigil::weave::PaintStyle> Composer::Impl::metricTextStyle(
    Instance& inst, const PaintContext& paintCtx) {
  const ElementNode& node = *inst.description;
  const material::skia::Paint* metricMat = metricFillOf(node);
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
    const Fill sf = resolveRef(node.textData->textStrokeFill, paintCtx);
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
  // the metric band, so uResolution baked from the node's layout size
  // would divide a second time and a unit-space ramp would collapse onto
  // its first stop, flat and silently. A ramp authored in [0,1]² crosses
  // the type because the band is what it is mapped onto.
  PaintContext metricCtx = paintCtx;
  metricCtx.size = {1.0f, 1.0f};
  const Fill f = (metricMat->isAnimated() || metricMat->geometryDependent())
                     ? resolveFill(*metricMat, metricCtx)
                     : toFill(*metricMat);
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
    // The first run that carries glyphs is the face the cap band is read
    // from — the runs in draw order, and no walk of every glyph in the
    // passage to reach the first one.
    const sigil::weave::ShapedWord* firstFont = nullptr;
    for (const sigil::weave::PositionedRun& run : inst.textLayout.runs)
      if (run.shaped) {
        firstFont = run.shaped;
        break;
      }
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
                                  float leafOpacity, Phase phase,
                                  bool deferLayerEffect) {
  const ElementNode& node = *inst.description;
  // The two halves of a node's paint, split at the children loop. A
  // split bake is only ever offered to a node with no layer effect — that
  // one WRAPS BOTH HALVES and a bake of the prefix alone would have to
  // reproduce it. A clip and a whole-node mask wrap both halves too, but
  // both are opened and closed inside EACH phase, so the phases stay a pair
  // of skips over an otherwise untouched function; the granular mask scopes
  // below are each opened and closed inside the half they belong to.
  // A node HOSTING A SHARED SPACE was left by paint() at the plane the
  // space is drawn on: its own paint — both halves — concatenates its own
  // plane here, and its children place themselves against the canvas as
  // it stands. A host whose own plane is not drawn (facing away with its
  // backface hidden, or edge-on) emits neither half of its own paint and
  // still paints the children.
  const std::optional<SkMatrix> ownPlane = curOwnPlane;
  const bool ownVisible = !curOwnHidden;
  // ONE SAVE, AND THE CLIP'S SAVE NESTS INSIDE IT. A node that hosts a
  // shared space is the only one with a plane of its own to enter, and a
  // node that clips its content hosts no space — so the two saves are
  // never interleaved and each is restored in its own order.
  const auto enterOwn = [&] {
    if (ownPlane) {
      SkASSERT(!node.clipContent);
      canvas.save();
      canvas.concat(*ownPlane);
    }
  };
  const auto leaveOwn = [&] {
    if (ownPlane) canvas.restore();
  };
  const bool emitOwn = phase != Phase::ChildrenOnly && ownVisible;
  const bool emitChildren = phase != Phase::OwnOnly;
  // A COVERAGE TRACE OF THIS NODE draws everything the node draws EXCEPT
  // its own marks: the marks are what dress the traced boundary, so a mark
  // inside the trace would be dressing itself. The node's children — and
  // their marks — are drawn, because they are part of what the node drew
  // and none of them reads this node's boundary.
  const bool emitMarks = coverageTrace != &inst && ownVisible;
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
  const SpanArithmeticOperations* arith = nullptr;
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
  // WHAT THE DECORATIONS DRESS. Every decoration is drawn across an
  // outline, and a text leaf whose boundary is its GLYPHS hands them the
  // glyph contours the placement produced instead of its box — which is
  // the whole of what makes a chrome or a bevel land on letters. The
  // SURFACE keeps the node's own shape either way: a fill is what the box
  // is filled with, not what its letters are cut from.
  //
  // A COVERAGE boundary is neither: it is the silhouette of what the node
  // actually drew, traced off its layer's alpha, which is the only answer
  // that knows about an image's cut-out or a masked subtree. It is asked
  // for only where it is consumed — inside the trace itself the node has
  // no marks, so it needs no boundary for them either, and that is what
  // keeps the trace from re-entering itself.
  const SkPath* decorationBase = &fullOutline;
  if (node.boundary == Boundary::Glyphs && node.kind == Kind::Text &&
      inst.paragraph) {
    if (inst.glyphOutlineRev != inst.measuredRev) {
      inst.glyphOutline = inst.textLayout.glyphOutline();
      inst.glyphOutlineRev = inst.measuredRev;
    }
    if (!inst.glyphOutline.isEmpty()) decorationBase = &inst.glyphOutline;
  } else if (node.boundary == Boundary::Coverage && emitMarks) {
    // TRACED AT THE HOST'S SCALE, whatever scale this paint is running at:
    // a bake paints its content at a reduced raster scale of its own, and
    // a silhouette traced there is a second answer that neither side of
    // the frame can reuse — the derive pass asks at the host's scale, and
    // one cached trace serves both.
    const SkPath& traced =
        coverageOutline(inst, {bounds.width(), bounds.height()}, hostScale);
    if (!traced.isEmpty()) decorationBase = &traced;
    // A staircase of whole device pixels is a different path at a
    // different scale, so a recording that freezes one in is pinned to the
    // scale it was traced at, exactly as one holding a device blit is
    // pinned to its matrix. One step per device pixel is as fine as a
    // grid gets, so the window this narrows to is a point.
    narrowScaleWindow(hostScale, hostScale);
  }
  SkPath marksPath = !marksShow ? *decorationBase
                     : (surfaceShow && *marksShow == *surfaceShow &&
                        decorationBase == &fullOutline)
                         ? surfacePath
                         : gateOutline(arith, *decorationBase, *marksShow);
  // …and the shape that boundary was cut from, for an aligned stroke to
  // clip against while only part of it is shown.
  SkPath marksSilhouette =
      marksShow ? gateSilhouette(*decorationBase, *marksShow) : SkPath();
  const bool trimmed = cut;

  // The MARKS' boundary is what a decoration receives: every decoration
  // dresses the outline, and a spans gate over the marks is a cut of that
  // outline. The surface keeps its own (they are the same path, and the
  // same object, whenever one mask gates both — the common case).
  //
  // Built BEFORE the effect's saveLayer because an effect's child Material
  // resolves against it — the node's box, the node's clock, exactly what
  // Material::child hands a fill's children.
  // The pointer in the node's own box: the canvas point the host fed,
  // taken back through the node's transform. A projection with no
  // inverse leaves it at the origin, as a node nobody can point at.
  PaintContext::Pointer pointerHere;
  pointerHere.pressed = pointerPressed;
  if (SkMatrix toNode; curToRoot.invert(&toNode))
    pointerHere.at = toNode.mapPoint(pointerAt);
  const PaintContext paintCtx{
      .size = {bounds.width(), bounds.height()},
      .outline = std::move(marksPath),
      .silhouette = std::move(marksSilhouette),
      .elapsedSeconds = elapsed(),
      .contentScale = contentScale,
      .animating = ticker.active(),
      .fonts = &fonts,
      .borrowed = inst.borrowedPaths.empty() ? nullptr : &inst.borrowedPaths,
      .stamps = &inst.stampCache,
      .toRoot = curToRoot,         // node→root, as paint() stacked it
      .rootSize = rootLayoutSize,  // …and the canvas it maps into
      // The cascade as it resolved at this node: the ink every mark that
      // names no colour takes, the font a pen program begins in, and the
      // custom properties a fill or an ink may read.
      .ink = inst.font.color.value_or(SkColor4f{0, 0, 0, 1}),
      .font = inst.font,
      .vars = inst.vars.get(),
      .pointer = pointerHere,
      .keys = &keys,
      .bakeDensity = bakeDensity};

  // The node's own layer effect wraps everything painted here, so it is
  // captured by picture recordings and BAKED by texture snapshots. A LIVE
  // effect (bound uniforms, a live child material) resolves here per paint,
  // and computeVolatile has declared such a node volatile, so this
  // recording is never cached stale.
  //
  // DEFERRED, the effect is left out of what is emitted and applied to the
  // bake at its blit instead: an effect whose parameters move over content
  // that does not is the one case where the two are not the same picture in
  // cost, since a filter over an image whose identity holds finds any held
  // pass of it already made.
  const material::skia::Effect* layerFx =
      deferLayerEffect ? nullptr : layerEffectOf(node);
  const material::skia::PaintFrame layerFrame = frameOf(paintCtx);
  const sk_sp<SkImageFilter> layerFilter =
      layerFx ? layerFx->resolvedImageFilter(&layerFrame) : nullptr;
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
    const float rad = geometry::path::radians(angleDeg);
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
          .size = paintCtx.size,
          .outline = arith ? arith->spanPath(fullOutline, run) : fullOutline,
          .silhouette = gateSilhouette(fullOutline, run),
          .elapsedSeconds = paintCtx.elapsedSeconds,
          .contentScale = paintCtx.contentScale,
          .animating = paintCtx.animating,
          .fonts = paintCtx.fonts,
          .borrowed = paintCtx.borrowed,
          .stamps = nullptr,  // deliberately not shared with a span pass
          .toRoot = paintCtx.toRoot,
          .rootSize = paintCtx.rootSize,
          .ink = paintCtx.ink,
          .font = paintCtx.font,
          .vars = paintCtx.vars,
          .pointer = paintCtx.pointer,
          .keys = paintCtx.keys,
          .bakeDensity = paintCtx.bakeDensity};
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
      const PaintContext markCtx{
          .size = paintCtx.size,
          .outline = gateOutline(arith, fullOutline, run),
          .silhouette = gateSilhouette(fullOutline, run),
          .elapsedSeconds = paintCtx.elapsedSeconds,
          .contentScale = paintCtx.contentScale,
          .animating = paintCtx.animating,
          .fonts = paintCtx.fonts,
          .borrowed = paintCtx.borrowed,
          .stamps = nullptr,  // not shared with a mark
          .toRoot = paintCtx.toRoot,
          .rootSize = paintCtx.rootSize,
          .ink = paintCtx.ink,
          .font = paintCtx.font,
          .vars = paintCtx.vars,
          .pointer = paintCtx.pointer,
          .keys = paintCtx.keys,
          .bakeDensity = paintCtx.bakeDensity};
      d.paint(canvas, markCtx);
    } else {
      d.paint(canvas, paintCtx);
    }
    if (granularPlane) leaveGates(saves, cover);
  };

  // The own half stands on the node's own plane (a host's is entered here;
  // every other node's is the canvas as paint() left it).
  enterOwn();

  // Background decorations paint beneath the fill (the CSS box-shadow
  // ordering): shadow and pattern layers first, then the surface.
  // Decorations are NEVER clipped — they dress the outline, so shadows keep
  // their reach and an outer stroke survives on a node that clips its
  // content.
  if (emitOwn && emitMarks) {
    for (size_t i = 0; i < node.backgrounds.size(); ++i)
      paintMark(node.backgrounds[i], detail::MarkSlot::Background, i);
    // Span-qualified BACKGROUND passes land here, in the background half,
    // under the fill and therefore under the content and the children —
    // a z-slot a stroke pass cannot reach: under the fill, and therefore
    // under the content and the children.
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
  } else if (const material::skia::Paint* live = liveMaterialOf(node)) {
    resolvedFill = inst.hasPendingLiveFill ? inst.pendingLiveFill
                                           : resolveFill(*live, paintCtx);
  } else if (node.paint.fill) {
    Fill fill;
    if (const choreograph::Output<Fill>* binding = node.paint.fill->binding())
      fill = binding->value();
    else if (inst.anims[Instance::kFillLerp] &&
             inst.anims[Instance::kFillLerp]->started &&
             inst.anims[Instance::kFillLerp]->value.isConnected()) {
      const float t = inst.anims[Instance::kFillLerp]->value.value();
      // Either endpoint may be written as the ink in force or a custom
      // property, so both are resolved here before the colours are mixed.
      const Fill from = resolveRef(inst.fillFrom, paintCtx);
      const Fill to = resolveRef(inst.fillTo, paintCtx);
      fill = to;
      for (int i = 0; i < 4; ++i)
        fill.colorValue.vec()[i] =
            from.colorValue.vec()[i] +
            (to.colorValue.vec()[i] - from.colorValue.vec()[i]) * t;
      fill.kind = Fill::Kind::Color;
    } else {
      ResolvedProperty<Fill> resolved =
          resolveProperty(*node.paint.fill, node.nodeTransition);
      fill = resolved.target;
    }
    resolvedFill = resolveRef(fill, paintCtx);
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
  if (emitOwn && emitMarks && node.fxData)
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
          // A leaf that spends the room left over down its box needs the
          // RESOLVED DEPTH for the same reason an aligned leaf needs the
          // resolved width: a box of a stated height never reaches the
          // measure callback at all, so this is the only place the depth
          // `distribute` spends is known.
          const bool distributesRoom =
              node.textData && node.textData->distributesRoom();
          if (inst.measuredRev != inst.contentRev ||
              (!onPathRun && node.textData &&
               (verticalRun || distributesRoom ||
                node.textData->alignment() !=
                    sigil::weave::TextAlignment::kStart) &&
               (inst.measuredForWidth != bounds.width() ||
                ((verticalRun || distributesRoom) &&
                 inst.measuredForHeight != bounds.height()))))
            // A FRAME is bounded by its own depth either way round: its
            // remainder is what the next frame of its chain begins at, and
            // a re-layout here at an unbounded depth would place the whole
            // story and leave the next frame nothing.
            layoutText(
                inst, bounds.width(),
                verticalRun || distributesRoom ||
                        (node.textData && !node.textData->threadTo.empty())
                    ? bounds.height()
                    : 1.0e6f);
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
          const TextPainterOperations* painter = textPainterOf(inst);
          if ((hasTextFx(inst) || onPath) && painter) {
            painter->paint(inst, canvas, glyphPaint, onPath,
                           {bounds.width(), bounds.height()}, paintCtx);
          } else {
            inst.textLayout.drawBatched(&canvas, *inst.paragraph, glyphPaint);
          }
          // The readings beside the type. They stand AT REST while the
          // letters move, as a mark and a band do: a reading that travelled
          // with a cascade would be reading a letter that had left.
          for (const Instance::PlacedAnnotation& reading : inst.textAnnotations)
            if (reading.paragraph)
              reading.layout.drawBatched(&canvas, *reading.paragraph);
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
  // picture — ancestor re-records don't repaint clean subtrees). The
  // children of a host stand in the space it opened: the canvas is left at
  // the plane the space is drawn on, and they are painted back to front by
  // the depth of their centres rather than in stacking order.
  leaveOwn();
  const size_t kidsCover = coverStack.size();
  const int kidsSaves = granularPlane && emitChildren
                            ? enterGates(false, Parts::kChildren, {})
                            : -1;
  if (emitChildren) {
    if (const Space* hosted = curSpace) {
      std::vector<size_t> order;
      depthOrder(inst, hosted->accum, order);
      for (size_t index : order) paint(*inst.children[index], canvas);
    } else {
      for (size_t index : inst.paintOrder) paint(*inst.children[index], canvas);
    }
  }
  if (granularPlane && emitChildren) leaveGates(kidsSaves, kidsCover);
  enterOwn();

  if (node.clipContent) canvas.restore();  // decorations below stay unclipped

  // FOREGROUNDS PAINT AFTER THE CHILDREN, so they belong to the children
  // half and can never be in an own-paint bake. The own half is the
  // contiguous PREFIX up to the children loop, which is not the same thing
  // as "everything except the children".
  if (emitChildren && emitMarks)
    for (size_t i = 0; i < node.foregrounds.size(); ++i)
      paintMark(node.foregrounds[i], detail::MarkSlot::Foreground, i);

  // Span-qualified stroke passes, in declaration order, in the same slot
  // as the unqualified strokes they append to. Each one paints against
  // the sub-geometry it CLAIMED, so a brush that knows nothing about
  // spans (a PathFormat, a Brush, a brush::Pattern) dresses part of a
  // boundary with no new vocabulary.
  if (emitChildren && emitMarks)
    paintSpanHalf(detail::StrokePass::Half::Foreground);
  leaveOwn();

  leaveGates(hoistSaves, hoistCover);

  if (hasEffect) canvas.restore();
}

}  // namespace sigil::compose
