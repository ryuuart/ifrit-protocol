#pragma once
// Internal to SigilCompose — the RETAINED runtime shared by the reconciler's
// phase translation units (Reconcile/Layout/Derive/Paint/Transitions/Query.cpp)
// and the Composer facade (Composer.cpp). Holds the retained Instance node and
// the Composer::Impl state; every phase file includes this and defines its
// slice of the Impl/Instance method set. Element DESCRIPTIONS live in
// ComposeInternal.h; this is the resolved, mutable, per-frame side.

#include "ComposeInternal.h"

#include <yoga/Yoga.h>

// markPaintDirtyUp() calls sk_sp::reset() inline, so the ref-counted payload
// types must be complete here (not merely forward-declared).
#include <include/core/SkContourMeasure.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sigil::compose::detail {

/** One float property that can transition: the Choreograph output is the
 *  source of truth while a motion is connected. */
struct AnimatedFloat {
  choreograph::Output<float> value{0.0f};
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this slot's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;
};

struct Instance {
  Composer::Impl *owner = nullptr;
  Instance *parent = nullptr;
  std::shared_ptr<ElementNode> desc;       // resolved (post-memo) description
  std::shared_ptr<ElementNode> memoShell;  // the memo element, if any
  YGNodeRef yoga = nullptr;
  std::vector<std::unique_ptr<Instance>> children;
  std::vector<size_t> paintOrder; // child indices sorted by zIndex

  // Text state
  std::optional<sigil::weave::Paragraph> paragraph;
  sigil::weave::ParagraphLayout textLayout;
  std::vector<sigil::weave::LineMetrics> lines;
  float measuredForWidth = -1.0f;
  YGSize measuredSize{0, 0};
  uint32_t contentRev = 0;    // bumped on text/exclusion change
  uint32_t measuredRev = ~0u; // rev the cached measurement belongs to
  // VariationDrive probe result for the CURRENT text content:
  // -1 unprobed, 0 refused (axis absent or advance-variant), 1 live.
  int8_t driveProbe = -1;

  // Transition state, keyed by property slot
  // The FIXED property slots. Mask gates deliberately are NOT here — their
  // count is a property of the description, so they live in maskAnims (the
  // four that used to be here, kTrimStart/kTrimEnd/kTrimOffset/kWipe, went
  // with trim() and wipe()).
  enum Slot : int {
    kOpacity, kTx, kTy, kRotate, kScale, kFillLerp,
    kGlyphProgress, kSkewX, kSkewY, kScaleX, kScaleY, kMotionT,
    kSlots
  };
  std::unique_ptr<AnimatedFloat> anims[kSlots];
  Fill fillFrom, fillTo; // endpoints for kFillLerp

  // Derive-phase state
  std::vector<SkRect> exclusionsLocal; // flowAround rects, text-local
  SkPath connectorPath;                // routed path (connector OR rail), local
  SkRect connectorFrom = SkRect::MakeEmpty(),
         connectorTo = SkRect::MakeEmpty();
  std::vector<SkPoint> railPoints;     // last resolved rail waypoints
  SkPath routedHitPath;                // stroke-expanded route (hit testing)
  SkPath bandSpine;                    // band(around(key)): borrowed spine
  // spans::fit(key): the keyed boxes, in this node's local space, in the
  // order the element declared them.
  std::vector<std::pair<std::string, SkRect>> spanFitRects;
  // strand::from(key): the keyed paths, in this node's local space —
  // handed to decorations as PaintContext::borrowed.
  std::vector<std::pair<std::string, SkPath>> borrowedPaths;
  // Animated span endpoints (THREE per term — begin, end, offset — per
  // pass, in declaration order). A vector rather than the fixed Slot array
  // because the count is a property of the description, not of the kernel.
  std::vector<std::unique_ptr<AnimatedFloat>> spanAnims;
  // Animated MASK GATE scalars, in declaration order: three per Spans term
  // (begin, end, offset), one per Edge fraction, none for Shape or Alpha.
  //
  // SEPARATELY INDEXED PER MASK, and that is the whole point rather than an
  // implementation detail. Three masks on one node may carry three motions
  // at three rates; if they shared a slot the second would retarget the
  // first and a composition the designer asked for by name would be a race.
  // Positional like spanAnims: a description that changes the SHAPE of its
  // mask list drops the running motions rather than carrying them onto
  // numbers that now mean something else.
  std::vector<std::unique_ptr<AnimatedFloat>> maskAnims;

