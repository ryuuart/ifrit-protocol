#pragma once
// Internal to SigilCompose — the RETAINED runtime shared by the reconciler's
// phase translation units (Reconcile/Layout/Derive/Paint/Transitions/Query.cpp)
// and the Composer facade (Composer.cpp). Holds the retained Instance node and
// the Composer::Impl state; every phase file includes this and defines its
// slice of the Impl/Instance method set. Element DESCRIPTIONS live in
// ComposeInternal.h; this is the resolved, mutable, per-frame side.

#include <yoga/Yoga.h>

#include "ComposeInternal.h"

// markPaintDirtyUp() calls sk_sp::reset() inline, so the ref-counted payload
// types must be complete here (not merely forward-declared).
#include <include/core/SkCanvas.h>  // NodeTransform::concatTo's elementary ops
#include <include/core/SkContourMeasure.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>

#include <algorithm>
#include <cmath>
#include <iterator>  // std::size, for the kSlotSpecs asserts
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sigil::compose::detail {

struct Instance;

/** WHETHER THE CHILDREN OF THIS NODE CARRY YOGA NODES. Stated once because
 *  two places ask — the mount that creates the node and the patch that
 *  decides a reused instance is in the wrong mode — and a drift between
 *  them is a hard Yoga abort rather than a wrong pixel.
 *
 *  Two families answer no. A `positioned()` container's descendants carry
 *  their rects in their own descriptions instead of in the flex engine. And
 *  a TEXT node's children never can: Yoga forbids children under a node
 *  that has a measure function, and the measure function is how text sizes
 *  to its container. So text keeps measuring, and a `rich().slot()` pill
 *  takes its box from the paragraph — which is the only thing that knows
 *  where the reserved run landed. */
inline bool childrenCarryYoga(const Instance& inst);

/** One float property that can transition: the Choreograph output is the
 *  source of truth while a motion is connected. */
struct AnimatedFloat {
  choreograph::Output<float> value{0.0f};
  bool started = false;
  // Where the running motion is headed — lets a patch that does not change
  // this slot's target leave the motion ALONE (no hitch, no re-held delay).
  float target = 0.0f;
};

/** ONE RESOLVED `flowAround` TARGET, in the text node's own space.
 *
 *  A target that resolves a SILHOUETTE of its own — a `shape()`, a routed
 *  connector or rail — is subtracted by that outline, concavities and holes
 *  included, so text runs into the notch of a star and through the hole of
 *  a ring. A target that resolves none is subtracted by its BOX, which is
 *  the whole of what it occupies. `circle` is the analytic case: a round
 *  silhouette costs one square root per line band instead of a walk of a
 *  flattened outline, and the answer is the same one.
 *
 *  Both forms are one value so the compare that decides whether the text
 *  must be laid out again stays a plain vector compare. */
struct Exclusion {
  SkRect bounds = SkRect::MakeEmpty();  ///< the target's box (or the circle's)
  SkPath path;          ///< the silhouette; empty when the target has none
  bool circle = false;  ///< `bounds`'s inscribed circle IS the silhouette
  bool operator==(const Exclusion& other) const {
    return bounds == other.bounds && circle == other.circle &&
           path == other.path;
  }
};

struct Instance {
  Composer::Impl* owner = nullptr;
  Instance* parent = nullptr;
  std::shared_ptr<ElementNode> desc;       // resolved (post-memo) description
  std::shared_ptr<ElementNode> memoShell;  // the memo element, if any
  YGNodeRef yoga = nullptr;
  std::vector<std::unique_ptr<Instance>> children;
  std::vector<size_t> paintOrder;  // child indices sorted by zIndex

  // Text state
  std::optional<sigil::weave::Paragraph> paragraph;
  sigil::weave::ParagraphLayout textLayout;
  std::vector<sigil::weave::LineMetrics> lines;
  /// The same geometry for a VERTICAL passage, where a "line" is a column
  /// and lineMetrics() answers with nothing. Exactly one of the two lists is
  /// ever non-empty, and which one is the node's writing mode.
  std::vector<sigil::weave::ColumnMetrics> columns;
  float measuredForWidth = -1.0f;
  float measuredForHeight = -1.0f;
  YGSize measuredSize{0, 0};
  float measuredBaseline = 0.0f;  // first character's baseline, from the top
  uint32_t contentRev = 0;        // bumped on text/exclusion change
  uint32_t measuredRev = ~0u;     // rev the cached measurement belongs to
  // rich().slot(): the slot names in the order the content declares them —
  // which is the order weave matches its placeholder records in — and where
  // the finished layout put each one, in this node's own space. A child
  // keyed by one of these names takes that rect as its box.
  std::vector<std::string> textSlotKeys;
  std::vector<std::pair<std::string, SkRect>> textSlotRects;
  // mark(): the rect each anchored child's key resolves to, in this node's
  // own space, resolved once per layout from the placement the paragraph
  // produced. A key that resolved no glyphs is absent, and its child places
  // nothing.
  std::vector<std::pair<std::string, SkRect>> textMarkRects;
  // rich().add(text, styleName): each named run and the text it occupies, in
  // declaration order — what sel::style resolves against. Cleared and
  // rebuilt with the paragraph, so the names a node answers for are exactly
  // the ones its current content declares.
  std::vector<detail::NamedRun> textNamedRuns;
  // onPath(): the run broken across the baseline's contours, and the
  // geometry it was broken across. A SECOND layout beside `textLayout`
  // rather than a replacement for it — `textLayout` is still the node's
  // MEASURE, the run laid straight, and the box it measures is what the
  // baseline is resolved against. Rebuilt when the content, the box or the
  // baseline value changes; the `at` phase is applied at PAINT and never
  // touches it.
  sigil::weave::ParagraphLayout pathLayout;
  std::vector<sigil::weave::LineInterval> pathIntervals;
  float pathTotalLength = 0;   // every contour's arc length together
  float pathRestAt = 0;        // the `at` the layout's entry point baked in
  SkPoint pathCentroid{0, 0};  // Orient::Radial's centre
  bool pathValid = false;
  uint32_t pathRev = ~0u;            // contentRev the path layout belongs to
  SkSize pathSize = {-1, -1};        // the box the baseline resolved against
  std::optional<TextPath> pathSpec;  // the value it was built from

  // Transition state, keyed by property slot
  // The FIXED property slots — one per property every node can carry, so the
  // count is a property of the KERNEL. Mask gates and fx() tracks are
  // deliberately not here: how many a node has is a property of its
  // description, so they live in the maskAnims and trackAnims vectors
  // instead.
  //
  // ADDING A SLOT HERE IS A BUILD FAILURE until it also gets a row in
  // kSlotSpecs below, the one table every consumer of this enum walks.
  enum Slot : int {
    kOpacity,
    kTx,
    kTy,
    kRotate,
    kScale,
    kFillLerp,
    kSkewX,
    kSkewY,
    kScaleX,
    kScaleY,
    kMotionT,
    kTextPathAt,
    kSlots
  };
  std::unique_ptr<AnimatedFloat> anims[kSlots];
  Fill fillFrom, fillTo;  // endpoints for kFillLerp

  // Derive-phase state
  std::vector<Exclusion> exclusionsLocal;  // flowAround targets, text-local
  SkPath connectorPath;  // routed path (connector OR rail), local
  SkRect connectorFrom = SkRect::MakeEmpty(), connectorTo = SkRect::MakeEmpty();
  std::vector<SkPoint> railPoints;  // last resolved rail waypoints
  SkPath routedHitPath;             // stroke-expanded route (hit testing)
  SkPath bandSpine;                 // band(around(key)): borrowed spine
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
  // Animated fx() TRACK progresses, one per track in declaration order.
  //
  // Positional and separately indexed for the same reason mask gates are:
  // three tracks on one text node may run at three rates, and a shared slot
  // would make the third retarget the first. A description that changes the
  // NUMBER of tracks drops the running motions rather than carrying them
  // onto a progress that now drives a different effect.
  std::vector<std::unique_ptr<AnimatedFloat>> trackAnims;

  // ---- fx() selection, resolved once per (content, layout, selector) -------
  //
  // A selector answers one byte per glyph, and answering it can mean an ICU
  // regular expression over the whole paragraph. That is a per-EDIT cost,
  // not a per-frame one: the masks below are rebuilt when the text changes,
  // when the layout reflows (a line selector moves with the break), or when
  // the description's selectors themselves change.
  std::vector<Selector> selectionKeys;
  std::vector<std::vector<uint8_t>> selectionMasks;
  uint32_t selectionRev = ~0u;
  float selectionWidth = -1.0f;

  // Caching
  sk_sp<SkPicture> picture;
  sk_sp<SkImage> textureImage;
  float textureScale = 1.0f;
  SkRect textureBakeRect = SkRect::MakeEmpty();  // bake covers paint bounds
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
  // NOT the same predicate: a node with no animated property of its own
  // still moves every frame under a resizing window or a pinch zoom, and a
  // device-pinned bake would re-bake each time — losing exactly the
  // one-bake-per-quantized-step reuse the local bake exists to provide.
  //
  // AN INVARIANT OF THIS PAIR, NOT OF ANY ONE CALL SITE: it is written only
  // at recordingDepth == 0. A picture can replay under a different matrix
  // than it records at, so a rect observed inside a recording is not this
  // node's device rect — writing it would poison the stability compare and
  // force a spurious re-bake on the node's next live frame. The guard is
  // also what makes the history meaningful at all: every node that can
  // reach a device bake is painted every frame, so it actually accumulates
  // frame-over-frame history; a node painted once into an ancestor's
  // recording accumulates none. The writers, each behind that guard: the
  // Cache::Group device bake and the Cache::Texture device bake, both in
  // paint(). Automatic promotion shares the recordingDepth == 0 refusal but
  // keeps no rect history. Any new writer must sit behind the same check.
  //
  // The first sighting counts as stable: a node's first frame is otherwise
  // forced down the local path and then re-bakes on its second, which is a
  // re-bake the common case (a host that never changes scale) should never
  // pay.
  SkIRect lastDeviceRect = SkIRect::MakeEmpty();
  bool deviceRectSeen = false;
  float bakedLeafOpacity = 1.0f;  // frozen into the recording
  SkBlendMode bakedLeafBlend = SkBlendMode::kSrcOver;
  bool paintDirty = true;
  bool subtreeVolatile = false;
  // Live-material stability (the resolve memo's paint half): set when the
  // node's ONLY volatility is its live material; the painter then replays
  // the cached picture whenever resolve() returns the shader the picture
  // baked (quantized/held materials repaint at their own rate, not the
  // frame rate).
  bool liveMatOnly = false;
  // Set when the node's only content volatility is animated scalars, which
  // lets the painter re-record just when one of those values actually
  // changes. Volatility alone answers whether a motion is connected, not
  // whether it is currently moving: a keyframe's hold segment, a settled
  // easing, and any waypoint pair with equal endpoints are constant while
  // they hold, and a recording baked from them stays exact that whole time.
  //
  // Deliberately separate from liveMatOnly. The two compare different things
  // — a shader pointer versus a list of floats — so a node can qualify for
  // one and not the other.
  bool scalarMemo = false;
  /** The content scalars a recording was baked with. A node that has none
   *  compares equal to itself forever.
   *
   *  To ride here a value must be resolvable to floats outside of painting,
   *  bounded per node, and cheap to compare by value — that is what makes it
   *  usable as a cache key. Mask gate scalars qualify, so a held keyframe on
   *  a masked node repaints nothing.
   *
   *  Per-pass span endpoints do not qualify and are excluded: they belong to
   *  an open-ended pass list rather than to the node, so there is no bounded
   *  set of floats to compare. A node animated only by a per-pass span falls
   *  back to per-frame content volatility and does not cache. */
  struct ContentScalars {
    /** Every fx() track's master progress, resolved, in the order
     *  trackAnims indexes them. Empty on text carrying no tracks. */
    std::vector<float> tracks;
    /** Every mask gate's animated floats, resolved, in the order
     *  maskAnims indexes them. */
    std::vector<float> gates;
    /** The node→root matrix's affine six, for a node whose material paints
     *  in world space under a connected transform (its own or an
     *  ancestor's). That matrix is then a content input of the recording
     *  exactly as a gate fraction is — floats, resolvable outside paint —
     *  so a turning node re-anchors and a held one re-caches with no
     *  machinery beyond this compare. All-zero, not identity, when the node
     *  carries no world-space material: both sites that fill this member
     *  apply the same guard, so the compare stays meaningful. */
    std::array<float, 6> world{};
    /** The resolved value of a fill bound to a live output. `Fill`'s
     *  equality is structurally exact (kind, plus the colour bitwise, plus
     *  shader POINTER identity — the same rule `bakedLiveShader` holds the
     *  live-material memo with) and resolving one is a pointer dereference,
     *  so "the value the recording was baked with" is well defined for this
     *  lane even though it is not a float. Default (None) when the node's
     *  fill is not bound — every site reads it through
     *  Instance::resolveBoundFill(), so the guard cannot drift between the
     *  volatility walk, the released scan and the paint probe. The sk_sp
     *  keeps a shader-kind value alive, so pointer identity can never be a
     *  freed allocation reused at the same address and comparing equal by
     *  accident. */
    Fill fill;
    /** The pan a recording's fill shader was baked with, when the fill
     *  material's tile offset is bound to live outputs: two floats resolved
     *  by pointer dereference, the world lane's and the fill lane's third
     *  sibling. All-zero when the node's fill material carries no top-level
     *  bound offset — every site reads it through
     *  Instance::resolvePatternOffset(), so the guard cannot drift between
     *  the volatility walk, the released scan and the paint probe. */
    std::array<float, 2> pattern{};
    /** onPath()'s resolved phase — WHERE ALONG the baseline the run sits
     *  this frame. The recording bakes the glyph positions that phase
     *  produced, so it is a content input exactly as a gate fraction is.
     *  Zero when the node carries no path baseline. */
    float pathAt = 0;
    bool operator==(const ContentScalars&) const = default;
  };
  ContentScalars bakedScalars;
  // The observed-stability RELEASE, and what scalarMemo alone cannot do.
  // scalarMemo keeps THIS node's own recording; the node still DECLARES
  // content volatility, so no ancestor may cache across it. Once the same
  // scalars resolve identically for kScalarSettleFrames consecutive paints,
  // the node stops declaring that volatility and ancestors can cache too.
  // The frame any value moves, the compare below fails, volatility
  // re-declares, and the ancestor's recording is refused before anything
  // stale replays.
  ContentScalars settledScalars;
  int settleFrames = 0;
  /** Consecutive stable paints before the release. Long enough that a value
   *  pausing between two keyframes does not count as settled. */
  static constexpr int kScalarSettleFrames = 8;
  // ---- Cache::Group, the SUBTREE value memo -------------------------------
  // scalarMemo asks "did this node's own animated scalars move". A group
  // asks the same question of a whole subtree, and for a different reason:
  // many small pieces forming one static assembly, each carrying a bound
  // opacity and scale for an entrance that runs and then holds. Their
  // pictures never change, so nothing about them is cacheable by the
  // volatility rule (the bindings stay connected forever) and everything
  // about them is cacheable for every frame the entrance is not running.
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
  bool groupWarned = false;  // the refusal is printed once per instance
  // The subtree's animated scalars as of the PREVIOUS frame, in tree order.
  // Compared by value rather than hashed. A hash collision here would not
  // show up as a glitch — it would silently replay a stale picture for as
  // long as the subtree stayed collided — and the float vector is small
  // enough, on the few nodes that opt in, that the trade is not worth taking.
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
  // frames on which the live material resolved to the SHADER ALREADY BAKED.
  // A material quantized to a step slower than the frame rate settles high;
  // one bound to a continuous output sits at zero and never promotes. The
  // rule is observed per frame rather than read off the material's declared
  // quantization, because a bound output that happens to be holding still
  // is just as cacheable, and a quantized one being driven faster than its
  // own step rate is not.
  float liveStableRate = 0;
  // True when this node OR anything below it blends against what is already
  // on the canvas (a non-srcOver blend, or a backdrop filter). Such a
  // subtree cannot be baked into a transparent layer and blitted back: the
  // blend would resolve against transparent black instead of the real
  // backdrop, and the pixels would differ. Computed by computeVolatile.
  bool subtreeReadsBackdrop = false;
  // The same question asked of the node's OWN paint alone, without its
  // children. The two halves are what the split bake turns on: a bake that
  // replaces only the node's own layer, with live children drawn over the
  // BLIT, fails when the OWN paint would resolve against transparent black
  // — and is unaffected by what the children do, because they composite
  // against the blitted destination exactly as they would have against a
  // freshly rasterized one. `subtreeReadsBackdrop` (own OR children) is
  // still the right question for whole-subtree promotion, where the
  // children ARE inside the bake.
  bool ownReadsBackdrop = false;
  // The split bake's other half: is the node's OWN content volatile, as
  // distinct from its subtree's? `subtreeVolatile && !ownContentVolatile`
  // is exactly "the children are what makes this node uncacheable", which
  // is the split bake's entire premise — a large static backdrop that is
  // re-rasterized every frame only so a small moving child can land on top
  // of it.
  bool ownContentVolatile = false;
  // ---- the split bake ------------------------------------------------------
  // The node's own paint, baked alone; the children are painted LIVE over
  // the blit. Deliberately NOT `textureImage`: a node can only ever hold one
  // of the two (the split needs subtreeVolatile, every other bake needs the
  // opposite), but sharing the slot would make each path's staleness rules
  // answer for the other's, and they are different rules.
  sk_sp<SkImage> ownImage;
  SkRect ownBakeRect = SkRect::MakeEmpty();  // device rect the bake covers
  float ownPaintMs = 0;                      // EMA of the own-paint cost
  uint8_t ownHotFrames = 0;
  // Consecutive frames on which the own bake had to be REMADE. A bake per
  // frame costs more than the live draw it replaced, so a node whose own
  // paint really is invalidated every frame must not hold its promotion on
  // the strength of a measurement taken while it was still cheap.
  uint8_t ownRebakes = 0;
  bool splitBake = false;  // sticky, like autoTexture
  // Staleness for `ownImage`, and the reason it cannot be `paintDirty`:
  // markPaintDirtyUp() propagates a descendant's patch to every ancestor,
  // which is correct for a RECORDING (it baked the child's draw calls) and
  // wrong for a split bake (the children were never in it). Set only on the
  // node whose own description changed.
  bool ownPaintDirty = true;
  bool hasPendingLiveFill = false;
  Fill pendingLiveFill;
  sk_sp<SkShader> bakedLiveShader;

  // Does this node's description carry a world-space material ANYWHERE a
  // paint consumes one — the fill slot, textFill, an effect's child
  // materials, a mask's coverage? Computed once at reconcile patch
  // (Reconcile.cpp), so the per-relayout syncLayoutRects walk and the
  // volatility walk read a bool instead of re-walking material trees.
  // Such a node's recording bakes its node→root matrix, so any movement of
  // the node OR of an ancestor stales its OWN paint. Note that the ancestor
  // case cannot be detected from this node's rect: instanceRect is
  // parent-relative, so an ancestor's move changes this node's world matrix
  // while its own rect compares equal.
  bool hasWorldSpaceMaterial = false;

  // The layout rect this node was last painted/recorded at. ensureLayout's
  // post-pass compares and invalidates: a SIZE change stales this node's own
  // recording (its content baked the old bounds — text lines, geometry
  // materials' uResolution, rrect geometry); a POSITION change stales the
  // parent's recording (which baked the old offset). Without this, cached
  // ancestors replay stale geometry after any relayout that wasn't caused by
  // a prop patch.
  SkRect lastLayoutRect = SkRect::MakeLTRB(-1, -1, -1, -1);

  // Resolved custom-outline cache: generators (blobs, rounded stars) can be
  // arbitrarily expensive — resolve once per (description, size). Desc pointer
  // identity keys invalidation: every patch swaps the description.
  SkPath outlineCache;
  SkSize outlineCacheSize = {-1.0f, -1.0f};
  const ElementNode* outlineCacheDesc = nullptr;

  // Stamped-brush bakes live with the NODE (handed to decorations via
  // PaintContext::stamps), so a brush value rebuilt every describe reuses
  // its art's bake instead of re-rastering it.
  StampCache stampCache;

  /** THE MOTION-PATH TABLE — `Element::travel()`'s measured curve.
   *
   *  Cached against the two INPUTS that determine it, never behind a dirty
   *  flag: the `Shape` VALUE (so a comparable scheme keeps its table across
   *  describes, and the raw-callable escape hatch re-measures exactly as it
   *  re-describes) and the SIZE it was resolved at, which is the parent's
   *  box, so a relayout re-measures. Comparing against what the table was
   *  built FROM cannot go stale the way a "have I run yet" flag can. */
  struct MotionCache {
    Shape shape;                // the value this table was built from
    SkSize size{-1.0f, -1.0f};  // …at this parent size
    std::vector<sk_sp<SkContourMeasure>> contours;
    std::vector<float> starts;  // cumulative length before each contour
    float total = 0;
    bool closed = false;  // every contour closed → t WRAPS
  };
  std::unique_ptr<MotionCache> motion;

  ~Instance();
  float resolveFloat(Instance::Slot slot, const Animatable<float>& v) const;
  /** The same resolution over an explicitly-held motion — the span
   *  endpoints, whose count the description decides. One body: a bound
   *  Output wins, then a running ramp, then the plain value. */
  float resolveFloatAt(const AnimatedFloat* anim,
                       const Animatable<float>& v) const;
  /** Resolve every stroke pass's claimed runs for this frame, with
   *  rest() complements applied. Empty when the node has no passes. */
  std::vector<std::vector<Span>> resolveSpans(const SkPath& outline) const;
  /** Resolve every mask gate's animatable floats for this frame, in the
   *  order maskAnims indexes them (and ContentScalars::gates stores
   *  them) — every value, live or settled, because the memo compares what
   *  the recording was BAKED with. Empty when the node carries no mask, or
   *  only shape/alpha gates, which have no numbers. */
  std::vector<float> resolveGateValues() const;
  /** The same resolution over the fx() TRACKS: every track's master
   *  progress this frame, in the order trackAnims indexes them (and
   *  ContentScalars::tracks stores them). Empty when the node carries no
   *  tracks. */
  std::vector<float> resolveTrackValues() const;
  /** The same resolution over onPath()'s `at` phase — one float, or zero
   *  when the node carries no path baseline. Every compare site reads this
   *  one body, so the volatility walk, the released scan and the paint
   *  probe cannot drift apart. */
  float resolvePathAt() const;
  /** `resolveGateValues`'s sibling for the fill lane: the value a bound
   *  `fill(&output)` resolves to this frame — `binding()->value()`, exactly
   *  the read paint() bakes into the recording — or a default (None) Fill
   *  when the node's fill is not bound.
   *
   *  ALL THREE compare sites call this one body: the volatility walk's
   *  release test, the per-draw released scan, and the paint-side probe.
   *  Re-spelling the resolution at any of them makes the compares disagree,
   *  and disagreement here is silent — a recording keeps replaying with a
   *  colour the binding has since moved off. */
  Fill resolveBoundFill() const;
  /** The third sibling: the tile pan a bound `Pattern::offset(&x, &y)`
   *  resolves to this frame — one pointer dereference per axis, through
   *  `Material::boundOffsetValue()` — or all-zero when the node's fill
   *  material carries no top-level bound offset. Same one-body rule as
   *  resolveBoundFill: the walk release, the per-draw scan and the paint
   *  probe all call THIS, so the three compares cannot drift apart. */
  std::array<float, 2> resolvePatternOffset() const;

  /** A change here stales every ancestor's recording too.
   *
   *  `ownPaint` says whether THIS node's own paint changed. It is false for
   *  exactly one caller — a child whose layout POSITION moved, which stales
   *  the parent's recording (it baked the old offset) and leaves the
   *  parent's own paint untouched. Only the split bake reads that
   *  distinction; passing true is always the safe answer. */
  void markPaintDirtyUp(bool ownPaint = true) {
    if (ownPaint) {
      ownPaintDirty = true;
      ownImage.reset();
    }
    for (Instance* i = this; i; i = i->parent) {
      if (i->paintDirty && i != this) break;  // ancestors already invalidated
      i->paintDirty = true;
      i->picture.reset();
      i->textureImage.reset();
    }
  }
};

inline bool childrenCarryYoga(const Instance& inst) {
  return inst.yoga != nullptr && inst.desc && !inst.desc->layout.positioned &&
         inst.desc->kind != Kind::Text;
}

// ---------------------------------------------------------------------------
// THE SLOT TABLE — a slot added to Instance::Slot is a BUILD FAILURE
//
// THE FAILURE THIS CLOSES IS THE SAME ONE ComposeInternal.h's FIELD PINS
// CLOSE, ONE LEVEL UP. Four functions consume `Instance::Slot` —
// `collectGroupScalars` and `computeVolatile` (Paint.cpp),
// `applyMountTransitions` and `applyTransitions` (Transitions.cpp). Were
// each of them to enumerate the slots by hand, a slot appended to the enum
// would compile perfectly while being absent from any of them, and every
// one of those absences is silent:
//
//   - absent from `applyTransitions`  → `animate()` on the property never
//     ramps; it snaps, and looks like a missing transition spec.
//   - absent from `applyMountTransitions` → `animate(from().to())` plays no
//     entrance; the node just appears at its settled value.
//   - absent from `computeVolatile` → the property is not volatility, so an
//     ancestor caches across it and the motion FREEZES in a replayed
//     picture. Invisible in any still.
//   - absent from `collectGroupScalars` → a `Cache::Group` holds a bake
//     while the property moves, i.e. blits last frame's pixels.
//
// No test catches any of these: nothing errors, and a still frame of the
// affected node looks exactly right.
//
// THE MECHANISM: one row per enum value, INDEX-ALIGNED, under asserts that
// make a missing row, a duplicated row and a misordered row all fail to
// compile. What the rows carry is the only thing the four consumers ever
// want from a slot —
//
//   `of`      the description's Animatable for it (null when this node does
//             not carry the block that holds it), and
//   `role`    which of the three questions it answers.
//
// The three roles are not a taxonomy invented for the table; they are the
// split `computeVolatile` has to make anyway, which sorts its slots into
// opacity (applied by paint()'s saveLayer), geometric (applied by paint()'s
// matrix, so it moves the device rect and refuses a device-pinned bake) and
// content (rebuilds what the node RECORDS). The other three consumers each
// read that split or ignore it; none needs a fourth thing, which is why one
// table serves all four.
//
// WHY A TABLE HERE AND A NAMED SUBTRACTION IN computeVolatile. That
// function's content terms are a heterogeneous bag of booleans (a bound
// fill, an animated image frame, a live effect) with no enum behind them,
// so the only thing that can hold them together is a single named
// expression every consumer subtracts from. These twelve slots are an
// ENUMERATED AXIS instead, and an enumerated axis can be counted by the
// compiler.

/** Which of the three questions a slot answers — the axis `computeVolatile`
 *  already split on, named so the other three consumers can read it. */
enum class SlotRole : uint8_t {
  /** Applied by paint()'s saveLayer, OUTSIDE the node's content: a fading
   *  node replays its picture, and does not move its device rect. */
  Opacity,
  /** Applied by paint()'s matrix. Moves the device rect, so a device-space
   *  bake is refused while it runs (`Instance::transformLive`). */
  Geometric,
  /** Rebuilds what the node RECORDS, so its own picture is invalidated. */
  Content,
  /** No `Animatable<float>` in the description AT ALL, so the table cannot
   *  reach it and every consumer keeps its own handling. Costs a written
   *  reason in `bespoke`, which the assert below enforces. */
  Bespoke,
};

/** One row per `Instance::Slot`, index-aligned with the enum. */
struct SlotSpec {
  Instance::Slot slot;
  SlotRole role;
  /** The description's animatable for this slot on this node, or nullptr
   *  when the node does not carry the block that holds it (a node with no
   *  `travel()` has no `t`). Null for a Bespoke row — call it through
   *  slotValueOf(), which answers nullptr for those. */
  const Animatable<float>* (*of)(const ElementNode&);
  /** The standing endpoint a PATCH ramps from or to when `of` answers
   *  nullptr on one side of the diff — a node that GAINS or LOSES the block
   *  has no previous/next value, so the field's OWN DEFAULT is the
   *  endpoint. Pinned to the real default by
   *  ComposeSlotPins.EverySlotRowReachesItsOwnFieldAtItsStandingDefault;
   *  unused by a slot whose `of` never answers nullptr. */
  float standing;
  /** Why this slot is out of the table's reach. Non-null IFF the role is
   *  Bespoke — asserted below, so the escape hatch cannot be taken blank. */
  const char* bespoke;
};

inline constexpr SlotSpec kSlotSpecs[] = {
    {Instance::kOpacity, SlotRole::Opacity,
     [](const ElementNode& n) { return &n.paint.opacity; }, 1.0f, nullptr},
    {Instance::kTx, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.translateX; }, 0.0f, nullptr},
    {Instance::kTy, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.translateY; }, 0.0f, nullptr},
    {Instance::kRotate, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.rotate; }, 0.0f, nullptr},
    {Instance::kScale, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.scale; }, 1.0f, nullptr},
    // kFillLerp is a 0→1 PROGRESS the composer synthesizes for a
    // colour→colour `animate()`; the description holds an
    // `Animatable<Fill>` and no float anywhere, so there is nothing for
    // `of` to return. Its four call sites are hand-written beside the loop
    // that walks this table, each labelled "the kFillLerp row".
    {Instance::kFillLerp, SlotRole::Bespoke, nullptr, 0.0f,
     "a progress scalar over paint.fill's Transitioned<Fill> — there is no "
     "Animatable<float> in the description to point at"},
    {Instance::kSkewX, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.skewX; }, 0.0f, nullptr},
    {Instance::kSkewY, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.skewY; }, 0.0f, nullptr},
    {Instance::kScaleX, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.scaleX; }, 1.0f, nullptr},
    {Instance::kScaleY, SlotRole::Geometric,
     [](const ElementNode& n) { return &n.paint.scaleY; }, 1.0f, nullptr},
    // travel(): the `t` lane moves the node exactly as tx/ty do, so it is
    // the GEOMETRIC half and a device-space bake is refused while it runs.
    {Instance::kMotionT, SlotRole::Geometric,
     [](const ElementNode& n) -> const Animatable<float>* {
       return n.motionData ? &n.motionData->t : nullptr;
     },
     0.0f, nullptr},
    // onPath(): `at` is WHERE ALONG the baseline the run sits, so moving it
    // re-places every glyph INSIDE the node's own box and leaves the box
    // where it was. That is the CONTENT half, not the geometric one — the
    // recording is rebuilt, the device rect is not — and the resolved value
    // joins ContentScalars so a marquee that stops running releases like any
    // other settled scalar.
    {Instance::kTextPathAt, SlotRole::Content,
     [](const ElementNode& n) -> const Animatable<float>* {
       return n.textData && n.textData->onPath ? &n.textData->onPath->at
                                               : nullptr;
     },
     0.0f, nullptr},
};

