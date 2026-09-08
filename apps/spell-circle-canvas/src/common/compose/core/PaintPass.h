#pragma once

/** @file
 * The state one node's paint is decided on, and what the cache tiers read
 * it through: the node's placement, the effects lifted off its content, the
 * memos that say its cached pixels are still the right ones, the composite
 * its blend and opacity take, and the device geometry every bake is
 * measured in.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPaint.h>
#include <include/core/SkRect.h>
#include <include/core/SkTypes.h>  // SkDebugf
#include <sigilmeasure/time/Stopwatch.h>

#include <cstdint>

#include "ComposeRuntime.h"
#include "PaintProfile.h"

namespace sigil::compose {

/** Promotion thresholds. A node must cost more than this to replay, for
 *  this many consecutive frames, before the library re-bakes it. 1 ms is
 *  ~6% of a 60 FPS frame — well above noise, far below the point where a
 *  sketch is in trouble; 8 frames keeps a one-off stall from promoting
 *  anything. Shared by the whole-subtree promotion and the split bake,
 *  which warm on the same bar. */
constexpr double kPromoteMs = 1.0;
constexpr uint8_t kPromoteFrames = 8;

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

/** One node's paint, as the tiers below it read it. Filled in the order the
 *  walk computes it: the placement first, then the effects and memos the
 *  cache decision stands on, then the device geometry a bake is measured
 *  in and the promotion verdict taken from it. */
struct PaintPass {
  PaintPass(Composer::Impl& impl, detail::Instance& inst, SkCanvas& canvas,
            const SkRect& rect, ProfileScope& profile)
      : impl(impl),
        inst(inst),
        node(*inst.description),
        canvas(canvas),
        rect(rect),
        profile(profile) {}

  Composer::Impl& impl;
  detail::Instance& inst;
  const detail::ElementNode& node;
  SkCanvas& canvas;
  /** The node's layout rect, in its parent's space. */
  SkRect rect;
  ProfileScope& profile;
  /** The node's own opacity this frame, already clamped. */
  float opacity = 1.0f;

  // ---- the placement ----
  /** The node's transform lanes, as the resolver produced them. */
  const Composer::Impl::NodeTransform* tf = nullptr;
  /** Whether the node is placed by a flattened 4x4 — a depth lane off rest,
   *  or a node standing in a shared space — rather than by the flat
   *  translate-and-concat the transform lanes describe. */
  bool placedByPlane = false;
  /** Whether the node hosts a shared space for its children. */
  bool spaceHost = false;
  /** The device matrix the node's own transform is applied ON TOP OF. The
   *  bake ladder asks it what scale a declared scale motion is heading
   *  for, which is a question about this node's own lane and not about the
   *  matrix it currently reads as. */
  SkMatrix parentCanvasM = SkMatrix::I();

  // ---- the effect lifted off the content ----
  /** The layer effect the content bake leaves out, run over the blit
   *  instead. */
  sk_sp<SkImageFilter> deferredFilter;
  /** The LIVE tier's own answer, kept apart from the static one: it is what
   *  lets a node hold a bake its content volatility would otherwise drop,
   *  and the static tier claims nothing of the sort. */
  bool deferLiveEffect = false;
  /** Either tier's, as the bake below reads it. */
  bool deferEffect = false;
  /** Whether a STATIC layer effect over settled content may be lifted off
   *  the content raster. The candidacy only; the tier is taken at the local
   *  bake, which is the one that holds the two surfaces it needs. */
  bool staticEffectCandidate = false;

  // ---- the memos ----
  /** The animated content scalars AS OF THIS FRAME — what a recording or a
   *  bake taken now is stamped with. */
  detail::Instance::ContentScalars scalarsNow;
  /** "Are the cached pixels still the RIGHT pixels?" — the two memos answer
   *  for their own input and abstain on the other. */
  bool memoStale = false;
  /** "May this node keep its cached pixels?" — either nothing about it is
   *  volatile, every input it reads is memoized and provably unchanged, or
   *  the volatility is entirely outside the pixels the cache holds. */
  bool cacheHolds = false;

  // ---- the composite ----
  /** The blend a fill-only leaf routes onto its fill paint instead of a
   *  saveLayer, and kSrcOver for every other node. */
  SkBlendMode leafBlend = SkBlendMode::kSrcOver;
  float leafOpacity = 1.0f;
  /** Whether the node's blend and opacity ride its blit's paint rather than
   *  a device-clip-sized saveLayer. */
  bool deferBlendToBlit = false;
  /** Whether that saveLayer was opened, and so must be restored. */
  bool needsLayer = false;

  // ---- the device geometry ----
  /** WHAT THIS NODE'S DRAWS REACH THE DEVICE THROUGH. Outside a recording
   *  it is the canvas's own matrix. Inside one the canvas's matrix is in
   *  the recording's space, and the recording is replayed under a matrix of
   *  its own; the composition is the device grid the node is drawn on,
   *  which is the grid a device-space bake must sample and the rect its
   *  blit must land on. Every bake tier reads this and never the canvas
   *  matrix alone. */
  SkMatrix totalM = SkMatrix::I();
  /** "Is the node where it was last frame?" — its own history at the root,
   *  where it is painted every frame, and the outermost open recording's
   *  inside one, where it is painted only when that recording is taken. */
  bool matrixStable = true;
  /** Upright, unmirrored, unrotated and unskewed — the condition under
   *  which an integer device offset cancels exactly, which is what both the
   *  promoter and the split bake are held to. */
  bool upright = false;
  /** THE DEVICE-BAKE RULE, which the Cache::Group, Cache::Texture and split
   *  tiers share: a device-space bake blits with the matrix reset at an
   *  ABSOLUTE device rect, so it is exact under one matrix and wrong under
   *  every other. It may be taken at the root, or inside recordings that
   *  are all PINNED, and never inside an unpinned one. */
  bool deviceBakeable = false;