  // Caching
  sk_sp<SkPicture> picture;
  sk_sp<SkImage> textureImage;
  float textureScale = 1.0f;
  SkRect textureBakeRect = SkRect::MakeEmpty(); // bake covers paint bounds
  // Which SPACE the held bake lives in, and therefore how it must be
  // blitted: a device-space bake is snapped to whole device pixels and
  // drawn with the matrix reset (a literal copy, at any angle); a local
  // one is drawn through the node's transform and resampled by it. The
  // flag exists so a bake taken in one mode is never blitted in the other
  // — the two store different rectangles in textureBakeRect.
  bool textureDeviceSpace = false;
  // Is this node's OWN transform animating? (Geometric slots only — opacity
  // does not move the device rect.) A device-space bake is exact but is
  // pinned to one device rect, so it re-bakes whenever the node moves;
  // the quantized local bake exists precisely so a spinning ornament bakes
  // ONCE and rides its transform. Computed in computeVolatile, which runs
  // every frame whether or not the node is repainted — unlike anything
  // derived from paint history, which a node under a cached parent never
  // accumulates.
  bool transformLive = false;
  // …and is the device rect it actually LANDS on holding still? These are
  // NOT the same predicate, which is the seam a quantized-scale test caught:
  // a node with no animated property of its own still moves every frame
  // under a resizing window or a pinch zoom, and a device-pinned bake would
  // re-bake each time — losing exactly the one-bake-per-quantized-step reuse
  // the local bake exists to provide.
  //
  // Per-node history is sound HERE, and only here, because the device path
  // is refused inside a picture recording — so every node that can reach it
  // is painted every frame and does accumulate history. The first sighting
  // counts as stable: a node's first frame is otherwise forced down the
  // local path and then re-bakes on its second, which is a re-bake the
  // common case (a host that never changes scale) should never pay.
  SkIRect lastDeviceRect = SkIRect::MakeEmpty();
  bool deviceRectSeen = false;
  float bakedLeafOpacity = 1.0f;               // frozen into the recording
  SkBlendMode bakedLeafBlend = SkBlendMode::kSrcOver;
  bool paintDirty = true;
  bool subtreeVolatile = false;
  // Live-material stability (the resolve memo's paint half): set when the
  // node's ONLY volatility is its live material; the painter then replays
  // the cached picture whenever resolve() returns the shader the picture
  // baked (quantized/held materials repaint at their own rate, not the
  // frame rate).
  bool liveMatOnly = false;
  // §17, the same argument for animated SCALARS. Volatility asks whether a
  // motion is CONNECTED; it never asked whether the value moved. A keyframe
  // path's hold segment, a settled easing, and any waypoint pair with equal
  // values are all provably constant — and a recording made with those
  // values is still exact while they hold. Measured before this existed:
  // 29 ms of a 38 ms frame, on runs whose keyframes were between waypoints
  // and therefore not changing at all.
  //
  // Set when the node's content volatility is ENTIRELY these scalars; the
  // painter then re-records only when one of them actually ticks. Kept
  // separate from liveMatOnly rather than merged into it: the two memos
  // compare different things (a shader pointer, five floats), and merging
  // them would have meant rewriting fifteen call sites of the subtlest
  // function in the library to gain nothing.
  bool scalarMemo = false;
  /** The content scalars a recording was baked with. A node that has none
   *  compares equal to itself forever.
   *
   *  §3.6's REPAIR, and the one hard constraint the masking family was
   *  designed around. This used to be a FIXED five-float struct —
   *  trimStart/trimEnd/trimOffset/wipe/glyph — and that fixed size was the
   *  whole obstacle: it is why `spanVolatile` is excluded from this memo by
   *  a written decision in Paint.cpp, and therefore why every one of R2's
   *  58 trim→`stroke(spans::…)` ports moved its node from the §17 scalar
   *  memo to per-frame content volatility and out of `Cache::Group`. The
   *  plate ledger was byte-identical, so nothing caught it: byte-identity
   *  is a pixel gate, not a cost gate.
   *
   *  A mask's gate scalars are a BOUNDED, per-node, resolvable-to-floats
   *  list, so they ride here as a vector and an element-level gate keeps
   *  the memo — a held keyframe on a masked node still repaints nothing.
   *  (Per-PASS span endpoints are still excluded; they are a property of an
   *  open pass list, not of the node, and closing that is a separate
   *  piece of work.) */
  struct ContentScalars {
    float glyph = 1.0f;
    /** Every mask gate's animated floats, resolved, in the order
     *  maskAnims indexes them. */
    std::vector<float> gates;
    bool operator==(const ContentScalars &) const = default;
  };
  ContentScalars bakedScalars;
  // §20: the measured-stability RELEASE. computeVolatile resolves the
  // same scalars per frame; after kSettleFrames of identity the node
  // stops declaring content volatility for them, so ANCESTORS can cache
  // across a settled binding (the §17 memo already kept the node's own
  // recording — what never released was the flag). The frame a value
  // moves, the compare fails, volatility re-declares, and the ancestor's
  // recording is refused before anything stale replays.
  ContentScalars settledScalars;
  int settleFrames = 0;
  /** Consecutive stable paints before the release — promotion's own
   *  consecutive-frames bar (§29's kPromoteFrames precedent). */
  static constexpr int kScalarSettleFrames = 8;
  // ---- §30: Cache::Group, the SUBTREE value memo -------------------------
  // §17 asked "did this node's own animated scalars move". A group asks the
  // same question of a whole subtree, and for a different reason: kumiko's
  // 523 strips are volatile not because their CONTENT changes — their
  // pictures are stable — but because each carries a bound opacity and
  // scale on a 6.4 s entrance. Nothing about them is cacheable by the
  // volatility rule, and everything about them is cacheable for the four
  // seconds a frame the entrance is not running.
  //
  // `groupSafe` is the subtree-wide AND: does this node, and everything
  // under it, carry only volatility a float comparison can SEE? A live
  // material, an animated decoration, a GIF frame, a bound Fill or a
  // Cache::None paint program all move pixels with no float to compare, so
  // one of them anywhere below a group root refuses the whole thing rather
  // than baking something that goes stale silently.
  bool groupSafe = false;
  // …and may THIS node be a group root — cacheMode is Group, its own paint
  // is memo-visible, every child is groupSafe, no backdrop filter. The
  // root's own blend and opacity are deliberately allowed: they are applied
  // by paint()'s saveLayer OUTSIDE the bake, exactly as they would be
  // applied outside the live paint.
  bool groupRootOK = false;
  bool groupWarned = false; // the refusal is printed once per instance
  // The subtree's animated scalars as of the PREVIOUS frame, in tree order.
  // Compared by value, not by hash: a 64-bit digest of 2000 floats is a
  // small chance of blitting last second's picture forever, and the vector
  // costs 8 KB on the one node in a tree that asked for it.
  std::vector<float> groupPrev;
  bool groupPrevSeen = false;
  // Auto texture promotion: a rolling estimate of what this node's PAINT
  // costs each frame — a picture replay for a cached subtree, the live
  // draw for a leaf that never records one — and how many consecutive
  // frames it has been expensive. Promotion is sticky (autoTexture) until
  // the node goes dirty.
  float replayMs = 0;
  uint8_t hotFrames = 0;
  bool autoTexture = false;
  // Temporal stability, for the liveMatOnly case: the fraction of recent
  // frames on which the live material resolved to the SHADER ALREADY
  // BAKED. quantizeTime(10) at 60 FPS settles near 0.83; a material bound
  // to a continuous Output sits at 0 and never promotes. This is why the
  // rule is measured instead of read off quantizeTime(): a bound Output
  // that happens to be holding still is just as cacheable, and a quantized
  // one being driven past its step rate is not.
  float liveStableRate = 0;
  // True when this node OR anything below it blends against what is already
  // on the canvas (a non-srcOver blend, or a backdrop filter). Such a
  // subtree cannot be baked into a transparent layer and blitted back: the
  // blend would resolve against transparent black instead of the real
  // backdrop, and the pixels would differ. Computed by computeVolatile.
  bool subtreeReadsBackdrop = false;
  // The same question asked of the node's OWN paint alone, without its
  // children. The two halves are what §15 turns on: a bake that replaces
  // only the node's own layer, with live children drawn over the BLIT,
  // fails when the OWN paint would resolve against transparent black —
  // and is unaffected by what the children do, because they composite
  // against the blitted destination exactly as they would have against a
  // freshly rasterized one. `subtreeReadsBackdrop` (own OR children) is
  // still the right question for whole-subtree promotion, where the
  // children ARE inside the bake.
  bool ownReadsBackdrop = false;
  // §15's other half: is the node's OWN content volatile, as distinct from
  // its subtree's? `subtreeVolatile && !ownContentVolatile` is exactly "the
  // children are what makes this node uncacheable", which is the split
  // bake's entire premise — a static full-canvas ground plane carrying one
  // moving disc measured 34.9 ms of self time doing nothing but redrawing
  // itself so the disc could land on top.
  bool ownContentVolatile = false;
  // ---- the split bake (§15) ------------------------------------------------
  // The node's own paint, baked alone; the children are painted LIVE over
  // the blit. Deliberately NOT `textureImage`: a node can only ever hold one
  // of the two (the split needs subtreeVolatile, every other bake needs the
  // opposite), but sharing the slot would make each path's staleness rules
  // answer for the other's, and they are different rules.
  sk_sp<SkImage> ownImage;
  SkRect ownBakeRect = SkRect::MakeEmpty(); // device rect the bake covers
  float ownPaintMs = 0;                     // EMA of the own-paint cost
  uint8_t ownHotFrames = 0;
  // Consecutive frames on which the own bake had to be REMADE. A bake per
  // frame costs more than the live draw it replaced, so a node whose own
  // paint really is invalidated every frame must not hold its promotion on
  // the strength of a measurement taken while it was still cheap.
  uint8_t ownRebakes = 0;
  bool splitBake = false; // sticky, like autoTexture
  // Staleness for `ownImage`, and the reason it cannot be `paintDirty`:
  // markPaintDirtyUp() propagates a descendant's patch to every ancestor,
  // which is correct for a RECORDING (it baked the child's draw calls) and
  // wrong for a split bake (the children were never in it). Set only on the
  // node whose own description changed.
  bool ownPaintDirty = true;
  bool hasPendingLiveFill = false;
  Fill pendingLiveFill;
  sk_sp<SkShader> bakedLiveShader;