static_assert(std::size(kSlotSpecs) == (size_t)Instance::kSlots,
              "every Instance::Slot needs a row here — a slot added without "
              "one is SILENTLY absent from collectGroupScalars, "
              "computeVolatile, applyMountTransitions and applyTransitions, "
              "and every one of those absences is invisible (no error, no "
              "failing test): see the comment above this table");

/** Index alignment and the Bespoke invariant, checked at compile time.
 *  Stronger than the size assert alone: it also catches a row inserted in
 *  the wrong place, a slot named twice, and a row with no accessor that did
 *  not declare itself out of reach. */
constexpr bool slotTableWellFormed() {
  for (size_t i = 0; i < std::size(kSlotSpecs); ++i) {
    if (kSlotSpecs[i].slot != (Instance::Slot)i)
      return false;  // rows must be index-aligned with the enum
    const bool bespoke = kSlotSpecs[i].role == SlotRole::Bespoke;
    if (bespoke != (kSlotSpecs[i].of == nullptr))
      return false;  // no accessor ⇔ declared out of the table's reach
    if (bespoke != (kSlotSpecs[i].bespoke != nullptr))
      return false;  // …and the declaration carries a written reason
  }
  return true;
}
static_assert(slotTableWellFormed(),
              "kSlotSpecs must be index-aligned with Instance::Slot, and a "
              "row with no accessor must declare SlotRole::Bespoke with a "
              "written reason");

