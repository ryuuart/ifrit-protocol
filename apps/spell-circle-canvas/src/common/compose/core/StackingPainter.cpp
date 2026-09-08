/** @file
 * paint: the per-node walk and the cache ladder it dispatches through —
 * live paint, an automatic picture over provably-static subtrees, a split
 * bake, a memo-held bake and a texture bake — with the picture tier behind
 * the kernel's bake seam.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPicture.h>
#include <include/core/SkPictureRecorder.h>
#include <include/core/SkSurface.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilmeasure/time/Stopwatch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>   // std::snprintf, on a keyless node's profile label
#include <cstdlib>  // std::getenv, std::strtod — the profile threshold
#include <optional>
#include <string>
#include <utility>

#include "ComposeRuntime.h"
#include "PaintInternal.h"

namespace sigil::compose {

using namespace detail;

// ---------------------------------------------------------------------------
// The picture tier, behind the bake seam

void PictureBake::take(PictureBakeTarget& t) const {
  t.painter->recordPicture(*t.inst, t.deviceMatrix, t.deviceClip,
                           t.matrixStable, t.hostScale, t.leafBlend,
                           t.leafOpacity, std::move(*t.scalars));
}
void PictureBake::replay(PictureBakeTarget& t) const {
  t.canvas->drawPicture(t.inst->picture);
  // A held picture replayed into an enclosing recording hands that
  // recording the device blits it holds: the outer picture is now pinned
  // to the same matrix, and remade with this one when it changes.
  t.painter->recordingDeviceBakes += t.inst->pictureDeviceBakes;
  t.painter->recordingDeviceDeferred |= t.inst->pictureDeviceDeferred;
}
void PictureBake::drop(PictureBakeTarget& t) const { t.inst->picture.reset(); }
bool PictureBake::held(const PictureBakeTarget& t) const {
  return (bool)t.inst->picture;
}

void Composer::Impl::recordPicture(Instance& inst, const SkMatrix& deviceMatrix,
                                   const SkIRect& deviceClip, bool matrixStable,
                                   float hostScale, SkBlendMode leafBlend,
                                   float leafOpacity,
                                   Instance::ContentScalars&& scalars) {
  // The same rect the layers and bakes use. Its job HERE is only to be an
  // honest bounds advertisement (SkPicture::cullRect) — this path attaches
  // no BBH, so nothing is culled against it either at record or at
  // playback; see the note on ownPaintBounds for the measurement.
  const SkRect cull = recordBounds(inst);
  SkPictureRecorder recorder;
  SkCanvas* rec = recorder.beginRecording(cull);
  // WHAT A RECORDING MAY HOLD. A picture is a list of draw calls and
  // replays under whatever matrix it meets, so everything inside one must
  // be matrix-independent — with one precise exception. A device-space
  // bake is a blit at an absolute device rect, exact at any angle and
  // wrong under any other matrix. It may be recorded when every recording
  // it stands inside is PINNED: made under a matrix that is stamped on the
  // instance, replayed only under that matrix, and remade the frame it
  // differs. A recording is pinnable when nothing above it moves by
  // declaration — no live transform on this node or an ancestor, no
  // shared space whose view is live — because a pinned recording under
  // motion would be remade every frame, which is the one cost the picture
  // tier exists to avoid. Under a declared motion the recording stays
  // UNPINNED and its contents keep the matrix-independent rule; the
  // coverage trace, an offscreen raster at a scale of its own, is unpinned
  // for the same reason.
  //
  // A pinned recording with no device blit inside is still
  // matrix-independent and is never remade for the matrix alone. So the
  // count is kept, not a flag: it is the number of device blits the
  // recording holds, its own nodes' and those of every held picture
  // replayed into it.
  const bool unpinned = inst.placementUnderMotion;
  // The outermost recording's node is painted every frame, so its verdict
  // on the matrix is a history; a nested one inherits it.
  if (recordingDepth == 0) recordingMatrixStable = matrixStable;
  const SkMatrix outerReplay = recordingReplay;
  const SkMatrix outerReplayInverse = recordingReplayInverse;
  // The ops recorded here reach the device through this node's matrix —
  // already composed out through every enclosing recording.
  recordingReplay = deviceMatrix;
  const bool invertible = recordingReplay.invert(&recordingReplayInverse);
  const uint32_t outerBakes = recordingDeviceBakes;
  const bool outerDeferred = recordingDeviceDeferred;
  recordingDeviceBakes = 0;
  recordingDeviceDeferred = false;
  ++recordingDepth;
  // A replay matrix with no inverse has no device rect to land a blit on.
  if (unpinned || !invertible) ++unpinnedRecordingDepth;
  paintContent(inst, *rec, hostScale, leafBlend, leafOpacity);
  if (unpinned || !invertible) --unpinnedRecordingDepth;
  --recordingDepth;
  inst.picture = recorder.finishRecordingAsPicture();
  inst.pictureMatrix = deviceMatrix;
  inst.pictureDeviceClip = deviceClip;
  inst.pictureDeviceBakes = recordingDeviceBakes;
  inst.pictureDeviceDeferred = recordingDeviceDeferred;
  // What this recording holds, the enclosing one now holds too.
  recordingDeviceBakes = outerBakes + inst.pictureDeviceBakes;
  recordingDeviceDeferred = outerDeferred || inst.pictureDeviceDeferred;
  recordingReplay = outerReplay;
  recordingReplayInverse = outerReplayInverse;
  inst.bakedLeafOpacity = leafOpacity;  // a settled transition re-bakes
  inst.bakedLeafBlend = leafBlend;      // (the recording froze them in)
  inst.bakedLiveShader =
      inst.hasPendingLiveFill ? inst.pendingLiveFill.shaderValue : nullptr;
  inst.bakedScalars = std::move(scalars);
  inst.paintDirty = false;
  stats.picturesRecorded++;
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
  const detail::ElementNode& node = *inst.description;
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

/** A BAKE LAYER IS A DEVICE OF ITS OWN. What is painted into it lands on
 *  the layer's grid, and the layer is then blitted wherever the node is —
 *  so a node inside the bake composes its matrix through nothing, whatever
 *  recording the blit itself is being recorded into. RAII because the
 *  bake sites return from the middle of paint(). */