  // The layout rect this node was last painted/recorded at. ensureLayout's
  // post-pass compares and invalidates: a SIZE change stales this node's own
  // recording (its content baked the old bounds — text lines, geometry
  // materials' uResolution, rrect geometry); a POSITION change stales the
  // parent's recording (which baked the old offset). Without this, cached
  // ancestors replay stale geometry after any relayout that wasn't caused by
  // a prop patch — the latent resize-staleness gap.
  SkRect lastLayoutRect = SkRect::MakeLTRB(-1, -1, -1, -1);

  // Resolved custom-outline cache: generators (blobs, rounded stars) can be
  // arbitrarily expensive — resolve once per (description, size). Desc pointer
  // identity keys invalidation: every patch swaps the description.
  SkPath outlineCache;
  SkSize outlineCacheSize = {-1.0f, -1.0f};
  const ElementNode *outlineCacheDesc = nullptr;

  // §16: stamped-brush bakes live with the NODE (handed to decorations via
  // PaintContext::stamps), so a brush value rebuilt every describe reuses
  // its art's bake instead of re-rastering it.
  StampCache stampCache;

  /** THE MOTION-PATH TABLE — `Element::travel()`'s measured curve.
   *
   *  Cached against the two INPUTS that determine it, never behind a
   *  dirty flag: the `Shape` VALUE (so a comparable scheme keeps its
   *  table across describes, and the raw-callable escape hatch re-measures
   *  exactly as it re-describes) and the SIZE it was resolved at (which is
   *  the parent's box, so a relayout re-measures). That is the house rule
   *  `world::CameraPath` states — compare against the destination, not a
   *  "have I run" flag — with the second input the 3D case never had. */
  struct MotionCache {
    Shape shape;                  // the value this table was built from
    SkSize size{-1.0f, -1.0f};    // …at this parent size
    std::vector<sk_sp<SkContourMeasure>> contours;
    std::vector<float> starts;    // cumulative length before each contour
    float total = 0;
    bool closed = false;          // every contour closed → t WRAPS
  };
  std::unique_ptr<MotionCache> motion;