/** The row's animatable on this node, or nullptr. A Bespoke row always
 *  answers nullptr, so a consumer that walks the table without special-casing
 *  one is INERT for it rather than dereferencing a null function pointer. */
inline const Animatable<float>* slotValueOf(const SlotSpec& spec,
                                            const ElementNode& node) {
  return spec.of ? spec.of(node) : nullptr;
}

// ---- cross-TU paint/shape helpers -----------------------------------------

/** The matrix's true maximum geometric scale over `local` — how many device
 *  pixels one local unit can span, whatever the rotation.
 *
 *  NOT max(|getScaleX()|, |getScaleY()|). Those are the matrix DIAGONAL, and
 *  a quarter turn moves the whole scale into the SKEW terms: at ±90° the
 *  diagonal is exactly (0, 0), because Skia's setRotate snaps cos(90°) to
 *  zero. A raster-target decision reading the diagonal therefore sees
 *  "scale 0" for a quarter-turned node, clamps to the caller's floor, and
 *  bakes at a fraction of device resolution to be linearly upscaled by the
 *  blit — a visible softening of everything the node contains. Singular
 *  values instead; a pure rotation reports 1.
 *
 *  Under PERSPECTIVE (getMinMaxScales refuses) the scale is
 *  position-dependent, so there is no one number for the whole plane, and
 *  falling back to the diagonal is wrong twice over: a rotation empties it
 *  exactly as above, and it reads no position at all while a projected
 *  quad's near edge magnifies well past it. The honest local answer is the
 *  JACOBIAN of the projective map, evaluated where the node actually is:
 *  the largest singular value of
 *  J(p) = (1/w)·[[a−gX, b−hX], [d−gY, e−hY]], taken at the center and four
 *  corners of `local` and maxed — for a plane the extremum over a convex
 *  quad sits at a corner (the one nearest the horizon), and max-over-
 *  samples errs in the CONSERVATIVE direction for every consumer: an
 *  overestimate steps a bake finer (memory, never wrong pixels), an
 *  underestimate ships a stale, blurry bake. Samples at or behind the
 *  horizon (w ≤ 0) have no finite local scale and are skipped; if every
 *  sample is degenerate the diagonal stands, bounded by the callers'
 *  clamps. */
