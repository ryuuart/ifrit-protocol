/** @file
 * paintContent: what one node emits, in stacking order — backgrounds, the
 * clip, the fill, the echoes and overlays, the leaf content, the children,
 * the foregrounds — with the masking family applied over it, the silhouette
 * the marks are dressed along.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPathBuilder.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypes.h>  // SkASSERT
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Color.h>
#include <sigilweave/choreograph/Choreograph.h>
#include <sigilweave/fonts/FontContext.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Masking.h"
#include "PaintInternal.h"
#include "PenOnCanvas.h"
#include "SurfaceMap.h"
#include "layout/DeriveInternal.h"
#include "runtime/ComposeRuntime.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// Silhouette

const SkPath& Composer::Impl::resolveOutline(Instance& inst,
                                             SkSize size) const {
  if (!(inst.outlineCacheShape == inst.description->shapeFn) ||
      inst.outlineCacheSize != size) {
    inst.outlineCache = skiaOutline(inst.description->shapeFn, size);
    inst.outlineCacheShape = inst.description->shapeFn;
    inst.outlineCacheSize = size;
  }
  return inst.outlineCache;
}

// ---------------------------------------------------------------------------
// The stacking painter

void Composer::Impl::paintContent(Instance& inst, SkCanvas& canvas,
                                  float contentScale, SkBlendMode leafBlend,
                                  float leafOpacity, Phase phase,
                                  bool deferLayerEffect) {
  const ElementNode& node = inst.painted();
  const ComputedStyle& style = inst.computed;
  // The node's own paint is the prefix up to the children loop, which a
  // coverage trace of the node alone emits by itself. A clip and a
  // whole-node mask wrap the whole node, but both are opened and closed
  // inside either phase, so the phase stays a skip over an otherwise
  // untouched function; the granular mask scopes below are each opened and
  // closed inside the part they belong to.
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
      SkASSERT(!style.clipContent);
      canvas.save();
      canvas.concat(*ownPlane);
    }
  };
  const auto leaveOwn = [&] {
    if (ownPlane) canvas.restore();
  };
  const bool emitOwn = ownVisible;
  const bool emitChildren = phase != Phase::OwnOnly;
  // A COVERAGE TRACE OF THIS NODE draws everything the node draws EXCEPT
  // its own marks: the marks are what dress the traced boundary, so a mark
  // inside the trace would be dressing itself. The node's children — and
  // their marks — are drawn, because they are part of what the node drew
  // and none of them reads this node's boundary.
  const bool emitMarks = coverageTrace != &inst && ownVisible;
  const SkRect ownRect = instanceRect(inst);
  const SkRect bounds = SkRect::MakeWH(ownRect.width(), ownRect.height());
  const SkRRect rrect = cornersRRect(inst, bounds);

  // The node's shape: a band's swept region, a custom outline(), or the
  // corner-rounded box.
  const geometry::path::Profile* bandWidth = node.bandWidth();
  const bool customShape = node.shapeFn || bandWidth;
  SkPath outlinePath;
  if (bandWidth) {
    // A BAND's shape is derived: the region its spine sweeps at the
    // profile's width, on the declared side. The spine is guide data.
    const SkPath spine =
        node.deriveData->bandSpine
            ? skiaOutline(node.deriveData->bandSpine,
                          SkSize{bounds.width(), bounds.height()})
            : SkPath();
    const StrokeResolver& resolver = node.deriveData->bandResolver;
    outlinePath = resolver
                      ? resolver->bandRegion(spine, *bandWidth,
                                             node.deriveData->bandFormation)
                      : SkPath();
  } else if (customShape) {
    outlinePath = resolveOutline(inst, {bounds.width(), bounds.height()});
  } else {
    SkPathBuilder outlineBuilder;
    outlineBuilder.addRRect(rrect);
    outlinePath = outlineBuilder.detach();
  }

  // (overflow(Overflow::Clip) applies AFTER the decorations' outline is settled
  // — see below: decorations dress the outline and stay unclipped;
  // fill/content/children clip. The clip keeps the UNMASKED shape — a mask is a
  // paint reveal.)
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
  const BoundaryGates boundary =
      resolveBoundaryGates(inst, masks, gateValues, outlinePath);
  const SpanArithmeticOperations* const arith = boundary.arithmetic;
  const std::optional<std::vector<Span>>& surfaceShow = boundary.surfaceShow;
  const std::optional<std::vector<Span>>& marksShow = boundary.marksShow;
  const std::vector<std::pair<std::string, std::vector<Span>>>& namedShow =
      boundary.namedShow;
  const auto intersect = [&boundary](const std::vector<Span>& a,
                                     const std::vector<Span>& b) {
    return boundary.intersect(a, b);
  };
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
  // Text placement is needed by glyph decorations as well as content.
  // Yoga skips the measure callback when both dimensions are fully
  // determined, and a leaf whose lines place against its resolved box is
  // laid out at it here: the same rule a borrow of the placed glyphs reads.
  if (node.kind == Kind::Text && inst.paragraph)
    prepareTextForBox(*this, inst, bounds.width(), bounds.height());

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
  // The held parts the marks' outline was composed from, where it was.
  const std::vector<CoveragePart>* composedParts = nullptr;
  SkPath deviatedGlyphs;
  if (node.boundary == Boundary::Glyphs && node.kind == Kind::Text &&
      inst.paragraph) {
    if (inst.glyphOutlineRev != inst.measuredRev) {
      inst.glyphOutline = geometry::path::toSk(inst.textLayout.glyphOutline());
      inst.glyphOutlineRev = inst.measuredRev;
    }
    decorationBase = &inst.glyphOutline;
    // Glyphs a track moves this frame are dressed where they are drawn.
    if (const TextPainterOperations* painter = textPainterOf(inst);
        painter && hasTextFx(inst) &&
        painter->deviatedGlyphOutline(
            inst, node.textData->onPath ? &*node.textData->onPath : nullptr,
            {bounds.width(), bounds.height()}, deviatedGlyphs))
      decorationBase = &deviatedGlyphs;
  } else if (node.boundary == Boundary::Coverage && emitMarks) {
    // TRACED AT THE HOST'S SCALE, whatever scale this paint is running at:
    // a bake paints its content at a reduced raster scale of its own, and
    // a silhouette traced there is a second answer that neither side of
    // the frame can reuse — the derive pass asks at the host's scale, and
    // one cached trace serves both.
    const SkPath& traced =
        coverageOutline(inst, {bounds.width(), bounds.height()}, hostScale);
    if (!traced.isEmpty()) {
      decorationBase = &traced;
      if (!marksShow && !inst.coverageParts.empty())
        composedParts = &inst.coverageParts;
    }
    // A staircase of whole device pixels is a different path at a
    // different scale, so a recording that freezes one in is pinned to the
    // scale it was traced at, exactly as one holding a device blit is
    // pinned to its matrix. One step per device pixel is as fine as a
    // grid gets, so the window this narrows to is a point.
    recording.narrowScaleWindow(hostScale, hostScale);
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
    pointerHere.at = geometry::path::fromSk(toNode.mapPoint(pointerAt));
  const PaintContext paintCtx{
      .size = {bounds.width(), bounds.height()},
      .outline = geometry::path::fromSk(std::move(marksPath)),
      .silhouette = geometry::path::fromSk(std::move(marksSilhouette)),
      .elapsedSeconds = elapsed(),
      .contentScale = contentScale,
      .contentScaleRead = &contentScaleRead,
      .destination = outputInfo,
      .recorder = paintDestination(canvas).recorder(),
      .surfaceMap = surfaceMap,
      .animating = engine.isRunning(),
      .fonts = &fonts,
      .borrowed = inst.borrowedPaths.empty() ? nullptr : &inst.borrowedPaths,
      .stamps = &inst.stampCache,
      .toRoot = geometry::path::fromSk(
          curToRoot),  // node→root, as paint() stacked it
      .rootSize = geometry::path::fromSk(
          rootLayoutSize),  // …and the canvas it maps into
      // The cascade as it resolved at this node: the ink every mark that
      // names no colour takes, the font a pen program begins in, and the
      // custom properties a fill or an ink may read.
      .ink = inst.font.color.value_or(material::Color{0, 0, 0, 1}),
      .inkPaint = inst.inkPaint.paint ? &inst.inkPaint : nullptr,
      .inkAnchorSize = geometry::path::fromSk(inkAnchorSize),
      // This node's own space mapped INTO the anchor box: the walk holds
      // the anchor's node→root matrix, and a slice is what stands between
      // the two.
      .inkAnchorToRoot = geometry::path::fromSk(anchorSpace()),
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
  const material::Filter* layerFx =
      deferLayerEffect ? nullptr : layerEffectOf(node);
  // The layer's frame names what the layer holds, which may reach past
  // the box: a filter that crops to its reach crops to that.
  material::FrameData layerFrame = frameOf(paintCtx);
  // The extent the filter reads stands on the node's filter grid, so a
  // mark whose bleed grows within a cell moves none of its picture.
  const SkRect filtered = layerFx ? onFilterGrid(recordBounds(inst)) : SkRect();
  if (layerFx) {
    layerFrame.paintedMin = {filtered.left(), filtered.top()};
    layerFrame.paintedMax = {filtered.right(), filtered.bottom()};
  }
  const sk_sp<SkImageFilter> layerFilter =
      layerFx ? material::skia::resolvedImageFilter(*layerFx, &layerFrame)
              : nullptr;
  const bool hasEffect = (bool)layerFilter;
  if (hasEffect) {
    SkPaint effectPaint;
    effectPaint.setImageFilter(layerFilter);
    // BOUNDED: with nullptr bounds the layer allocates at the CLIP size, so
    // a small icon's drop shadow on a root-level canvas would filter the
    // whole canvas. recordBounds is what the subtree actually paints, on
    // the filter grid; Skia expands it for the filter's own reach.
    canvas.saveLayer(&filtered, &effectPaint);
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
  PlaneGates gates(*this, inst, canvas, masks, gateValues, bounds, fullOutline,
                   paintCtx);
  const bool granularPlane = gates.granular();

  // The whole-node hoist.
  const size_t hoistCover = gates.coverDepth();
  const int hoistSaves = gates.enter(true, Parts::kAll, {});

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
      const size_t cover = gates.coverDepth();
      const int saves = granularPlane
                            ? gates.enter(false, Parts::kMarks, passes[i].name)
                            : -1;
      PaintContext passCtx = paintCtx;
      passCtx.outline = geometry::path::fromSk(
          arith ? arith->spanPath(fullOutline, run) : fullOutline);
      passCtx.silhouette =
          geometry::path::fromSk(gateSilhouette(fullOutline, run));
      passCtx.stamps = nullptr;  // each span keeps its own art cache
      detail::PenOnCanvas passPen(canvas, passCtx);
      passes[i].what.paint(passPen.pen(), passCtx);
      if (granularPlane) gates.leave(saves, cover);
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
    const size_t cover = gates.coverDepth();
    const int saves =
        granularPlane ? gates.enter(false, Parts::kMarks, label) : -1;
    detail::MarkState& state = inst.markStates.at((uint8_t)slot, index);
    if (refine) {
      std::vector<Span> run = *refine;
      if (marksShow) run = intersect(run, *marksShow);
      PaintContext markCtx = paintCtx;
      markCtx.outline =
          geometry::path::fromSk(gateOutline(arith, fullOutline, run));
      markCtx.silhouette =
          geometry::path::fromSk(gateSilhouette(fullOutline, run));
      markCtx.stamps = nullptr;  // each mark keeps its own art cache
      state.coverageParts = nullptr;
      markCtx.markState = &state;
      detail::PenOnCanvas markPen(canvas, markCtx);
      d.paint(markPen.pen(), markCtx);
    } else {
      PaintContext markCtx = paintCtx;
      state.coverageParts = composedParts;
      markCtx.markState = &state;
      detail::PenOnCanvas markPen(canvas, markCtx);
      d.paint(markPen.pen(), markCtx);
      state.coverageParts = nullptr;
    }
    if (granularPlane) gates.leave(saves, cover);
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

  // overflow(Overflow::Clip) bounds the fill, the content, and the children —
  // not the decorations (above and below), which trace the outline itself.
  if (style.clipContent) {
    canvas.save();
    if (customShape)
      canvas.clipPath(clipShape, true);
    else
      canvas.clipRRect(rrect, true);
  }

  // Fill (background): a live material resolves per frame from its bound
  // uniforms + the PaintContext; otherwise the stored Fill (binding, lerp, or
  // plain).
  std::optional<Fill> resolvedFill;
  if (!emitOwn) {
    // Nothing of the node's own shows: skip straight past the fill, the
    // echoes, the overlays and the leaf content to the children loop. (The
    // outline and the clip/wipe/effect wrappers above are recomputed rather
    // than skipped — they are cheap, they must stay balanced against their
    // restores below, and the foregrounds still trace the outline.)
  } else {
    resolvedFill = resolveFillInForce(
        *this, inst, paintCtx, bounds,
        customShape || trimmed ? &surfacePath : nullptr, rrect);
  }

  // The SURFACE — the fill and its echo re-stamps — under whatever gates
  // select `parts::surface()`.
  const size_t surfaceCover = gates.coverDepth();
  const int surfaceSaves =
      granularPlane && emitOwn ? gates.enter(false, Parts::kSurface, {}) : -1;

  // Misprint echoes of the FILL SHAPE, under the real pass (bottom first).
  if (!echoesOf(node).empty() && resolvedFill &&
      resolvedFill->kind != Fill::Kind::None) {
    for (const Echo& e : echoesOf(node)) {
      SkPaint stamp;
      stamp.setAntiAlias(true);
      stamp.setColor4f(material::skia::toSkColor(e.color), nullptr);
      canvas.save();
      canvas.translate(e.offset.x, e.offset.y);
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
      paint.setColor4f(material::skia::toSkColor(fill.colorValue), nullptr);
    else
      paint.setShader(material::skia::staticShader(detail::paintOf(fill)));
    // A lit fill on a page painted as a surface map already is the map.
    if (surfaceMap)
      if (const MaterialData* slot = fillSlotOf(inst); slot && slot->surfaced)
        markSurfaceMap(paint);
    // Leaf fast path: paint() proved a layer is unnecessary and routed the
    // node's blend/opacity straight onto the fill.
    paint.setBlendMode(leafBlend);
    if (leafOpacity < 1.0f) paint.setAlphaf(paint.getAlphaf() * leafOpacity);
    if (customShape || trimmed)
      canvas.drawPath(surfacePath, paint);
    else
      canvas.drawRRect(rrect, paint);
  }
  if (granularPlane && emitOwn) gates.leave(surfaceSaves, surfaceCover);

  // Overlays: over the fill, under the content and children. The slot a
  // textured button needs so its own hazard stripe does not grey out its
  // label. Unclipped like the other decorations — they dress the outline.
  if (emitOwn && emitMarks && node.fxData)
    for (size_t i = 0; i < node.fxData->overlays.size(); ++i)
      paintMark(node.fxData->overlays[i], detail::MarkSlot::Overlay, i);

  // Content, under whatever gates select parts::content().
  const size_t contentCover = gates.coverDepth();
  const int contentSaves =
      granularPlane && emitOwn ? gates.enter(false, Parts::kContent, {}) : -1;
  if (emitOwn) paintLeafContent(*this, inst, canvas, paintCtx, bounds);

  if (granularPlane && emitOwn) gates.leave(contentSaves, contentCover);

  // Children in stacking order (each clean static child replays its own nested
  // picture — ancestor re-records don't repaint clean subtrees). The
  // children of a host stand in the space it opened: the canvas is left at
  // the plane the space is drawn on, and they are painted back to front by
  // the depth of their centres rather than in stacking order.
  leaveOwn();
  const size_t kidsCover = gates.coverDepth();
  const int kidsSaves = granularPlane && emitChildren
                            ? gates.enter(false, Parts::kChildren, {})
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
  if (granularPlane && emitChildren) gates.leave(kidsSaves, kidsCover);
  enterOwn();

  if (style.clipContent) canvas.restore();  // decorations below stay unclipped

  // FOREGROUNDS PAINT AFTER THE CHILDREN, so they are not part of the
  // node's own paint. That is the contiguous PREFIX up to the children
  // loop, which is not the same thing as "everything except the children".
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

  gates.leave(hoistSaves, hoistCover);

  if (hasEffect) canvas.restore();
}

}  // namespace sigil::compose