  ~Instance();
  float resolveFloat(Instance::Slot slot, const Animatable<float> &v) const;
  /** The same resolution over an explicitly-held motion — the span
   *  endpoints, whose count the description decides. One body: a bound
   *  Output wins, then a running ramp, then the plain value. */
  float resolveFloatAt(const AnimatedFloat *anim,
                       const Animatable<float> &v) const;
  /** Resolve every stroke pass's claimed runs for this frame, with
   *  rest() complements applied. Empty when the node has no passes. */
  std::vector<std::vector<Span>> resolveSpans(const SkPath &outline) const;
  /** Resolve every mask gate's animatable floats for this frame, in the
   *  order maskAnims indexes them (and ContentScalars::gates stores
   *  them) — every value, live or settled, because the memo compares what
   *  the recording was BAKED with. Empty when the node carries no mask, or
   *  only shape/alpha gates, which have no numbers. */
  std::vector<float> resolveGateValues() const;

  /** A change here stales every ancestor's recording too.
   *
   *  `ownPaint` says whether THIS node's own paint changed. It is false for
   *  exactly one caller — a child whose layout POSITION moved, which stales
   *  the parent's recording (it baked the old offset) and leaves the
   *  parent's own paint untouched. §15's split bake reads that distinction;
   *  nothing else does, and passing true is always the safe answer. */
  void markPaintDirtyUp(bool ownPaint = true) {
    if (ownPaint) {
      ownPaintDirty = true;
      ownImage.reset();
    }
    for (Instance *i = this; i; i = i->parent) {
      if (i->paintDirty && i != this)
        break; // ancestors already invalidated
      i->paintDirty = true;
      i->picture.reset();
      i->textureImage.reset();
    }
  }
};