inline float maxScaleOf(const SkMatrix& m, const SkRect& local) {
  SkScalar s[2];
  if (m.getMinMaxScales(s) && s[1] > 0) return s[1];
  if (m.hasPerspective()) {
    const auto sigmaMaxAt = [&m](SkPoint pt) -> float {
      const float w = m.getPerspX() * pt.x() + m.getPerspY() * pt.y() +
                      m.get(SkMatrix::kMPersp2);
      if (!std::isfinite(w) || w <= 1e-8f)
        return -1.0f;  // at/behind the horizon: no finite local scale
      const SkPoint q = m.mapPoint(pt);
      const float inv = 1.0f / w;
      const float j00 = (m.getScaleX() - m.getPerspX() * q.x()) * inv;
      const float j01 = (m.getSkewX() - m.getPerspY() * q.x()) * inv;
      const float j10 = (m.getSkewY() - m.getPerspX() * q.y()) * inv;
      const float j11 = (m.getScaleY() - m.getPerspY() * q.y()) * inv;
      // Largest singular value of the 2×2, closed form.
      const float e = j00 + j11, f = j00 - j11;
      const float g = j10 + j01, h = j10 - j01;
      const float sig =
          0.5f * (std::sqrt(e * e + h * h) + std::sqrt(f * f + g * g));
      return std::isfinite(sig) ? sig : -1.0f;
    };
    const SkPoint samples[5] = {{local.centerX(), local.centerY()},
                                {local.left(), local.top()},
                                {local.right(), local.top()},
                                {local.right(), local.bottom()},
                                {local.left(), local.bottom()}};
    float best = -1.0f;
    for (const SkPoint& pt : samples) best = std::max(best, sigmaMaxAt(pt));
    if (best > 0) return best;
  }
  // Degenerate (a zero matrix; a horizon through every sample): the
  // diagonal is all there is, and the callers' clamps bound it.
  return std::max(std::abs(m.getScaleX()), std::abs(m.getScaleY()));
}