struct BakeLayerScope {
  Composer::Impl* impl;
  SkMatrix replay, inverse;
  uint32_t bakes;
  bool deferred;
  explicit BakeLayerScope(Composer::Impl* i)
      : impl(i),
        replay(i->recordingReplay),
        inverse(i->recordingReplayInverse),
        bakes(i->recordingDeviceBakes),
        deferred(i->recordingDeviceDeferred) {
    impl->recordingReplay = SkMatrix::I();
    impl->recordingReplayInverse = SkMatrix::I();
    // The blits taken inside the layer are the LAYER's, not the enclosing
    // recording's: the layer is blitted as one image wherever the node is,
    // so what it holds pins nothing above it.
    impl->recordingDeviceBakes = 0;
    impl->recordingDeviceDeferred = false;
  }
  ~BakeLayerScope() {
    impl->recordingReplay = replay;
    impl->recordingReplayInverse = inverse;
    impl->recordingDeviceBakes = bakes;
    impl->recordingDeviceDeferred = deferred;
  }
};

/** Scoped per-node timer. RAII because paint() has several early returns
 *  and a half-written row would be worse than no row at all. */
struct ProfileScope {
  Composer::Impl* impl = nullptr;
  size_t row = SIZE_MAX;
  double savedChildren = 0;
  // Absent until profiling is on: a clock read per node per frame is not
  // a cost an unprofiled paint should pay.
  std::optional<measure::Stopwatch> watch;