// ---- cross-TU paint/shape helpers -----------------------------------------

/** The matrix's true maximum geometric scale — how many device pixels one
 *  local unit can span, whatever the rotation.
 *
 *  NOT max(|getScaleX()|, |getScaleY()|). Those are the matrix DIAGONAL, and
 *  a quarter turn moves the whole scale into the SKEW terms: at ±90° the
 *  diagonal is exactly (0, 0), because Skia's setRotate snaps cos(90°) to
 *  zero. Every raster-target decision that read the diagonal therefore saw
 *  "scale 0", clamped to the 0.25 floor, baked a rotated node at QUARTER
 *  resolution and linear-upscaled it 4×. Measured on a 196×33 pill against
 *  the identical uncached render: mean |Δ| over its ink was 30–32/255 at
 *  ±90°, 14.5 at 45° (0.707 → the 0.75 step), 2.4 at 180° (|−1| → correct).
 *  Singular values instead; a pure rotation reports 1. */
inline float maxScaleOf(const SkMatrix &m) {
  SkScalar s[2];
  if (m.getMinMaxScales(s) && s[1] > 0)
    return s[1];
  // Perspective (getMinMaxScales refuses): the diagonal is all there is.
  return std::max(std::abs(m.getScaleX()), std::abs(m.getScaleY()));
}

/** The paint-transform pivot: fractional by default, node-local px under
 *  transformOriginPx(). One definition for paint(), recordBounds(), and
 *  the hit-test inverse. */