/** The paint-transform pivot: fractional by default, node-local px under
 *  transformOriginPx(). One definition for paint(), recordBounds(), and
 *  the hit-test inverse. */
inline SkPoint resolveOrigin(const PaintProps& p, float w, float h) {
  return p.originPx ? SkPoint{p.originX, p.originY}
                    : SkPoint{w * p.originX, h * p.originY};
}

inline SkRRect cornersRRect(const SkRect& bounds, const Corners& c) {
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

}  // namespace sigil::compose::detail

namespace sigil::compose {

struct Composer::Impl {
  motion::Ticker& ticker;
  sigil::weave::FontContext& fonts;
  const motion::FrameClock* clock = nullptr;

  SkSize size = SkSize::MakeEmpty();
  std::unique_ptr<detail::Instance> root;
  YGConfigRef yogaConfig = nullptr;
  bool needsLayout = true;
  bool contentDirty = true;
  std::unordered_map<std::string, detail::Instance*> byKey;
  // Slots get their OWN index. They live in byKey too (so bounds() and
  // hitTest() still answer for a slot's name), but a slot's CONTENT may
  // legitimately carry a root .key() with the same name — and a child is
  // indexed after its parent, so in a single shared map the content would
  // overwrite the slot's entry and every later renderSlot() would silently
  // find the wrong instance. Two namespaces, no collision.
  std::unordered_map<std::string, detail::Instance*> bySlot;
  // The EDGE STORE, rebuilt with the key index each render: routed nodes
  // (connector()/rail()) as a flat list in tree order, plus the back-index
  // anchor-key → routes-anchored-there. The derive pass iterates these flat
  // lists instead of recursing the whole tree, and routesAt() answers graph
  // queries ("which edges touch this node") in O(routes-at-node).
  std::vector<detail::Instance*> routedInstances;
  std::vector<detail::Instance*> flowInstances;  // flowAround() text nodes
  std::unordered_map<std::string, std::vector<detail::Instance*>>
      routesByAnchor;
  bool volatileDirty = true;  // recompute needed (render or animation)
  bool tickerWasActive = false;
  // Instances whose scalar volatility is RELEASED (settled bound gates,
  // glyph progress and the other memoized scalar lanes). Rebuilt by every
  // computeVolatile walk, and scanned once per draw so an EXTERNALLY-driven
  // output that starts moving again re-declares volatility the same frame:
  // the walk itself only re-runs on reconcile or while the ticker is
  // active, so without this scan a released node driven from outside the
  // library would never notice it had resumed. Guarded by !volatileDirty —
  // a pending recompute means the tree changed and these pointers may be
  // stale.
  std::vector<detail::Instance*> releasedScalars;
  void scanReleasedScalars();  // defined in Paint.cpp beside the memos
  // Recomputed with the key index, so unmounting the last derived or pinned
  // node clears them rather than latching them on forever.
  bool hasDerived = false;  // any flowAround/connector/rail in the tree
  bool hasCustomLayout = false;
  bool hasCenterPins = false;  // any centerAt() in the tree
  bool liveOnly = false;       // snapshot(): skip per-node caches
  Effect view;  // output view transform (null filter = pass-through)
  // What the AUTHOR declared their colour values to be. Read by
  // declaredInputSpace() and by nothing else: compositing happens in
  // encoded sRGB regardless, with no linear stage and no conversion, so
  // declaring a space only changes what the mismatch warning in
  // declareInputSpace() says at declaration time.
  InputSpace inputSpace = InputSpace::EncodedSRGB;
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
  bool autoPromote = true;  // Composer::setAutoTexturePromotion (the INTENT)
  bool promotionExplicit = false;  // did the host call the setter?
  // The value paint() actually reads, recomputed each draw(). Differs from
  // `autoPromote` only under the backend-aware default: automatic promotion
  // is OFF on a Graphite/GPU surface unless the host asked for it
  // explicitly. The cost model that drives promotion — the millisecond
  // threshold, the stability average, the temporal gate — times how long a
  // node takes to RECORD its ops, which stands in for raster cost and says
  // nothing about GPU cost. On GPU the threshold is rarely crossed, and
  // when it is, the bake plus its synchronization and upload costs more
  // than the recording it replaces. Re-enabling it there needs a cost model
  // built on GPU timestamps. The global switch still overrides in both
  // directions.
  bool autoPromoteEffective = true;
  // Promoted bakes are pixels, and a dense scene can carry many
  // full-canvas nodes at several megabytes each. A budget, carried from the
  // previous frame (paint order is stable, so the previous frame's total is
  // the right question to ask before adding one more), keeps an automatic
  // win from becoming an automatic out-of-memory.
  size_t promotedBytes = 0;      // accumulated during the current paint
  size_t promotedBytesLast = 0;  // what the previous frame ended up holding
  // >0 while painting INTO an SkPicture. Device-space bakes are pinned to
  // a device rect and must not be recorded into a picture that can replay
  // under a different matrix.
  int recordingDepth = 0;
  // The node→root matrix accumulated by paint()'s own recursion — the same
  // walk Query.cpp inverts for hit testing, run forwards. Saved and
  // restored around each paint() frame (RAII, because paint() returns from
  // several places); identity between draws. PaintContext::toRoot is read
  // from here, so every consumer of the material seam sees the SAME matrix
  // the hit test inverts, and a world-space field lands where the hit test
  // says the node is.
  SkMatrix curToRoot = SkMatrix::I();
  // The root's LAID-OUT size (canvas px) — differs from `size` under an
  // intrinsic root (snapshot()). Written by paint() at the root frame;
  // PaintContext::rootSize is read from here.
  SkSize rootLayoutSize = SkSize::MakeEmpty();
  static constexpr size_t kPromotedBudget = 192u * 1024 * 1024;
  std::vector<Composer::NodeCost> profileRows;
  double profChildMs = 0;
  int profDepth = 0;
  // render()/renderSlot() phase time accumulated since the previous draw();
  // draw() publishes it as stats.reconcileMs and zeroes the accumulator.
  double reconcileAccumMs = 0;