  // ---- the promotion verdict ----
  /** Every condition under which a bake would produce different pixels or
   *  would not pay for itself, as a BIT MASK over Composer::Promotion so
   *  ALL of them are reported rather than the first. */
  uint16_t refusals = 0;
  /** Whether automatic caching is switched off for this node — by the
   *  host's policy, or by the node's own cacheMode. */
  bool optedOut = false;
  /** Whether the node is eligible for an automatic bake at all. */
  bool promotable = false;
  /** EAGER SKIPS THE STOPWATCH AND NOTHING ELSE: the policy answers "is
   *  this node expensive enough to be worth baking" with yes, and changes
   *  none of the refusals. */
  bool eager = false;

  // ---- what the tiers ask of the pass ----

  /** The node's paint bounds in its own space. recordBounds() walks the
   *  whole subtree and three tiers ask for it, so it is memoised per paint
   *  — lazily, so a node that reaches none of them never pays for it. */
  const SkRect& localBounds() {
    if (!localBoundsDone) {
      localBoundsDone = true;
      localPaintBounds = impl.recordBounds(inst);
    }
    return localPaintBounds;
  }

  /** THE DEVICE CLIP, in the space a device blit lands in.
   *  `getDeviceClipBounds()` is in base device coordinates, which is the
   *  space `resetMatrix()` draws in, including inside a saveLayer; inside a
   *  recording the canvas's clip is in the recording's own space and is
   *  carried out through the replay, the same way the matrix is. */
  SkIRect deviceClip() const;

  /** The node's paint bounds as whole device pixels, with the margin every
   *  bake is allocated with (bakeMargin — the content must stand clear of
   *  the surface's own edge, or the scan converter answers a different
   *  coverage than the live paint's). */
  SkIRect deviceRect();

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
   *  stamped on the instance and every tier re-bakes when it moves: a clip
   *  narrows and widens for reasons the node's own bounds cannot see — an
   *  ancestor's layer opened over its box while it fades in, a panel
   *  revealing, a window growing — and a bake held across that blits the
   *  cut for as long as the node's content stands still. */
  void clipBakeLayer(SkCanvas* lc, const SkIRect& bake) const;

  /** A device blit: the matrix reset, so the image lands at an absolute
   *  device rect — and inside a recording, the replay's inverse
   *  concatenated, so the replay carries it back to exactly that rect.
   *  Counted, because the recording holding it is pinned to its matrix from
   *  here on. */
  void deviceBlit(const sk_sp<SkImage>& image, const SkIRect& at,
                  const SkPaint* paint);

  /** The node's layer effect, resolved against its box and clock — the same
   *  context the node's own paint builds, minus the marks outline, because
   *  nothing of the node has been painted yet. */
  sk_sp<SkImageFilter> resolveLayerFilter();

  /** Record the promotion verdict on this node's profile row. */
  void note(Composer::Promotion p);

  /** What this node cost to paint the way it painted — a picture replay for
   *  a cached subtree, the live draw for a leaf — folded into the rolling
   *  estimate, and the promotion decision taken from it. */
  void accrue(double cost);

  /** The two restores a node's paint ends on: the layer its blend and
   *  opacity opened, and the save its transform stands under. */
  void close() {
    if (needsLayer) canvas.restore();
    canvas.restore();
  }

  /** Time one draw against the COMPOSE_PROF threshold, printing it when it
   *  runs over. */
  template <class Draw>
  void profDraw(const char* what, Draw&& draw) {
    const double threshold = profileThresholdMs();
    if (threshold < 0.0) {
      draw();
      return;
    }
    const measure::Stopwatch watch;
    draw();
    const double ms = watch.elapsedMs();
    if (ms > threshold)
      SkDebugf("[prof] %s %s kind=%d rect=%.0fx%.0f %.1fms\n", what,
               node.key.empty() ? "(anon)" : node.key.c_str(), (int)node.kind,
               rect.width(), rect.height(), ms);
  }

  SkRect localPaintBounds = SkRect::MakeEmpty();
  bool localBoundsDone = false;
};

// ---------------------------------------------------------------------------
// The cache ladder, one tier per file, in the order paint() dispatches
// through them.

/** Automatic texture promotion (PromotionTier.cpp): the refusal mask, the
 *  primary verdict and the device-bake rule, written onto the pass. */
void decidePromotion(PaintPass& pass);

/** …and the bake it admits. True when the node was blitted and its paint is
 *  over. */
bool paintPromotedBake(PaintPass& pass);

/** The split bake (SplitBake.cpp): the node's OWN paint baked and blitted,
 *  its children drawn live over it. True when the node was a candidate, in
 *  which case its paint is over either way. */
bool paintSplitBake(PaintPass& pass);

/** Cache::Group (GroupBake.cpp): the whole subtree, held by a VALUE memo.
 *  True when the node was blitted and its paint is over; a ticking group
 *  falls through and paints live. */
bool paintGroupBake(PaintPass& pass);

/** The asked-for texture bake (TextureBake.cpp): the device-space exact
 *  bake, else the quantized local one, and the blit over it. True when the
 *  node's paint is over. */
bool paintTextureBake(PaintPass& pass);

/** The picture tier (PictureTier.cpp): the node's own paint recorded into a
 *  replayable command list, taken or replayed by the kernel's bake seam. */
void paintThroughPicture(PaintPass& pass);

/** The live draw (PictureTier.cpp), timed when the node could be promoted
 *  and untimed when it could not. */
void paintLive(PaintPass& pass);

}  // namespace sigil::compose