inline SkPoint resolveOrigin(const PaintProps &p, float w, float h) {
  return p.originPx ? SkPoint{p.originX, p.originY}
                    : SkPoint{w * p.originX, h * p.originY};
}

inline SkRRect cornersRRect(const SkRect &bounds, const Corners &c) {
  const SkVector radii[4] = {{c.topLeft, c.topLeft},
                             {c.topRight, c.topRight},
                             {c.bottomRight, c.bottomRight},
                             {c.bottomLeft, c.bottomLeft}};
  SkRRect rrect;
  rrect.setRectRadii(bounds, radii);
  return rrect;
}

// Yoga measure/baseline callbacks (defined in Layout.cpp; referenced by
// Reconcile.cpp when it installs them on a text leaf's YGNode).
YGSize measureTextNode(YGNodeConstRef node, float width,
                       YGMeasureMode widthMode, float heightHint,
                       YGMeasureMode heightMode);
float baselineOfTextNode(YGNodeConstRef node, float width, float height);

} // namespace sigil::compose::detail

namespace sigil::compose {

struct Composer::Impl {
  motion::Ticker &ticker;
  sigil::weave::FontContext &fonts;
  const motion::FrameClock *clock = nullptr;

  SkSize size = SkSize::MakeEmpty();
  std::unique_ptr<detail::Instance> root;
  YGConfigRef yogaConfig = nullptr;
  bool needsLayout = true;
  bool contentDirty = true;
  std::unordered_map<std::string, detail::Instance *> byKey;
  // Slots get their OWN index. They live in byKey too (so bounds() and
  // hitTest() still answer for a slot's name), but a slot's CONTENT may
  // legitimately carry a root .key() with the same name — and since a
  // child is indexed after its parent, last-writer-wins let it shadow the
  // slot, so every later renderSlot() returned silently and the slot
  // froze on its first value. Two namespaces, no collision.
  std::unordered_map<std::string, detail::Instance *> bySlot;
  // The EDGE STORE, rebuilt with the key index each render: routed nodes
  // (connector()/rail()) as a flat list in tree order, plus the back-index
  // anchor-key → routes-anchored-there. The derive pass iterates these flat
  // lists instead of recursing the whole tree, and routesAt() answers graph
  // queries ("which edges touch this node") in O(routes-at-node).
  std::vector<detail::Instance *> routedInstances;
  std::vector<detail::Instance *> flowInstances; // flowAround() text nodes
  std::unordered_map<std::string, std::vector<detail::Instance *>>
      routesByAnchor;
  bool volatileDirty = true; // recompute needed (render or animation)
  bool tickerWasActive = false;
  // §20: instances whose scalar volatility is RELEASED (settled bound
  // gates/glyph progress). Rebuilt by every computeVolatile walk; scanned
  // once per draw so an EXTERNALLY-driven Output that moves re-declares
  // volatility the same frame — the walk itself only re-runs on
  // reconcile/ticker, which is exactly why the flag never released
  // before. Guarded by !volatileDirty (a pending recompute means the
  // tree changed and these pointers may be stale).
  std::vector<detail::Instance *> releasedScalars;
  void scanReleasedScalars(); // defined in Paint.cpp beside the memos
  // Recomputed with the key index (so unmounting the last derived/pinned
  // node actually clears them — they were latch-only before).
  bool hasDerived = false; // any flowAround/connector/rail in the tree
  bool hasCustomLayout = false;
  bool hasCenterPins = false; // any centerAt() in the tree
  bool liveOnly = false; // snapshot(): skip per-node caches
  Effect view;           // output view transform (null filter = pass-through)
  // staggerChildren(): the accumulated extra mount delay for the subtree
  // being mounted right now (depth-first, saved/restored per child — a
  // nested staggered container compounds on its parent's carry).
  float mountDelayCarryMs = 0;