  Impl(motion::Ticker& t, sigil::weave::FontContext& f) : ticker(t), fonts(f) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return clock ? clock->elapsed() : 0.0; }

  // ---- reconcile (Reconcile.cpp) ----
  std::unique_ptr<detail::Instance> mount(
      const std::shared_ptr<detail::ElementNode>& node,
      detail::Instance* parent);
  void patch(detail::Instance& inst, std::shared_ptr<detail::ElementNode> node);
  void patchChildren(detail::Instance& inst,
                     const std::vector<Element>& newChildren);
  void applyLayoutProps(detail::Instance& inst);
  /** Builds the instance's Paragraph from whichever content form its
   *  description carries — plain utf8, `rich()` runs, or a copy of a
   *  supplied Paragraph — and then applies the span restyles in
   *  declaration order. @p lines is the geometry a previous layout
   *  produced, which is what a `sel::line` restyle addresses; empty leaves
   *  those selectors unresolved. @p columns carries the same geometry for a
   *  vertical passage, where a line IS a column. */
  void materializeText(
      detail::Instance& inst,
      std::span<const sigil::weave::LineMetrics> lines = {},
      std::span<const sigil::weave::ColumnMetrics> columns = {});
  /** The options a text node actually lays out under: the full-control
   *  overload's value where it has one, with every field a fluent setter
   *  named written over it. */
  sigil::weave::ParagraphLayoutOptions textLayoutOptions(
      const detail::Instance& inst) const;
  void applyTransitions(detail::Instance& inst, const detail::ElementNode& prev,
                        const detail::ElementNode& next);
  void applyMountTransitions(detail::Instance& inst,
                             const detail::ElementNode& node);
  std::shared_ptr<detail::ElementNode> resolveMemo(
      detail::Instance* existing,
      const std::shared_ptr<detail::ElementNode>& node, bool& described);
  void rebuildKeyIndex();
  void indexKeys(detail::Instance& inst);

