#pragma once

/** @file
 * Internal to the kernel — the RETAINED runtime node: one Instance per
 * mounted element, with its text state, its transition slots, its derive
 * results, its selection cache and its caching tiers, and the animated
 * float a slot transitions through.
 */

#include <sigilcore/cache/Settle.h>
#include <sigilcore/reconcile/Node.h>
#include <sigilmotion/values/Lanes.h>
#include <yoga/Yoga.h>

#include "ComposeInternal.h"

// markPaintDirtyUp() calls sk_sp::reset() inline, so the ref-counted payload
// types must be complete here (not merely forward-declared).
#include <include/core/SkCanvas.h>  // NodeTransform::concatTo's elementary ops
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRRect.h>
#include <include/core/SkRect.h>
#include <include/core/SkTypeface.h>
#include <sigilgeometry/path/Pose.h>
#include <sigilweave/fonts/FontContext.h>

#include <algorithm>
#include <cmath>
#include <iterator>  // std::size, for the kSlotSpecs asserts
#include <map>
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
 *  source of truth while a motion is connected. SigilMotion's, because a
 *  moving animatable is the motion library's business. */
using AnimatedFloat = motion::AnimatedFloat;

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

/** THE TEXT ENGINE'S STATE ON A NODE — what dressed type keeps between
 *  frames beyond the paragraph and the flow layout the kernel measures by.
 *  Held by the instance through one pointer and created the first time an
 *  engine operation touches the node, so text drawn at rest costs none of
 *  it. Written and read by the text painter a description carries; the
 *  kernel itself reads one field, the folded axis tracks, because they are
 *  tracks the painter draws and volatility counts. */
struct TextState {
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
  // spanStyle() restyles that differ from the text they cover ONLY in
  // advance-invariant variable-font axes, carried as tracks instead of
  // re-shaping: the paragraph keeps the glyphs and pen positions it shaped,
  // and the coordinate reaches the glyphs at draw time exactly as a driven
  // axis does. Decided against the materialized paragraph, which is why the
  // list lives here and not on the description, and rebuilt with it. Drawn
  // AFTER the description's own tracks, so the painter's selection and
  // track lists are the description's tracks followed by these.
  std::vector<Track> spanAxisTracks;
};

/** The retained node. The tree skeleton — `parent`, `desc` (the resolved,
 *  post-memo description), `memoShell` (the memo element, if any) and
 *  `children` — is SigilCore's Node, which is what the reconciler walks;
 *  everything below it is what this kernel retains per node. */
// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Instance : core::Node<Instance, std::shared_ptr<ElementNode>> {
  Composer::Impl* owner = nullptr;
  YGNodeRef yoga = nullptr;
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
  // thread(): the word this frame's fill begins at — 0 for the head of a
  // chain, and whatever the frame before it left unplaced for every other.
  uint32_t threadCursor = 0;
  // …and the LINE this frame's first line is, counted from the story's
  // start. A story numbers its own lines: sel::line(40) is the fortieth
  // line of the story wherever it landed, so a chain that reflows moves
  // the selection with the text instead of addressing a different line in
  // every frame. 0 for the head, and for every text that is not a frame.
  uint32_t threadLineOffset = 0;
  // …and the MEASURE THE NEXT FRAME SETS IN, which the widow rule needs
  // because the lines it counts are the ones this frame will not hold.
  // Only the chain knows it; 0 says it is not known yet, which is the
  // first pass over a chain nobody has laid out.
  float threadNextMeasure = 0;
  // …and the lines the WHOLE chain placed, written to every frame once the
  // walk knows it. A cascade numbered over the story needs the story's own
  // count, and no single fill has it.
  uint32_t threadStoryLines = 0;
  uint32_t contentRev = 0;     // bumped on text/exclusion change
  uint32_t measuredRev = ~0u;  // rev the cached measurement belongs to
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
  // annotate(): every reading, laid out where the base's units put it, in
  // this node's own space. Each is a small paragraph of its own with its own
  // placement, so the kernel draws it exactly as it draws the base.
  struct PlacedAnnotation {
    std::shared_ptr<sigil::weave::Paragraph> paragraph;
    sigil::weave::ParagraphLayout layout;
  };
  std::vector<PlacedAnnotation> textAnnotations;
  // rich().add(text, styleName): each named run and the text it occupies, in
  // declaration order — what sel::style resolves against. Cleared and
  // rebuilt with the paragraph, so the names a node answers for are exactly
  // the ones its current content declares.
  std::vector<detail::NamedRun> textNamedRuns;
  // The engine's state (TextState) — null until dressed type first asks
  // for it. Read through textStateOf().
  std::unique_ptr<TextState> textState;

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
  // …and does what this node draws LAND SOMEWHERE ELSE next frame?
  // `transformLive` is the node's OWN declared motion; this is that, OR any
  // ancestor's, OR — for text — a live fx() track whose effect moves glyphs
  // off their pen positions. Text asks it: a run whose device placement
  // creeps needs its glyph origins on the subpixel grid, and a figure
  // rotating above the text, or a slide dragging every letter sideways,
  // makes it creep exactly as a marquee phase does. Every term is read off
  // a declaration, so a run that stops keeps the placement it was moving
  // with instead of taking one last shift as it settles.
  bool placementUnderMotion = false;
  // …and is the COMPOSER still working on this passage? A text told its
  // input is moving (Element::live) has its break decisions kept and
  // reused, and a frame that answered every block from that store did no
  // work at all: the passage is set exactly as the frame before it. So the
  // leaf reports what its last layout cost — how many blocks came from the
  // store, how many the budget forced to the greedy breaker — and this is
  // the one bit the caching proof reads off that report: a live passage
  // whose layout is not yet stable can change without any number this node
  // carries changing, which is what "opaque to a value memo" means.
  // Written in layoutText, beside the layout it describes.
  bool textComposing = false;
  /// What the last layout of this leaf cost, for `Composer::settling`.
  int textReusedBlocks = 0, textDegradedBlocks = 0;
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
  // The three sides of that release — the count kept where paint() runs,
  // the release the volatility walk performs, and the per-draw scan over
  // what it released — are SigilCoreCache's protocol, held here over this
  // library's own scalars.
  core::Settle<ContentScalars> settle;
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
  // Element::boundary(Boundary::Glyphs): the union of this text's glyph
  // outlines at the placement its layout produced, resolved once per
  // layout because a decoration asked for it and never otherwise.
  SkPath glyphOutline;
  uint32_t glyphOutlineRev = ~0u;
  // Element::boundary(Boundary::Coverage): the silhouette of what this
  // node's layer drew, traced off that layer's alpha, in the node's own
  // space. Re-traced when the layer that produced it is invalidated —
  // `paintDirty`, which every content, prop and layout change raises on
  // the node and on every ancestor — and on every frame of a subtree whose
  // volatility means nothing about it is cached. The size it was traced at
  // is kept beside it because a resized box is a different silhouette and
  // is the one invalidation that must not depend on a flag.
  SkPath coverageOutline;
  SkSize coverageOutlineSize = {-1.0f, -1.0f};
  float coverageOutlineScale = -1.0f;  ///< the device scale it was traced at
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
    std::vector<geometry::path::Contour> contours;
    float total = 0;      // the arc length across all of them
    bool closed = false;  // every contour closed → t WRAPS
  };
  std::unique_ptr<MotionCache> motion;

  ~Instance();
  float resolveFloat(Instance::Slot slot,
                     const motion::Animatable<float>& v) const;
  /** The same resolution over an explicitly-held motion — the span
   *  endpoints, whose count the description decides. One body: a bound
   *  Output wins, then a running ramp, then the plain value. */
  float resolveFloatAt(const AnimatedFloat* anim,
                       const motion::Animatable<float>& v) const;
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

/** The node's text-engine state, created on first use. */
inline TextState& textStateOf(Instance& inst) {
  if (!inst.textState) inst.textState = std::make_unique<TextState>();
  return *inst.textState;
}

inline bool childrenCarryYoga(const Instance& inst) {
  return inst.yoga != nullptr && inst.desc && !inst.desc->layout.positioned &&
         inst.desc->kind != Kind::Text;
}

/** WHERE A LEAF STANDS IN ITS STORY, read off the retained instance: the
 *  line offset the frame chain gave it, and its own key for the frame-local
 *  address. A leaf that is not a frame of a chain answers a zero offset and
 *  its own key, so the ordinary text is the general case with nothing
 *  subtracted. */
[[nodiscard]] inline TextScope scopeOf(const Instance& inst) {
  const bool threads = inst.desc && inst.desc->textData &&
                       !inst.desc->textData->threadTo.empty();
  return {inst.threadLineOffset, inst.threadStoryLines,
          threads || inst.threadLineOffset > 0,
          inst.desc ? std::string_view(inst.desc->key) : std::string_view{}};
}

}  // namespace sigil::compose::detail