  mutable Stats stats;
  // ---- per-node paint profiler (opt-in; Composer::setProfiling) ----------
  // profChildMs is the running total the CURRENT node's children have cost;
  // each node saves its parent's value, zeroes it, paints, then reports its
  // own total upward. That gives selfMs = totalMs - children without a
  // second traversal.
  bool profileEnabled = false;
  bool autoPromote = true; // Composer::setAutoTexturePromotion (the INTENT)
  bool promotionExplicit = false; // did the host call the setter?
  // The value paint() actually reads, recomputed each draw(). Differs from
  // `autoPromote` only under the backend-aware default: automatic promotion
  // is OFF on a Graphite/GPU surface unless the host asked for it
  // explicitly, because the whole cost model that drives it — the 1 ms
  // threshold, the stability EMA, the temporal gate — is measured on
  // op-RECORDING time, which describes raster and not GPU. Measured: on GPU,
  // promotion on vs off is noise (kumiko 112–144 vs 111–124 ms, the
  // run-to-run variance dwarfing the delta), because it rarely crosses the
  // recording-time threshold and rarely fires; when it would fire, the
  // bake+sync+upload costs more than the ~0.8 ms recording it replaces. It
  // is dead weight there, not a win and not a regression, so it is off until
  // a GPU-timestamp cost model can re-enable it with evidence. The global
  // switch still overrides in both directions.
  bool autoPromoteEffective = true;
  // Promoted bakes are pixels, and a dense study has a lot of full-canvas
  // nodes: chaucer_astrolabe paints 174 at 2400x1600, which is 15 MB each.
  // A budget, carried from the previous frame (paint order is stable, so
  // the previous frame's total is the right question to ask before adding
  // one more), keeps an automatic win from becoming an automatic OOM.
  size_t promotedBytes = 0;     // accumulated during the current paint
  size_t promotedBytesLast = 0; // what the previous frame ended up holding
  // >0 while painting INTO an SkPicture. Device-space bakes are pinned to
  // a device rect and must not be recorded into a picture that can replay
  // under a different matrix.
  int recordingDepth = 0;
  static constexpr size_t kPromotedBudget = 192u * 1024 * 1024;
  std::vector<Composer::NodeCost> profileRows;
  double profChildMs = 0;
  int profDepth = 0;
  // render()/renderSlot() phase time accumulated since the previous draw();
  // draw() publishes it as stats.reconcileMs and zeroes the accumulator.
  double reconcileAccumMs = 0;

  Impl(motion::Ticker &t, sigil::weave::FontContext &f) : ticker(t), fonts(f) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return clock ? clock->elapsed() : 0.0; }

  // ---- reconcile (Reconcile.cpp) ----
  std::unique_ptr<detail::Instance>
  mount(const std::shared_ptr<detail::ElementNode> &node,
        detail::Instance *parent);
  void patch(detail::Instance &inst,
             std::shared_ptr<detail::ElementNode> node);
  void patchChildren(detail::Instance &inst,
                     const std::vector<Element> &newChildren);
  void applyLayoutProps(detail::Instance &inst);
  void applyTransitions(detail::Instance &inst, const detail::ElementNode &prev,
                        const detail::ElementNode &next);
  void applyMountTransitions(detail::Instance &inst,
                             const detail::ElementNode &node);
  std::shared_ptr<detail::ElementNode>
  resolveMemo(detail::Instance *existing,
              const std::shared_ptr<detail::ElementNode> &node, bool &described);
  void rebuildKeyIndex();
  void indexKeys(detail::Instance &inst);

  // ---- volatility & caching (Paint.cpp) ----
  bool computeVolatile(detail::Instance &inst);
  // Scratch for the §30 subtree value memo, swapped with the group root's
  // `groupPrev` each frame so a settled group allocates nothing at all.
  std::vector<float> groupScratch;

  // ---- layout (Layout.cpp) ----
  bool applyCustomLayouts(detail::Instance &inst);
  bool applyCenterPins(detail::Instance &inst);
  void ensureLayout();
  void syncLayoutRects(detail::Instance &inst);
  void layoutText(detail::Instance &inst, float constraint);
  SkRect instanceRect(const detail::Instance &inst) const;
  SkRect positionedRect(const detail::Instance &inst) const;
  SkRect absoluteRect(const detail::Instance &inst) const;