  // ---- volatility & caching (Paint.cpp) ----
  /** @p movingAbove: a bound or transitioning transform is connected on
   *  some ancestor. A node carrying a world-space material below one has
   *  its node→root matrix changing off the describe clock, which is CONTENT
   *  volatility for that node — and it joins the memoized scalar lane,
   *  because that matrix is six floats, so the recording survives between
   *  ticks and the flag releases when the motion settles. Threaded down the
   *  existing recursion; everything else ignores it. */
  bool computeVolatile(detail::Instance& inst, bool movingAbove = false);
  /** The node→root matrix, recomputed OUTSIDE paint by walking the ancestor
   *  chain root-down through the same ops paint() accumulates —
   *  translate(rect), then NodeTransform::matrix. The result must be
   *  BIT-IDENTICAL to the paint-side accumulation: the settle compare reads
   *  an ulp of drift as motion and never releases. */
  SkMatrix worldMatrixOf(detail::Instance& inst);
  /** That matrix's affine six for the ContentScalars lane — all-zero unless
   *  the instance carries a world-space material (both sites that fill the
   *  member apply the same guard). */
  std::array<float, 6> worldScalarsOf(detail::Instance& inst);
  // Scratch for the subtree value memo, swapped with the group root's
  // `groupPrev` each frame so a settled group allocates nothing at all.
  std::vector<float> groupScratch;

  // ---- layout (Layout.cpp) ----
  bool applyCustomLayouts(detail::Instance& inst);
  bool applyCenterPins(detail::Instance& inst);
  void ensureLayout();
  /** @p movedAbove: some ancestor's layout rect changed this pass. A node
   *  carrying a world-space material below any moved rect marks its OWN
   *  paint dirty — its recording baked the node→root matrix, and that
   *  matrix moved with the ancestor even though this node's
   *  parent-relative rect did not. */
  void syncLayoutRects(detail::Instance& inst, bool movedAbove = false);
  /** Lays the node's text out inside @p constraint px across and
   *  @p downConstraint px down. A horizontal passage reads the first as its
   *  measure and ignores the second; a vertical one reads the first as
   *  where its rightmost column stands and the second as how far a column
   *  may run before the next one starts. */
  void layoutText(detail::Instance& inst, float constraint,
                  float downConstraint = 1.0e6f);
  SkRect instanceRect(const detail::Instance& inst) const;
  SkRect positionedRect(const detail::Instance& inst) const;
  SkRect absoluteRect(const detail::Instance& inst) const;

  // ---- derive (Derive.cpp) ----
  /** One pass over the flat flow/route lists (the edge store) — no tree
   *  recursion. Returns true when a text exclusion changed (second layout
   *  pass needed). */
  bool resolveDerived();
  bool deriveFlow(detail::Instance& inst);
  void deriveRoute(detail::Instance& inst);