  ProfileScope(Composer::Impl* i, const detail::Instance& inst,
               const SkRect& rect)
      : impl(i) {
    // A COVERAGE TRACE IS NOT A FRAME: it paints a node again into an
    // offscreen raster to read what it drew, so its nodes are not nodes
    // the viewer saw and their cost is not the frame's.
    if (!impl->profileEnabled || impl->coverageTrace) return;
    row = impl->profileRows.size();
    impl->profileRows.push_back(Composer::NodeCost{profileLabel(inst, rect), 0,
                                                   0, impl->profDepth,
                                                   Composer::CacheState::Live});
    savedChildren = impl->profChildMs;
    impl->profChildMs = 0;
    ++impl->profDepth;
    watch.emplace();
  }
  ~ProfileScope() {
    if (row == SIZE_MAX) return;
    const double total = watch->elapsedMs();
    impl->profileRows[row].totalMs = total;
    impl->profileRows[row].selfMs = total - impl->profChildMs;
    // Hand our whole cost up to the parent's child accumulator.
    impl->profChildMs = savedChildren + total;
    --impl->profDepth;
  }
};

}  // namespace

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
  // before that transform is concatenated. The coarse bake ladder below
  // needs it to ask what scale a declared scale motion is heading for,
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

  const material::skia::Effect* backdropFx = backdropEffectOf(node);
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
  const auto resolveLayerFilter = [&] {
    const PaintContext effectCtx{{rect.width(), rect.height()},
                                 SkPath(),
                                 elapsed(),
                                 hostScale,
                                 ticker.active(),
                                 &fonts,
                                 nullptr,
                                 &inst.stampCache,
                                 curToRoot,
                                 rootLayoutSize};
    const material::skia::PaintFrame effectFrame = frameOf(effectCtx);
    return layerEffectOf(node)->resolvedImageFilter(&effectFrame);
  };
  sk_sp<SkImageFilter> deferredFilter;
  if (inst.effectOnly) deferredFilter = resolveLayerFilter();
  // The live tier's own answer, kept apart from the static one below: it is
  // what lets a node hold a bake its content volatility would otherwise
  // drop, and the static tier claims nothing of the sort.
  const bool deferLiveEffect = (bool)deferredFilter;
  bool deferEffect = deferLiveEffect;

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
  // The candidacy only; the tier is TAKEN at the local bake below, which
  // is the one that holds the two surfaces it needs. An ASKED-FOR bake
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

  // Automatic caching at topmost provably-static subtrees: pictures by
  // default, a rasterized image under Cache::Texture (the raster-target pixel
  // win — replaying a picture re-rasterizes, blitting doesn't).
  // COMPOSE_PROF=<ms> prints any draw above the threshold — cached-texture
  // blits, picture replays (which re-EXECUTE recorded ops on raster), live
  // paints, and the bakes themselves, which are the cost a blit is bought
  // with. Nested lines overlap (inclusive of children); any unparsable
  // value means 4ms.
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
    const measure::Stopwatch watch;
    draw();
    const double ms = watch.elapsedMs();
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
  // THE DEVICE MATRIX — what this node's draws reach the device through.
  // Outside a recording it is the canvas's own. Inside one the canvas's
  // matrix is in the recording's space, and the recording is replayed
  // under a matrix of its own; the composition is the device grid the
  // node is drawn on, which is the grid a device-space bake must sample
  // and the rect its blit must land on. Every bake tier below reads this
  // and never the canvas matrix alone.
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
  // A device blit: the matrix reset, so the image lands at an absolute
  // device rect — and inside a recording, the replay's inverse concatenated,
  // so the replay carries it back to exactly that rect. Counted, because
  // the recording holding it is pinned to its matrix from here on.
  const auto deviceBlit = [&](const sk_sp<SkImage>& image, const SkIRect& at,
                              const SkPaint* paint) {
    canvas.save();
    canvas.resetMatrix();
    if (recordingDepth > 0) canvas.concat(recordingReplayInverse);
    canvas.drawImage(image, (float)at.left(), (float)at.top(),
                     SkSamplingOptions(), paint);
    canvas.restore();
    if (recordingDepth > 0) ++recordingDeviceBakes;
  };
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
  // THE DEVICE CLIP, in the space a device blit lands in.
  // `getDeviceClipBounds()` is in base device coordinates, which is the
  // space `resetMatrix()` draws in, including inside a saveLayer; inside a
  // recording the canvas's clip is in the recording's own space and is
  // carried out through the replay, the same way the matrix is.
  const auto deviceClipOf = [&] {
    const SkIRect clip = canvas.getDeviceClipBounds();
    if (recordingDepth == 0) return clip;
    return recordingReplay.mapRect(SkRect::Make(clip)).roundOut();
  };
  // The node's paint bounds as whole device pixels, with the margin every
  // bake is allocated with (bakeMargin — the content must stand clear of
  // the surface's own edge, or the scan converter answers a different
  // coverage than the live paint's).
  const auto deviceRectOf = [&] {
    const SkRect f = totalM.mapRect(localBoundsOf());
    SkIRect r = SkIRect::MakeLTRB(
        (int)std::floor(f.left()), (int)std::floor(f.top()),
        (int)std::ceil(f.right()), (int)std::ceil(f.bottom()));
    const int m = bakeMargin(std::max(r.width(), r.height()));
    r.outset(m, m);
    return r;
  };
  /** EVERY DEVICE BAKE CARRIES THE CANVAS'S OWN CLIP, and this is not an
   *  optimisation — it is the condition that makes a bake the same pixels as
   *  the paint it replaces. Skia rasterizes an antialiased edge against the
   *  clip it is given, so an edge that leaves the canvas is CUT in the live
   *  paint and whole in a bake that spans the node's full paint bounds, and
   *  the coverage the two compute for the pixels either side of it differs
   *  by TENS of code values — not the single least-significant bit an
   *  integer offset costs. Anything with bleed — a glow, a turned piece, a
   *  full-bleed plane, a tile that overruns its page — leaves its canvas on
   *  some side, which is most of what a bake is ever taken over.
   *
   *  Applied to the layer while its matrix is still identity, so the rect is
   *  in the layer's own pixels: the same device-aligned integer rect, in the
   *  same place, cutting the same coverage. The bake RECT stays the node's
   *  full paint bounds, because that rect is also the identity a held bake
   *  is compared against — a rect narrowed to the clip is shared by two
   *  different pictures whenever the clip is the smaller of the two.
   *
   *  WHICH IS WHY THE CLIP IS PART OF WHAT THE BAKE IS. What the layer
   *  holds is the node's paint as this clip left it, so the clip is
   *  stamped on the instance and every tier below re-bakes when it moves:
   *  a clip narrows and widens for reasons the node's own bounds cannot
   *  see — an ancestor's layer opened over its box while it fades in, a
   *  panel revealing, a window growing — and a bake held across that
   *  blits the cut for as long as the node's content stands still. */
  const auto clipBakeLayer = [&](SkCanvas* lc, const SkIRect& bake) {
    lc->clipIRect(deviceClipOf().makeOffset(-bake.left(), -bake.top()));
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
  const bool optedOut =
      autoPromoteEffective == Composer::PromotionPolicy::Off ||
      node.cacheMode != Cache::Auto;
  // EAGER SKIPS THE STOPWATCH AND NOTHING ELSE. Every refusal below is a
  // condition under which a bake would paint different pixels, and this
  // policy changes none of them — it answers only "is this node expensive
  // enough to be worth baking" with yes, so a run that means to exercise
  // the promoter exercises every node the rules admit rather than the few
  // the machine happened to be slow on.
  const bool eager = autoPromoteEffective == Composer::PromotionPolicy::Eager;
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
  if (spaceHost) flag(Prom::HostsSpace);

  // The PRIMARY verdict: the first refusal in the order an author should
  // address them (their own switches first, then content, then geometry).
  static constexpr Prom kRefusalOrder[] = {
      Prom::OptedOut,      Prom::HostsSpace,  Prom::Volatile,
      Prom::Composited,    Prom::Transformed, Prom::Filtered,
      Prom::ReadsBackdrop, Prom::TooBig};
  Prom why = Prom::Cheap;
  for (Prom p : kRefusalOrder)
    if (refusals & (uint16_t)(1u << (unsigned)p)) {
      why = p;
      break;
    }

  // THE DEVICE-BAKE RULE, which the Cache::Group, Cache::Texture and split
  // tiers below share: a device-space bake blits with the matrix reset at
  // an ABSOLUTE device rect, so it is exact under one matrix and wrong
  // under every other. It may be taken at the root, or inside recordings
  // that are all PINNED — made under a matrix stamped on the instance and
  // remade the frame it differs — and never inside an unpinned one, which
  // replays under a declared motion and would be remade every frame.
  // Inside a pinned recording the node is painted only when the recording
  // is, so the matrix must also be holding still by the recording's own
  // history: a bake taken under a moving matrix would pin a recording that
  // is then remade, and the bake with it, on every frame of the motion.
  const bool deviceBakeable =
      unpinnedRecordingDepth == 0 && (recordingDepth == 0 || matrixStable);
  const bool promotable = why == Prom::Cheap && !liveOnly && deviceBakeable;
  if (!promotable)
    inst.autoTexture = false;
  else if (eager)
    inst.autoTexture = true;  // no warmup: the bake is taken below, this frame
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
    // Nothing warms under a trace: the node is painted twice in the frame
    // it is traced, and a promotion clock that counted both would run at
    // double rate for a reason the viewer never sees.
    if (coverageTrace) return;
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
      // resized (which is how a transform-SCALE change arrives here), when
      // the clip the bake was cut to moved, or — the temporal case — when
      // the live material has actually ticked and the baked shader is no
      // longer the one this frame resolves to.
      if (!inst.textureImage || inst.paintDirty || memoStale ||
          inst.textureEffectDeferred ||
          inst.textureBakeRect != SkRect::Make(device) ||
          inst.textureBakeClip != deviceClipOf()) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          clipBakeLayer(lc, device);
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          profDraw("promote bake", [&] {
            const BakeLayerScope bakeLayer(this);
            paintContent(inst, *lc, hostScale, leafBlend, leafOpacity);
          });
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureInk = {};
          inst.textureDeviceSpace = true;
          inst.textureEffectDeferred = false;
          inst.textureBakeRect = SkRect::Make(device);
          inst.textureBakeClip = deviceClipOf();
          inst.bakedLiveShader = inst.hasPendingLiveFill
                                     ? inst.pendingLiveFill.shaderValue
                                     : nullptr;
          inst.bakedScalars = scalarsNow;
          inst.paintDirty = false;
          // AND THE RECORDING GOES. A bake is taken from the description as
          // it stands, and the node stops recording from the frame it is
          // taken — so a recording made before it holds the content of some
          // earlier frame with nothing left to say so: the scalars that
          // separated them are this bake's now, and a settled node is not
          // dirty. The bake is refused again the moment the matrix under it
          // moves (a plate photographed at another scale, a host resized),
          // and a recording kept here is what would replay then. Re-recording
          // costs one paint; replaying that one is a picture of another
          // frame.
          inst.picture.reset();
          stats.picturesRecorded++;
          if (!coverageTrace) stats.texturesBaked++;
        }
      }
      if (inst.textureImage) {
        promotedBytes += bytes;
        if (profileScope.row != SIZE_MAX)
          profileRows[profileScope.row].cacheState =
              Composer::CacheState::Promoted;
        note(Prom::Promoted);
        deviceBlit(inst.textureImage, device, nullptr);
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
      deviceBakeable && !inst.transformLive && !spaceHost &&
      // `upright` for the same reason promotion needs it, and it is the
      // SAME construction: an integer device offset concatenated onto the
      // node's matrix. Under rotation a shader's local coordinates come
      // back through an inverse that cannot cancel that offset exactly, and
      // the antialiased edges land about a least-significant bit apart.
      // Leaving it out would hold the split to a weaker standard than the
      // promoter beside it.
      upright;
  if (!splitCandidate)
    inst.splitBake = false;
  else if (eager)
    inst.splitBake = true;  // the own half, from its first frame
  if (splitCandidate) {
    // ownPaintBounds, NOT recordBounds. recordBounds unions the children
    // in, so it moves every frame a child moves — and a bake rect that
    // moves every frame is a bake remade every frame, which is the one
    // failure mode that would make this feature cost more than it saves on
    // precisely the scenes it exists for. The own paint's extent does not
    // depend on the children at all.
    const SkRect ownF = totalM.mapRect(ownPaintBounds(inst));
    const SkIRect device = [&] {
      SkIRect r = SkIRect::MakeLTRB(
          (int)std::floor(ownF.left()), (int)std::floor(ownF.top()),
          (int)std::ceil(ownF.right()), (int)std::ceil(ownF.bottom()));
      const int m = bakeMargin(std::max(r.width(), r.height()));
      r.outset(m, m);  // the same margin, same reason
      return r;
    }();
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
      if (!inst.ownImage || inst.ownPaintDirty || inst.ownBakeRect != want ||
          inst.ownBakeClip != deviceClipOf()) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          clipBakeLayer(lc, device);
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          {
            const BakeLayerScope bakeLayer(this);
            paintContent(inst, *lc, hostScale, leafBlend, leafOpacity,
                         Phase::OwnOnly);
          }
          inst.ownImage = layer->makeImageSnapshot();
          inst.ownBakeRect = want;
          inst.ownBakeClip = deviceClipOf();
          inst.ownPaintDirty = false;
          if (!coverageTrace) stats.texturesBaked++;
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
        profDraw("split blit",
                 [&] { deviceBlit(inst.ownImage, device, nullptr); });
        blitted = true;
      }
    }
    if (!blitted) {
      // The own half, live and TIMED. This is the number the split is
      // promoted on — the node's own paint, with its children excluded by
      // construction rather than by subtraction.
      const measure::Stopwatch ownWatch;
      profDraw("live own", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity,
                     Phase::OwnOnly);
      });
      const double ownMs = ownWatch.elapsedMs();
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
    if (!coverageTrace) stats.nodesPainted++;
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
  if (!liveOnly && inst.groupRootOK && deviceBakeable) {
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
    // THE RECT ITSELF IS NARROWED TO THE CANVAS HERE, on top of the clip
    // every bake layer carries: a lattice of rotated pieces with any bleed
    // overruns its own canvas on all four sides, and this tier exists for
    // exactly that content, so the pixels outside are worth not allocating.
    SkIRect device = deviceRectOf();
    if (!device.intersect(deviceClipOf())) device = SkIRect::MakeEmpty();
    // The rect history is this node's own only where it is painted every
    // frame; inside a recording the recording's matrix verdict stands in.
    bool rectStable = matrixStable;
    if (recordingDepth == 0) {
      rectStable = !inst.deviceRectSeen || device == inst.lastDeviceRect;
      inst.lastDeviceRect = device;
      inst.deviceRectSeen = true;
    }

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
          inst.textureEffectDeferred || inst.textureBakeRect != want ||
          inst.textureBakeClip != deviceClipOf()) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(device.width(), device.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          clipBakeLayer(lc, device);
          lc->translate(-(float)device.left(), -(float)device.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          // No leaf blend and no leaf opacity: bakes isolate, and the node's
          // own blend/opacity are applied by the saveLayer wrapping the blit
          // — which is why leafDirectBlend excludes Cache::Group.
          {
            const BakeLayerScope bakeLayer(this);
            paintContent(inst, *lc, hostScale);
          }
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureInk = {};
          inst.textureDeviceSpace = true;
          inst.textureEffectDeferred = false;
          inst.textureBakeRect = want;
          inst.textureBakeClip = deviceClipOf();
          inst.textureScale = maxScaleOf(totalM, localBoundsOf());
          inst.paintDirty = false;
          // A group root never replays a recording. It can have made one on
          // its very first frame — before it had a previous frame to compare
          // with, a group with a fully static subtree falls through to the
          // picture branch once — and holding it after that is bytes nobody
          // will ever read.
          inst.picture.reset();
          stats.picturesRecorded++;
          if (!coverageTrace) stats.texturesBaked++;
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
        profDraw("group blit",
                 [&] { deviceBlit(inst.textureImage, device, nullptr); });
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

  if (!liveOnly && cacheHolds &&
      (node.cacheMode == Cache::Texture || deferEffect) &&
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
    // texel grid IS the device grid, at any angle. Three conditions gate
    // it, each about not throwing away what the local bake is FOR:
    //
    //  - bakeScale must be 1. Its whole purpose is to rasterize BELOW
    //    device resolution and let the blit stretch it back.
    //  - the device-bake rule above: at the root, or inside recordings
    //    all pinned to the matrix they were made under, never inside one
    //    that replays under a declared motion. Inside a pinned recording
    //    the device grid is the recording's space composed out through
    //    its replay (`totalM` here), the blit concatenates the replay's
    //    inverse so the replay lands it back at the device rect, and the
    //    recording is remade the frame that matrix differs.
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
    //        device-pinned texture every frame. At the root the node is
    //        painted every frame and keeps that history itself; inside a
    //        recording it is painted only when the recording is, so the
    //        outermost recording's own matrix history answers instead.
    //    While either says "moving", the quantized local bake is correct
    //    and cheap: one bake per coarse scale step, reused across the rest.
    //    A node refused for the matrix alone inside a recording marks the
    //    recording DEFERRED, and it is retaken once the matrix holds still
    //    so the node takes the exact bake then rather than after its next
    //    content change.
    const SkRect localBounds = localBoundsOf();
    bool deviceRectStable = false;
    SkIRect deviceR = SkIRect::MakeEmpty();
    if (unpinnedRecordingDepth == 0) {
      deviceR = deviceRectOf();
      if (recordingDepth == 0) {
        deviceRectStable =
            !inst.deviceRectSeen || deviceR == inst.lastDeviceRect;
        inst.lastDeviceRect = deviceR;
        inst.deviceRectSeen = true;
      } else {
        deviceRectStable = matrixStable;
      }
    }
    const int64_t deviceArea = (int64_t)deviceR.width() * deviceR.height();
    // A DEFERRED EFFECT keeps the LOCAL bake. A device-space bake blits with
    // the matrix reset, and an image filter's parameters are read in the
    // space of the canvas that applies it: a sigma declared in the node's
    // own units would become a sigma in device units, so the effect would
    // change size with the host's scale. The local bake blits through the
    // node's own matrix, which is the matrix the effect's saveLayer stood
    // under, so the filter is applied in exactly the space it was declared
    // in.
    //  - and no declared bake density. A device-space bake IS a bake at
    //    the view's own scale, pinned to the view's own grid; a host that
    //    has said what density its rasters are taken at has said this
    //    path is not what it wants, and the local bake below is the one
    //    that honours it.
    const bool deviceEligible =
        !deferEffect && !inst.transformLive && unpinnedRecordingDepth == 0 &&
        node.bakeScale >= 1.0f && !totalM.hasPerspective() &&
        bakeDensity <= 0 && deviceR.width() > 0 && deviceR.height() > 0 &&
        deviceArea <= int64_t{16} * 1024 * 1024;
    if (deviceEligible && !deviceRectStable && recordingDepth > 0)
      recordingDeviceDeferred = true;
    if (deviceEligible && deviceRectStable) {
      const SkRect bakeRect = SkRect::Make(deviceR);
      if (!inst.textureImage || inst.paintDirty || !inst.textureDeviceSpace ||
          memoStale || inst.textureBakeRect != bakeRect ||
          inst.textureBakeClip != deviceClipOf()) {
        sk_sp<SkSurface> layer = canvas.makeSurface(
            SkImageInfo::MakeN32Premul(deviceR.width(), deviceR.height()));
        if (!layer)
          layer = SkSurfaces::Raster(
              SkImageInfo::MakeN32Premul(deviceR.width(), deviceR.height()));
        if (layer) {
          SkCanvas* lc = layer->getCanvas();
          clipBakeLayer(lc, deviceR);
          lc->translate(-(float)deviceR.left(), -(float)deviceR.top());
          lc->concat(totalM);  // identical device geometry, offset by ints
          profDraw("bake", [&] {
            const BakeLayerScope bakeLayer(this);
            paintContent(inst, *lc, hostScale);  // no leaf blend: bakes isolate
          });
          inst.textureImage = layer->makeImageSnapshot();
          inst.textureInk = {};
          inst.textureDeviceSpace = true;
          inst.textureEffectDeferred = false;
          inst.textureBakeRect = bakeRect;
          inst.textureBakeClip = deviceClipOf();
          inst.textureScale = maxScaleOf(totalM, localBounds);
          inst.bakedLiveShader = inst.hasPendingLiveFill
                                     ? inst.pendingLiveFill.shaderValue
                                     : nullptr;
          inst.bakedScalars = scalarsNow;
          inst.paintDirty = false;
          stats.picturesRecorded++;
          if (!coverageTrace) stats.texturesBaked++;
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
        profDraw("blit", [&] {
          if (deferBlendToBlit) {
            // The node's blend and opacity on the ONE draw it composites
            // as — cheaper and slightly MORE exact than the layer it
            // replaces: no full-canvas intermediate, one less rounding.
            SkPaint blit;
            blit.setAlphaf(opacity);
            blit.setBlendMode(node.paint.blendMode);
            deviceBlit(inst.textureImage, deviceR, &blit);
          } else {
            deviceBlit(inst.textureImage, deviceR, nullptr);
          }
        });
        if (needsLayer) canvas.restore();
        canvas.restore();
        return;
      }
    }
    // WHAT RESOLUTION A BAKE IS TAKEN AT, and there are two answers.
    //
    // A DECLARED DENSITY is a picture of the canvas: the host has said how
    // many device pixels a layout unit is worth, the bake is taken at that
    // and at nothing else, and the blit carries it through whatever the
    // view does afterwards — sharp at the density it was taken for,
    // magnified past it, exactly as an image node's pixels are. Nothing
    // about the frame's matrix reaches the decision, so a reader zooming
    // walks no ladder and waits on no re-rasterization; only a change of
    // what the picture IS re-takes it.
    //
    // NO DECLARED DENSITY is a picture of the view: rasterize at the
    // canvas's current scale so a zoomed host stays crisp — quantized UP
    // to a coarse step, so a continuously changing scale reuses one bake
    // per step instead of re-rasterizing every frame. Between steps the
    // draw minifies slightly, which stays sharp. The DEVICE matrix
    // (composed out through any recording), so a bake taken inside a
    // replayed-at-scale recording is rasterized at the scale it will be
    // shown at.
    const SkMatrix& total = totalM;
    // A SCALE MOTION THAT NAMES ITS DESTINATION IS BAKED AT THE
    // DESTINATION, ONCE — a ladder question, so a declared density skips
    // it: that bake is not at a scale the motion can move. The ladder
    // quantizes so that a scale nobody
    // declared — a window resize, a pinch zoom — reuses one bake per step
    // instead of re-rasterizing per frame. An entrance is the opposite
    // case: it is not an unknown scale drifting, it is a known scale being
    // travelled, and quantizing it bakes the node again at every rung it
    // passes. A `from(a).to(b)` on a scale lane names b, so the bake is
    // taken there and the blit MINIFIES through the entrance, which is the
    // sharp direction. A scale driven by a binding names nothing and keeps
    // the ladder.
    //
    // The substitution is on the node's OWN lanes, rebuilt against the
    // matrix its parent supplied — not a factor applied to the current
    // reading, which is zero at the start of an entrance from nothing.
    // Only the flat placement is rebuilt this way: a plane that has turned
    // or stands in a shared space is placed by a 4x4 whose own producer
    // owns that composition.
    SkMatrix destTotal = total;
    if (bakeDensity <= 0 && !flat && !spaceHost) {
      NodeTransform destTf = tf;
      bool declared = false;
      const auto lane = [&](Instance::Slot slot,
                            const motion::Animatable<float>& v, float& out) {
        const AnimatedFloat* a = inst.anims[slot].get();
        if (v.binding() || !a || !a->started || !a->value.isConnected()) return;
        out = a->target;
        declared = true;
      };
      lane(Instance::kScale, node.paint.scale, destTf.scl);
      lane(Instance::kScaleX, node.paint.scaleX, destTf.sx);
      lane(Instance::kScaleY, node.paint.scaleY, destTf.sy);
      if (declared) {
        destTotal = recordingDepth == 0
                        ? parentCanvasM
                        : SkMatrix::Concat(recordingReplay, parentCanvasM);
        destTotal.preTranslate(rect.left(), rect.top());
        destTotal.preConcat(
            destTf.matrix({0, 0}, node.paint, rect.width(), rect.height()));
      }
    }
    // maxScaleOf, NOT the matrix diagonal: a quarter-turned node's diagonal
    // is (0, 0) and would clamp to the 0.25 floor, baking at a quarter
    // resolution to be upscaled by the blit (see maxScaleOf in
    // ComposeRuntime.h). The node's local bounds locate the Jacobian
    // samples when the CTM carries a host perspective. This ladder feeds
    // the re-bake test below, so an underestimate here means a stale,
    // blurry bake rather than a wasted one.
    static constexpr float kBakeSteps[] = {0.25f, 0.5f, 0.75f, 1.0f,
                                           1.5f,  2.0f, 3.0f,  4.0f};
    float scale = bakeDensity;
    if (bakeDensity <= 0) {
      const float raw =
          std::clamp(maxScaleOf(destTotal, localBounds), 0.25f, 4.0f);
      scale = kBakeSteps[std::size(kBakeSteps) - 1];
      for (float step : kBakeSteps)
        if (step >= raw) {
          scale = step;
          break;
        }
    }
    // bakeScale(): opt-in reduced raster scale — the bake evaluates fewer
    // pixels and the blit below linear-upscales through the same dst rect.
    scale = std::max(0.1f, scale * node.bakeScale);
    // THE STATIC EFFECT IS LIFTED OFF THE CONTENT HERE. The bake is taken
    // in two steps instead of one: the content rasterizes into a surface
    // with the effect left out, and the effect is then run over that image
    // into the surface the node holds. What it replaces is the layer the
    // filter opened INSIDE the content raster — allocated over the node's
    // whole band plus the filter's reach, cleared, drawn into and
    // composited back — and that layer is most of what an effect over a
    // large node costs, which is why a small sigma paid nearly what a
    // large one did.
    //
    // NOT ON THE BLIT, which is where a MOVING effect goes. A filter hung
    // on the blit's paint is evaluated per draw, and Skia answers it from
    // its own cache only while the mapping that draw stands under holds
    // still — so a node that TURNS, which is the whole population that
    // keeps a local bake rather than a device one, would pay the entire
    // filter on every frame in exchange for paying it once per bake.
    const bool deferStaticEffect = staticEffectCandidate;
    if (deferStaticEffect) {
      deferredFilter = resolveLayerFilter();
      deferEffect = (bool)deferredFilter;
    }
    // Bake the full PAINT bounds, not just the box — decoration bleed and
    // overflowing children truncate otherwise (same rule as the picture
    // cull).
    const SkRect bake = localBounds;
    const int pw = std::max(1, (int)std::ceil(bake.width() * scale));
    const int ph = std::max(1, (int)std::ceil(bake.height() * scale));
    // THE SAME CEILING EVERY BAKE TIER TAKES: a surface past it is a
    // hundreds-of-megabytes allocation to hold one node, and the node is
    // painted live instead. A surface the device refused is not a bake
    // either — the node paints live rather than drawing through nothing.
    const int64_t area = (int64_t)pw * ph;
    if (area <= int64_t{16} * 1024 * 1024 &&
        (!inst.textureImage || inst.paintDirty || inst.textureScale != scale ||
         inst.textureDeviceSpace || memoStale ||
         inst.textureEffectDeferred != deferLiveEffect ||
         inst.textureBakeRect != bake)) {
      sk_sp<SkSurface> layer =
          canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
      if (!layer)
        layer = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
      if (layer) {
        layer->getCanvas()->scale(scale, scale);
        layer->getCanvas()->translate(-bake.left(), -bake.top());
        profDraw("bake", [&] {  // no leaf blend: bakes isolate
          const BakeLayerScope bakeLayer(this);
          paintContent(inst, *layer->getCanvas(), scale, SkBlendMode::kSrcOver,
                       1.0f, Phase::All, deferEffect);
        });
        // THE STATIC EFFECT, RUN OVER THE CONTENT BAKE. One image draw
        // into a second surface of the same rect, under the same matrix
        // the effect's layer stood under, so the filter reads its
        // parameters in the units they were declared in and its output is
        // cut where the layer's output was cut — by the surface's own
        // edge. The image lands texel for texel: the dst rect is the
        // texels the content surface actually holds, in local units, so
        // nothing resamples on the way through.
        if (deferStaticEffect) {
          const sk_sp<SkImage> content = layer->makeImageSnapshot();
          sk_sp<SkSurface> filtered =
              canvas.makeSurface(SkImageInfo::MakeN32Premul(pw, ph));
          if (!filtered)
            filtered = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(pw, ph));
          if (content && filtered) {
            SkCanvas* fc = filtered->getCanvas();
            fc->scale(scale, scale);
            fc->translate(-bake.left(), -bake.top());
            SkPaint fp;
            fp.setImageFilter(deferredFilter);
            profDraw("bake effect", [&] {
              fc->drawImageRect(
                  content,
                  SkRect::MakeXYWH(bake.left(), bake.top(), (float)pw / scale,
                                   (float)ph / scale),
                  SkSamplingOptions(), &fp);
            });
            layer = std::move(filtered);
          }
        }
        // The ink grid, off the surface's own pixels — before the
        // snapshot, so nothing is copied for it. A GPU surface answers no
        // pixmap and the grid stays empty, which is a whole-rect blit.
        SkPixmap baked;
        inst.textureInk =
            layer->peekPixels(&baked) ? inkGridOf(baked) : InkGrid{};
        inst.textureImage = layer->makeImageSnapshot();
        inst.textureScale = scale;
        inst.textureDeviceSpace = false;
        inst.textureEffectDeferred = deferLiveEffect;
        inst.textureBakeRect = bake;
        inst.bakedLiveShader = inst.hasPendingLiveFill
                                   ? inst.pendingLiveFill.shaderValue
                                   : nullptr;
        inst.bakedScalars = std::move(scalarsNow);
        inst.paintDirty = false;
        stats.picturesRecorded++;
        if (!coverageTrace) stats.texturesBaked++;
      }
    }
    if (profileScope.row != SIZE_MAX) {
      profileRows[profileScope.row].cacheState =
          inst.textureImage ? Composer::CacheState::Texture
                            : Composer::CacheState::Live;
      profileRows[profileScope.row].promotion = Composer::Promotion::AskedFor;
      profileRows[profileScope.row].effectDeferred =
          deferEffect && inst.textureImage;
    }
    if (!inst.textureImage) {
      // Nothing to blit: the surface was refused, or the bake would be
      // past the ceiling. The node paints itself — and takes back the
      // blend and opacity the blit was to have carried, since there is no
      // blit to carry them.
      if (deferBlendToBlit) {
        SkPaint layerPaint;
        layerPaint.setAlphaf(opacity);
        layerPaint.setBlendMode(node.paint.blendMode);
        const SkRect content = recordBounds(inst);
        canvas.saveLayer(&content, &layerPaint);
      }
      profDraw("live", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
      });
      if (deferBlendToBlit) canvas.restore();
      if (needsLayer) canvas.restore();
      canvas.restore();
      return;
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
      SkPaint blit;
      bool dressed = false;
      if (deferBlendToBlit) {  // same rule as the device blit above
        blit.setAlphaf(opacity);
        blit.setBlendMode(node.paint.blendMode);
        dressed = true;
      }
      // The deferred layer effect, applied to the bake rather than to the
      // content: the canvas stands at the node's own matrix here, which is
      // the matrix the effect's saveLayer stood under, so the filter reads
      // its parameters in the units they were declared in. Skia grows the
      // draw for the filter's own reach, so nothing the effect spreads
      // outside the bake rect is lost.
      //
      // THE BLEED RULE, which both deferred tiers ask of the bake: the
      // filter reads the bake's own margin — the transparent band a node's
      // paint bounds carry around its ink — and it reads NOTHING else,
      // because outside the bake there are no pixels. So a node wearing a
      // deferred effect must carry the effect's reach as that margin, which
      // is what a declared bleed is for; a reach the paint bounds do not
      // hold is spread from a cut edge.
      if (deferLiveEffect) {
        blit.setImageFilter(deferredFilter);
        dressed = true;
      }
      // AN EFFECT ON THE BLIT IS NOT ADMITTED BY THE INK. The filter
      // spreads the content OUTSIDE the pixels that carry it — that is
      // what a glow is — and the grid describes where the ink is, not
      // where the filter will put it. Blitted whole. A STATIC effect is
      // already in the pixels the grid was taken from, so it keeps its
      // grid.
      //
      // AND THE INK CLIP IS A DEVICE-SPACE CLIP, so it obeys the device
      // bake's rule rather than the picture tier's. A region names whole
      // pixels of the device and ignores the matrix — which is what makes
      // it a set of pixels rather than an outline — so one recorded into a
      // picture is applied, unchanged, in the space that picture is
      // replayed into. It is therefore computed through the replay, and a
      // recording holding one is pinned to the matrix it was made under
      // exactly as one holding a device blit is. An UNPINNED recording —
      // one under a declared motion, which replays under a matrix nobody
      // knows yet — can hold no such clip, and the bake is blitted whole
      // inside it.
      const bool inkAdmitted = !deferLiveEffect && !inst.textureInk.empty() &&
                               unpinnedRecordingDepth == 0;
      drawInkedImage(canvas, inst.textureImage,
                     inkAdmitted ? inst.textureInk : InkGrid{}, dst, totalM,
                     deviceClipOf(), SkSamplingOptions(SkFilterMode::kLinear),
                     dressed ? &blit : nullptr);
      if (inkAdmitted && recordingDepth > 0) ++recordingDeviceBakes;
    });
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
    // The kernel's three-way answer, over this tier's own staleness rule:
    // the node is dirty, a memo's inputs moved, or the leaf paint the
    // recording FROZE IN is not the leaf paint this frame wants.
    PictureBakeTarget target{.painter = this,
                             .inst = &inst,
                             .canvas = &canvas,
                             .hostScale = hostScale,
                             .leafBlend = leafBlend,
                             .leafOpacity = leafOpacity,
                             .scalars = &scalarsNow,
                             .deviceMatrix = totalM,
                             .matrixStable = matrixStable,
                             .deviceClip = deviceClipOf()};
    // …and the pin: a recording holding device blits is exact under the
    // matrix it was made under and is remade under any other; one that
    // deferred a device bake for matrix motion is remade once the matrix
    // has held still for a frame.
    const bool pinMoved = inst.pictureDeviceBakes > 0 &&
                          (totalM != inst.pictureMatrix ||
                           deviceClipOf() != inst.pictureDeviceClip);
    const bool deferredDue = inst.pictureDeviceDeferred && matrixStable;
    if (core::decideBake({.cacheable = true,
                          .held = pictureBake->held(target),
                          .stale = inst.paintDirty || memoStale ||
                                   inst.bakedLeafOpacity != leafOpacity ||
                                   inst.bakedLeafBlend != leafBlend ||
                                   pinMoved || deferredDue}) ==
        core::BakeAction::Take)
      pictureBake->take(target);
    if (profileScope.row != SIZE_MAX)
      profileRows[profileScope.row].cacheState = Composer::CacheState::Picture;
    // The measurement that drives promotion: what the replay of this
    // node's recording cost, which is what the tier is choosing against.
    const measure::Stopwatch replayWatch;
    profDraw("replay", [&] { pictureBake->replay(target); });
    accrue(replayWatch.elapsedMs());
  } else {
    if (!coverageTrace) stats.nodesPainted++;
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
      const measure::Stopwatch liveWatch;
      profDraw("live", [&] {
        paintContent(inst, canvas, hostScale, leafBlend, leafOpacity);
      });
      accrue(liveWatch.elapsedMs());
    }
    inst.paintDirty = false;
  }

  if (needsLayer) canvas.restore();
  canvas.restore();
}

}  // namespace sigil::compose