  // ---- derive (Derive.cpp) ----
  /** One pass over the flat flow/route lists (the edge store) — no tree
   *  recursion. Returns true when a text exclusion changed (second layout
   *  pass needed). */
  bool resolveDerived();
  bool deriveFlow(detail::Instance &inst);
  void deriveRoute(detail::Instance &inst);

  // ---- the node's paint transform, resolved once (Paint.cpp) ----
  /** Every animated number in paint()'s matrix stack, for ONE frame.
   *
   *  One resolver, three consumers — paint()'s matrix, recordBounds()'s
   *  child union and hitInstance()'s inverse — because the three must
   *  describe the same matrix or a node draws where it cannot be hit.
   *  They used to resolve eight slots each, by hand, three times; adding
   *  `travel()` (which REPLACES tx/ty and ADDS to rot) would have been
   *  three chances to disagree. */
  struct NodeTransform {
    float tx = 0, ty = 0, rot = 0, scl = 1, sx = 1, sy = 1, skx = 0, sky = 0;
    /** Does anything past the translate need the origin pivot at all? */
    bool pivoted() const {
      return rot != 0 || scl != 1 || sx != 1 || sy != 1 || skx != 0 ||
             sky != 0;
    }
  };
  NodeTransform transformOf(detail::Instance &inst);
  /** Where on its motion path this node sits, in its PARENT's space, and
   *  the auto-orient angle in degrees. Nullopt when no path is engaged
   *  (absent, empty, or resolving to no measurable length) — the
   *  translate lanes then stand. Rebuilds the instance's arc-length table
   *  when the Shape value or the parent size no longer matches. */
  std::optional<std::pair<SkPoint, float>>
  motionPathSample(detail::Instance &inst, const SkSize &frame);

  // ---- paint (Paint.cpp) ----
  float hostScale = 1.0f; // device px per layout px at draw() entry
  void paint(detail::Instance &inst, SkCanvas &canvas);
  void paintTextOnPath(detail::Instance &inst, SkCanvas &canvas,
                       const TextPath &spec, SkSize size);
  void paintKineticText(detail::Instance &inst, SkCanvas &canvas,
                        const GlyphFx &fx);
  /** Which half of a node's paint to emit.
   *
   *  The node's own paint is a CONTIGUOUS PREFIX of paintContent —
   *  backgrounds, clip, fill, echoes, overlays, leaf content — ending
   *  exactly at the children loop. Everything after that loop (the clip
   *  restore, the FOREGROUNDS, the wipe and effect restores) belongs to the
   *  children half: foregrounds paint over the children, so they can never
   *  be in an own-paint bake. That is why this is a phase flag and two
   *  skips rather than a split function. */
  enum class Phase : uint8_t {
    All,          ///< the whole node, unchanged
    OwnOnly,      ///< the prefix: no children, no foregrounds
    ChildrenOnly, ///< the children and the foregrounds over them
  };
  void paintContent(detail::Instance &inst, SkCanvas &canvas, float contentScale,
                    SkBlendMode leafBlend = SkBlendMode::kSrcOver,
                    float leafOpacity = 1.0f, Phase phase = Phase::All);
  const SkPath &resolveOutline(detail::Instance &inst, SkSize size) const;
  /** What the node paints BY ITSELF, in its own local space: its box grown
   *  by every decoration's declared bleed and any routed path, and NOTHING
   *  from its children. §15's split bake sizes its layer with this — and
   *  the independence from the children is the load-bearing part, not an
   *  optimisation: `recordBounds` unions the children in, so it changes
   *  every frame a child moves, and a bake rect that changes every frame is
   *  a bake remade every frame. */
  SkRect ownPaintBounds(detail::Instance &inst);
  SkRect recordBounds(detail::Instance &inst);

  // ---- hit testing / queries (Query.cpp) ----
  bool shapeContains(detail::Instance &inst, SkPoint local, SkSize size) const;
  std::optional<std::string> hitInstance(detail::Instance &inst,
                                         SkPoint parentPt,
                                         const std::string *inheritedKey);
};

} // namespace sigil::compose