  // ---- the node's paint transform, resolved once (Paint.cpp) ----
  /** Every animated number in paint()'s matrix stack, for ONE frame.
   *
   *  One resolver, three consumers — paint()'s matrix, recordBounds()'s
   *  child union and hitInstance()'s inverse — because the three must
   *  describe the same matrix or a node draws where it cannot be hit. And
   *  one matrix PRODUCER, `matrix()` below, for the same reason one step
   *  later: turning these lanes into a matrix is itself a place three
   *  hand-written copies could disagree. */
  struct NodeTransform {
    float tx = 0, ty = 0, rot = 0, scl = 1, sx = 1, sy = 1, skx = 0, sky = 0;
    /** Does anything past the translate need the origin pivot at all?
     *
     *  THE ONE DEFINITION, and every consumer asks it rather than writing
     *  the condition out. A consumer that spells its own and omits a lane
     *  silently drops that lane's effect: a per-axis-scaled child would
     *  contribute UNSCALED bounds to its parent's layers and bakes, which
     *  truncates the overflow with no diagnostic. A lane added to this
     *  struct belongs in here. */
    bool pivoted() const {
      return rot != 0 || scl != 1 || sx != 1 || sy != 1 || skx != 0 || sky != 0;
    }
    /** The matrix these lanes describe, prepended with `anchor` (the
     *  layout offset — pass {0, 0} for node-local): the translate lanes,
     *  then — gated on pivoted(), NOT a copy of it — the origin-pivoted
     *  rotate → scale → skew stack. THE ONE PRODUCER for recordBounds()'s
     *  child union and hitInstance()'s inverse. The anchor folds into the
     *  FIRST translate rather than being post-concatenated, because the two
     *  associate their float multiplies differently and recordBounds()'s
     *  results must stay bitwise stable. */
    SkMatrix matrix(SkPoint anchor, const detail::PaintProps& p, float w,
                    float h) const {
      SkMatrix m = SkMatrix::Translate(anchor.x() + tx, anchor.y() + ty);
      if (pivoted()) {
        const SkPoint origin = detail::resolveOrigin(p, w, h);
        m.preTranslate(origin.x(), origin.y());
        if (rot != 0) m.preRotate(rot);
        if (scl != 1 || sx != 1 || sy != 1) m.preScale(scl * sx, scl * sy);
        if (skx != 0 || sky != 0)
          m.preSkew(std::tan(skx * 0.017453293f), std::tan(sky * 0.017453293f));
        m.preTranslate(-origin.x(), -origin.y());
      }
      return m;
    }
    /** paint()'s consumer: the SAME stack, applied as the canvas's own
     *  elementary ops rather than one concat of matrix()'s product.
     *
     *  NOT a convenience — a BYTE-EXACTNESS requirement. Composing the
     *  stack into one SkMatrix and concat()ing that associates the float
     *  multiplies differently than sequential canvas ops do, so the CTM
     *  lands a few ulps away, and antialiased coverage along every edge
     *  changes with it. Replacing this with a single concat of matrix()
     *  therefore moves pixels across the whole scene. The op list below and
     *  matrix()'s are THE SAME LIST in the same order; a lane added to the
     *  struct goes in both (the fieldPin below counts it). */
    void concatTo(SkCanvas& canvas, const detail::PaintProps& p, float w,
                  float h) const {
      if (tx != 0 || ty != 0) canvas.translate(tx, ty);
      if (pivoted()) {
        const SkPoint origin = detail::resolveOrigin(p, w, h);
        canvas.translate(origin.x(), origin.y());
        if (rot != 0) canvas.rotate(rot);
        if (scl != 1 || sx != 1 || sy != 1) canvas.scale(scl * sx, scl * sy);
        if (skx != 0 || sky != 0)
          canvas.skew(std::tan(skx * 0.017453293f),
                      std::tan(sky * 0.017453293f));
        canvas.translate(-origin.x(), -origin.y());
      }
    }
    /** FIELD PIN (see ComposeInternal.h's FIELD PINS block). `pivoted()`
     *  is a hand-written exhaustive list over these members, exactly like
     *  a comparator, and fails the same way: silently, by not noticing. */
    static void fieldPin(NodeTransform& v) {
      auto& [tx, ty, rot, scl, sx, sy, skx, sky] = v;
      static_assert(std::tuple_size_v<decltype(std::tie(tx, ty, rot, scl, sx,
                                                        sy, skx, sky))> == 8,
                    "NodeTransform gained or lost a lane — put it in "
                    "pivoted() above (unless it is a pure translate), in "
                    "matrix()'s build, and in transformOf()'s resolve, "
                    "then bump this count.");
    }
  };
  NodeTransform transformOf(detail::Instance& inst);
  /** Where on its motion path this node sits, in its PARENT's space, and
   *  the auto-orient angle in degrees. Nullopt when no path is engaged
   *  (absent, empty, or resolving to no measurable length) — the
   *  translate lanes then stand. Rebuilds the instance's arc-length table
   *  when the Shape value or the parent size no longer matches. */
  std::optional<std::pair<SkPoint, float>> motionPathSample(
      detail::Instance& inst, const SkSize& frame);

  // ---- paint (Paint.cpp) ----
  float hostScale = 1.0f;  // device px per layout px at draw() entry
  void paint(detail::Instance& inst, SkCanvas& canvas);
  /** Breaks the run across the baseline's contours through SigilWeave's
   *  contour-interval geometry, and caches the result on the instance. */
  void ensurePathLayout(detail::Instance& inst, const TextPath& spec,
                        SkSize size);
  /** THE ONE GLYPH DRAW for text that is not simply resting on its own
   *  straight baseline. The rest pose comes from the baseline — level on a
   *  plain run, on the curve and turned to it on a path run — and every
   *  fx() track's deviation applies ON TOP OF IT, in that pose's own frame.
   *  `onPath` is null for text with no baseline path. */
  void paintTextFx(detail::Instance& inst, SkCanvas& canvas,
                   const sigil::weave::PaintStyle* override,
                   const TextPath* onPath, SkSize size);
  /** THE SCHEDULE ONE TRACK IS RUNNING, resolved against the layout the
   *  last draw() produced — the read-back behind Composer::beatsOf. Rects
   *  come out in the NODE's own space; the caller offsets them into the
   *  composer's, as the bounds query does. */
  std::vector<Beat> beatsOfTrack(detail::Instance& inst, size_t trackIndex);
  /** WHERE EACH mark() ANCHORS, refilling `textMarkRects` from the layout
   *  just produced: one rect per anchor, the union of the advance boxes of
   *  the glyphs its selector addressed. Runs during LAYOUT, off the flow
   *  layout, which is why a path run's marks are refused rather than
   *  answered from the straight baseline it does not use. */
  void resolveTextMarks(detail::Instance& inst);
  /** The glyph-paint override textFill()/textStroke() ask for, or nullopt
   *  when the node asks for neither. ONE body, called by the resting draw
   *  and by the fx() draw — a letter in flight is painted exactly as a
   *  resting one is. */
  std::optional<sigil::weave::PaintStyle> metricTextStyle(
      detail::Instance& inst, const PaintContext& paintCtx);
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
    All,           ///< the whole node, unchanged
    OwnOnly,       ///< the prefix: no children, no foregrounds
    ChildrenOnly,  ///< the children and the foregrounds over them
  };
  void paintContent(detail::Instance& inst, SkCanvas& canvas,
                    float contentScale,
                    SkBlendMode leafBlend = SkBlendMode::kSrcOver,
                    float leafOpacity = 1.0f, Phase phase = Phase::All);
  const SkPath& resolveOutline(detail::Instance& inst, SkSize size) const;
  /** What the node paints BY ITSELF, in its own local space: its box grown
   *  by every decoration's declared bleed and any routed path, and NOTHING
   *  from its children. The split bake sizes its layer with this — and the
   *  independence from the children is the load-bearing part, not an
   *  optimisation: `recordBounds` unions the children in, so it changes
   *  every frame a child moves, and a bake rect that changes every frame is
   *  a bake remade every frame. */
  SkRect ownPaintBounds(detail::Instance& inst);
  SkRect recordBounds(detail::Instance& inst);

  // ---- hit testing / queries (Query.cpp) ----
  bool shapeContains(detail::Instance& inst, SkPoint local, SkSize size) const;
  std::optional<std::string> hitInstance(detail::Instance& inst,
                                         SkPoint parentPt,
                                         const std::string* inheritedKey);
};

}  // namespace sigil::compose
