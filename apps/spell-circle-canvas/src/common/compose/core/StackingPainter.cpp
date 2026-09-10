/** @file
 * paint: the per-node walk — the node's placement, the plane it stands on,
 * the effects and memos its cache decision rests on, the layer its blend
 * and opacity take — and the dispatch through the cache ladder beside it.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkM44.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <optional>
#include <utility>

#include "ComposeRuntime.h"
#include "PaintInternal.h"
#include "PaintPass.h"

namespace sigil::compose {

using namespace detail;

void Composer::Impl::paint(Instance& inst, SkCanvas& canvas) {
  const ElementNode& node = *inst.description;
  const SkRect rect = instanceRect(inst);
  ProfileScope profileScope(this, inst, rect);

  const float opacity = std::clamp(
      inst.resolveFloat(Instance::kOpacity, node.paint.opacity), 0.0f, 1.0f);
  if (opacity <= 0.0f) return;

  // (Size-change invalidation for recordings — including geometry-dependent
  // materials' baked uResolution — happens in ensureLayout's
  // syncLayoutRects pass, which sees every relayout; paint() may never reach
  // a node whose ancestor replays a cached picture.)

  // ONE transform producer for the resolver's lanes: concatTo() is
  // matrix()'s op list applied as elementary canvas ops — byte-exactness
  // demands that sequence, see its comment — while recordBounds()'s child
  // union and hitInstance()'s inverse map and invert the composed matrix()
  // itself. A PLANE THAT HAS TURNED — a depth lane off rest, or a node
  // standing in a shared space — is placed by the 4x4 instead, flattened:
  // the 3x3 with a perspective row that Skia draws, one concat.
  const NodeTransform tf = transformOf(inst);
  const Space* space = curSpace;  // the space the parent hosts, if any
  const bool spaceHost = hostsSpace(inst);
  std::optional<SkM44> depth;  // the node's 4x4 in the plane it is drawn on
  if (space || spaceHost || tf.spatial()) {
    SkM44 m = depthMatrixOf(inst, tf, rect);
    if (space) m = SkM44(space->accum, m);
    depth = m;
  }
  // …and whether that plane is drawn at all. A plane with a singular
  // flattening is edge-on or collapsed and has no pixels; one whose back
  // faces the viewer with its backface hidden draws none either. A node
  // hosting a space still paints the children the space holds — they
  // stand on their own planes — so only a node with no space to host
  // leaves here.
  std::optional<SkMatrix> flat;
  bool ownHidden = false;
  if (depth) {
    flat = depth->asM33();
    ownHidden =
        !flat->invert(nullptr) ||
        (node.depthData && node.depthData->backface == Backface::Hidden &&
         facesAway(*depth));
    if (ownHidden && !spaceHost) return;
  }

  // The device matrix the node's own transform is applied ON TOP OF — read
  // before that transform is concatenated. The coarse bake ladder needs it
  // to ask what scale a declared scale motion is heading for,
  // which is a question about this node's own lane and not about the
  // matrix it currently reads as.
  const SkMatrix parentCanvasM = canvas.getTotalMatrix();
  canvas.save();
  if (spaceHost) {
    // The canvas STAYS at the plane the space is drawn on, so the children
    // can place themselves against it; the host's own paint concatenates
    // its plane inside paintContent (curOwnPlane below).
  } else if (flat) {
    canvas.concat(*flat);
  } else {
    canvas.translate(rect.left(), rect.top());
    tf.concatTo(canvas, node.paint, rect.width(), rect.height());
  }

  // Accumulate the node→root matrix alongside the canvas ops — the same
  // T(rect)·matrix() product hitInstance() inverts, so a world-space
  // material draws its field exactly where the hit test says the node is.
  // NOT canvas.getTotalMatrix(): that includes the HOST's transform and any
  // bake-layer offset, and this matrix must stop at the composer root. RAII
  // because paint() returns from several places. The op sequence per case
  // is worldMatrixOf's, exactly: the settle compare reads an ulp of drift
  // between the two as motion.
  if (!inst.parent) rootLayoutSize = SkSize{rect.width(), rect.height()};
  struct ToRootScope {
    SkMatrix* slot;
    SkMatrix saved;
    explicit ToRootScope(SkMatrix* s) : slot(s), saved(*s) {}
    ~ToRootScope() { *slot = saved; }
  } toRootScope(&curToRoot);
  const SkMatrix plane = curToRoot;  // the parent's plane, before this node
  if (space) {
    curToRoot = space->rootToPlane;
    curToRoot.preConcat(*flat);
  } else if (flat) {
    curToRoot.preConcat(*flat);
  } else {
    curToRoot.preTranslate(rect.left(), rect.top());
    curToRoot.preConcat(
        tf.matrix({0, 0}, node.paint, rect.width(), rect.height()));
  }

  // The space this node hosts for its children, and its own plane for
  // paintContent; a node hosting none closes the one it stands in, since
  // its children are flat in its plane. RAII for the same reason as above.
  struct DepthScope {
    Composer::Impl* impl;
    const Space* savedSpace;
    std::optional<SkMatrix> savedOwnPlane;
    bool savedOwnHidden;
    DepthScope(Composer::Impl* i, const Space* s, std::optional<SkMatrix> own,
               bool hidden)
        : impl(i),
          savedSpace(i->curSpace),
          savedOwnPlane(i->curOwnPlane),
          savedOwnHidden(i->curOwnHidden) {
      impl->curSpace = s;
      impl->curOwnPlane = std::move(own);
      impl->curOwnHidden = hidden;
    }
    ~DepthScope() {
      impl->curSpace = savedSpace;
      impl->curOwnPlane = savedOwnPlane;
      impl->curOwnHidden = savedOwnHidden;
    }
  };
  Space hosted;
  if (spaceHost) hosted = Space{*depth, space ? space->rootToPlane : plane};
  DepthScope depthScope(this, spaceHost ? &hosted : nullptr,
                        spaceHost ? flat : std::nullopt,
                        spaceHost && ownHidden);

  // What the cache tiers below decide on. Filled in the order this walk
  // computes it — the placement it already holds, then the effects, the
  // memos, the composite and the device geometry.
  PaintPass pass{*this, inst, canvas, rect, profileScope};
  pass.opacity = opacity;
  pass.tf = &tf;
  pass.placedByPlane = (bool)flat;
  pass.spaceHost = spaceHost;
  pass.parentCanvasM = parentCanvasM;

  const material::skia::Effect* backdropFx = backdropEffectOf(node);
  sk_sp<SkImageFilter> backdropFilter;
  if (backdropFx) {
    // A backdrop effect's child materials resolve against the node's box
    // too — the same context the node's own paint builds, minus the marks
    // outline, because nothing of the node has been painted yet. Built
    // INSIDE the branch: every node reaches this line and only a few carry
    // a backdrop.
    const PaintContext backdropCtx{.size = {rect.width(), rect.height()},
                                   .elapsedSeconds = elapsed(),
                                   .contentScale = hostScale,
                                   .animating = ticker.active(),
                                   .fonts = &fonts,
                                   .stamps = &inst.stampCache,
                                   .toRoot = curToRoot,  // this node→root
                                   .rootSize = rootLayoutSize};
    const material::skia::PaintFrame backdropFrame = frameOf(backdropCtx);
    backdropFilter = backdropFx->resolvedImageFilter(&backdropFrame);
  }
  // THE DEFERRED LAYER EFFECT. A node whose only volatility is its own
  // layer effect's bound parameters paints static content under a moving
  // filter, so the content is baked once with the effect left out and the
  // effect is run over that one image at every blit. What that buys is not
  // the content's rasterisation, which is usually the cheap half: it is the
  // image's IDENTITY. An effect built over held passes — a pyramid whose
  // levels are the same filter nodes every frame — finds each pass already
  // made for an image it has filtered before, where a freshly rasterized
  // layer is a new image every frame and every pass runs again.
  //
  // Resolved here rather than inside paintContent because it is needed on
  // the frames that bake NOTHING, which are all of them but the first. Its
  // child materials therefore resolve against the node's box and clock,
  // exactly as a backdrop effect's do; the tier is refused to a masked node
  // so that this is the same outline paintContent would have handed them.
  if (inst.effectOnly) pass.deferredFilter = pass.resolveLayerFilter();
  // The live tier's own answer, kept apart from the static one: it is what
  // lets a node hold a bake its content volatility would otherwise drop,
  // and the static tier claims nothing of the sort.
  const bool deferLiveEffect = (bool)pass.deferredFilter;
  pass.deferLiveEffect = deferLiveEffect;
  pass.deferEffect = deferLiveEffect;

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
    PaintContext probe{
        .size = {rect.width(), rect.height()},
        .elapsedSeconds = elapsed(),
        .contentScale = hostScale,
        .animating = ticker.active(),
        .fonts = &fonts,
        .toRoot = curToRoot,  // so the memo digest sees this move
        .rootSize = rootLayoutSize};
    inst.pendingLiveFill = resolveFill(*liveMaterialOf(node), probe);
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
  Instance::ContentScalars& scalarsNow = pass.scalarsNow;
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
    // value the recording bakes is this frame's binding read, so the
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
  if (inst.scalarMemo && inst.settle.observe(scalarsStable, scalarsNow,
                                             Instance::kScalarSettleFrames))
    volatileDirty = true;
  // "May this node keep its cached pixels?" — either nothing about it is
  // volatile, or every input it reads is memoized and provably unchanged.
  const bool memoized = inst.liveMatOnly || inst.scalarMemo;
  // …or the volatility is entirely OUTSIDE the pixels the cache holds,
  // which is the deferred effect's whole claim.
  const bool cacheHolds = !inst.subtreeVolatile || memoized || deferLiveEffect;
  // …and "are they still the RIGHT pixels?" — the two memos answer for
  // their own input and abstain on the other.
  const bool memoStale =
      (inst.liveMatOnly && !liveStable) || (inst.scalarMemo && !scalarsStable);
  pass.cacheHolds = cacheHolds;
  pass.memoStale = memoStale;
  // A STATIC LAYER EFFECT OVER SETTLED CONTENT is lifted off the content
  // raster, for a different payoff than the moving one's. The moving tier
  // buys the image's identity across frames; this one buys the BAKE: a
  // filter left inside the content raster opens a layer of its own over
  // the node's whole band — allocated, cleared, drawn into and composited
  // back on every bake — and that layer is most of what an effect over a
  // large node costs, which is why a small sigma pays nearly what a large
  // one does. Lifted out, the content rasterizes into the bake surface
  // alone and the effect is one image draw over it.
  //
  // The candidacy only; the tier is TAKEN at the local bake, which is the
  // one that holds the two surfaces it needs. An ASKED-FOR bake
  // only: an automatic promotion refuses a filtered node outright, and
  // where the author has not asked for a texture at all this would turn a
  // picture replay into a bake, which is a different decision than this
  // one. A DEVICE-space bake is left exactly as it stands — it is taken
  // once and re-taken only when the node moves, so there is no repeated
  // bake there to make cheaper.
  const bool staticEffectCandidate =
      !deferLiveEffect && node.cacheMode == Cache::Texture &&
      layerEffectOf(node) && !layerEffectOf(node)->isAnimated() &&
      (!inst.subtreeVolatile || memoized) && !backdropEffectOf(node) &&
      !inst.subtreeReadsBackdrop && !node.hasMasks() &&
      node.boundary == Boundary::Auto;
  pass.staticEffectCandidate = staticEffectCandidate;

  // Fill-only leaves route blend/opacity straight onto the fill paint instead
  // of a (device-clip-sized!) saveLayer — a field of plus-blended shapes costs
  // path draws, not full-canvas layers. Excluded: texture bakes (blending
  // must hit the real destination, not the bake's transparent surface).
  //
  // A LIVE OPACITY IS ALLOWED, ON A LEAF THAT CANNOT RECORD. The hazard it
  // has to be kept away from is a recording: a recording that baked the
  // fill paint would freeze this frame's alpha into it and replay a fade
  // that has moved on. Every OTHER recording is already impossible — a
  // bound opacity declares the node's own paint volatile, which blocks
  // every containing recording and every group bake — so what is left is
  // the node's own, and the only two ways a leaf this predicate admits
  // takes one are asking for `Cache::Picture` and holding a memo. Both are
  // named. The composite itself is exact for a single fill: a lone path
  // drawn into a transparent layer and composited at alpha is the same
  // pixels as that path drawn at alpha, since there is nothing inside the
  // layer for it to composite against first.
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
      (!opacityLive || (node.cacheMode != Cache::Picture && !memoized)) &&
      node.cacheMode != Cache::Texture &&
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
  pass.deferBlendToBlit = deferBlendToBlit;
  pass.needsLayer = needsLayer;
  pass.leafBlend = leafBlend;
  pass.leafOpacity = leafOpacity;

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target
  // pixel win — replaying a picture re-rasterizes, blitting doesn't). The
  // tiers are read in the order they are asked below; each answers whether
  // the node's paint is over.

  // THE DEVICE MATRIX — what this node's draws reach the device through.
  // Outside a recording it is the canvas's own. Inside one the canvas's
  // matrix is in the recording's space, and the recording is replayed
  // under a matrix of its own; the composition is the device grid the
  // node is drawn on, which is the grid a device-space bake must sample
  // and the rect its blit must land on. Every bake tier reads this and
  // never the canvas matrix alone.
  const SkMatrix& canvasM = canvas.getTotalMatrix();
  const SkMatrix totalM = recordingDepth == 0
                              ? canvasM
                              : SkMatrix::Concat(recordingReplay, canvasM);
  // "Is the node where it was last frame?" — its own history at the root,
  // where it is painted every frame, and the outermost open recording's
  // inside one, where it is painted only when that recording is taken.
  // The first sighting counts as stable, as the device rect's does.
  bool matrixStable = recordingMatrixStable;
  if (recordingDepth == 0) {
    matrixStable = !inst.deviceMatrixSeen || totalM == inst.lastDeviceMatrix;
    inst.lastDeviceMatrix = totalM;
    inst.deviceMatrixSeen = true;
  }
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
  pass.totalM = totalM;
  pass.matrixStable = matrixStable;
  pass.upright = upright;

  decidePromotion(pass);
  if (paintPromotedBake(pass)) return;
  if (paintSplitBake(pass)) return;
  if (paintGroupBake(pass)) return;

  if (!liveOnly && cacheHolds &&
      (node.cacheMode == Cache::Texture || pass.deferEffect) &&
      !backdropEffectOf(node)) {
    if (paintTextureBake(pass)) return;
  } else if (!liveOnly && cacheHolds && node.cacheMode != Cache::None &&
             // A node HOSTING A SHARED SPACE never records: its children
             // are drawn on the plane beneath it, so a recording here
             // would bake their projections and the view above — matrices
             // that live on other nodes, whose patches would never reach
             // it. It paints live, its children keep their own recordings,
             // and every recording above it is dirtied by those nodes'
             // own patches. The cost is one traversal shim and the host's
             // own fill.
             !spaceHost &&
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
    paintThroughPicture(pass);
  } else {
    paintLive(pass);
  }

  pass.close();
}

}  // namespace sigil::compose
