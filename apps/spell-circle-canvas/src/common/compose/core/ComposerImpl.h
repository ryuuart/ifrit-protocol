#pragma once

/** @file
 * Internal to the kernel — Composer::Impl, the retained state behind the
 * facade and the method set every phase translation unit defines its slice
 * of.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>
#include <sigilcore/cache/Bake.h>
#include <sigilcore/cache/Volatility.h>
#include <sigilcore/reconcile/Phases.h>
#include <sigilcore/reconcile/Reconciler.h>
#include <sigilgeometry/path/Numeric.h>

#include <algorithm>
#include <boost/unordered/unordered_flat_map.hpp>
#include <cstdlib>
#include <optional>

#include "Instance.h"
#include "Lanes.h"
#include "NodeTransform.h"
#include "SlotSpecs.h"
#include "Transforms.h"

namespace sigil::compose {

/** WHAT THE PICTURE TIER BAKES: the node, the canvas its recording is
 *  replayed onto, and the paint context the recording freezes in — the
 *  leaf blend and opacity a recording bakes rather than applies, and the
 *  content scalars it was recorded from. */
struct PictureBakeTarget {
  Composer::Impl* painter = nullptr;
  detail::Instance* inst = nullptr;
  SkCanvas* canvas = nullptr;
  float hostScale = 1;
  SkBlendMode leafBlend = SkBlendMode::kSrcOver;
  float leafOpacity = 1;
  detail::Instance::ContentScalars* scalars = nullptr;
  /** The matrix the recording's ops reach the DEVICE through when it is
   *  replayed this frame — the canvas's own matrix composed out through
   *  every enclosing recording. A recording holding a device-space bake is
   *  exact under this one matrix and is remade when it differs. */
  SkMatrix deviceMatrix = SkMatrix::I();
  /** Whether that matrix is the one the node was drawn under last frame.
   *  The outermost recording hands it to every device bake inside it as
   *  the "holding still" verdict those bakes cannot observe for themselves,
   *  being painted only when the recording is. */
  bool matrixStable = true;
  /** The device clip the recording's ops were cut to. An ink clip inside
   *  one names whole device pixels, so a recording holding a device bake
   *  is exact for the clip it was made under and is remade under
   *  another. */
  SkIRect deviceClip = SkIRect::MakeEmpty();
};

/** THE RECORDED-COMMAND-LIST TIER, behind the kernel's bake seam. Taking
 *  it, replaying it and dropping it are this library's — an SkPicture is a
 *  Skia value and its cull rect, its recording depth and the leaf paint it
 *  freezes are all compose's rules. Whether to take one THIS FRAME is not:
 *  that is the kernel's three-way answer over what the proof said and what
 *  the node is holding, and the pixel tiers beside this one ask it in
 *  exactly the same words.
 *
 *  Stateless, so every instance of it is the same value. */
struct PictureBake : core::BakeOps<PictureBakeTarget> {
  void take(PictureBakeTarget& t) const override;
  void replay(PictureBakeTarget& t) const override;
  void drop(PictureBakeTarget& t) const override;
  [[nodiscard]] bool held(const PictureBakeTarget& t) const override;
  // Stateless: every instance of it is the same value. (A defaulted
  // comparison would compare the abstract base subobject, which has none.)
  bool operator==(const PictureBake&) const { return true; }
};

// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Composer::Impl {
  motion::Ticker& ticker;
  sigil::weave::FontContext& fonts;
  const motion::FrameClock* clock = nullptr;

  SkSize size = SkSize::MakeEmpty();
  std::unique_ptr<detail::Instance> root;
  /** The reconciler, with this composer as its host: it owns the shape of
   *  the tree — matching, memo, the identity prune, the counts — and
   *  reaches everything else through the host operations below. */
  using Reconciler = core::Reconciler<Impl, detail::Instance,
                                      std::shared_ptr<detail::ElementNode>>;
  Reconciler reconciler;
  YGConfigRef yogaConfig = nullptr;
  bool needsLayout = true;
  bool contentDirty = true;
  Reconciler::KeyIndex byKey;
  // Slots get their OWN index. They live in byKey too (so bounds() and
  // hitTest() still answer for a slot's name), but a slot's CONTENT may
  // legitimately carry a root .key() with the same name — and a child is
  // indexed after its parent, so in a single shared map the content would
  // overwrite the slot's entry and every later renderSlot() would silently
  // find the wrong instance. Two namespaces, no collision.
  boost::unordered_flat_map<std::string, detail::Instance*, core::KeyHash,
                            std::equal_to<>>
      bySlot;
  // The EDGE STORE, rebuilt with the key index each render: routed nodes
  // (connector()/rail()) as a flat list in tree order, plus the back-index
  // anchor-key → routes-anchored-there. The derive pass iterates these flat
  // lists instead of recursing the whole tree, and routesAt() answers graph
  // queries ("which edges touch this node") in O(routes-at-node).
  std::vector<detail::Instance*> routedInstances;
  std::vector<detail::Instance*> flowInstances;      // flowAround() text nodes
  std::vector<detail::Instance*> tetheredInstances;  // tether() nodes
  // Text nodes carrying mark() on a path-laid run. Their curve resolves
  // against the node's FINAL box, which measurement never sees, so their
  // marks resolve in a post-layout pass over this flat list instead of
  // inside measure like a flow run's.
  std::vector<detail::Instance*> pathMarkInstances;
  // Text nodes that thread INTO another frame. The chain is walked in the
  // derive pass, because frame b's fill begins where frame a's RESULT ended
  // and the phase order has no edge for that.
  std::vector<detail::Instance*> threadedInstances;
  // …and the frames THEY thread into, kept from the last walk so a frame
  // that stops being a target is unbounded again the moment it does.
  std::vector<detail::Instance*> threadTargets;
  boost::unordered_flat_map<std::string, std::vector<detail::Instance*>,
                            core::KeyHash, std::equal_to<>>
      routesByAnchor;
  bool volatileDirty = true;  // recompute needed (render or animation)
  bool tickerWasActive = false;
  // The root verdict's volatileAbove bit: unlike Instance::subtreeVolatile,
  // this includes the root's own opacity and transform, which can change the
  // composited pixels without invalidating any content cache below it.
  bool rootVolatile = false;
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
  void scanReleasedScalars();  // defined in Volatility.cpp beside the memos
  // Recomputed with the key index, so unmounting the last derived or pinned
  // node clears them rather than latching them on forever.
  bool hasDerived = false;  // any flowAround/connector/rail in the tree
  bool hasCustomLayout = false;
  bool hasCenterPins = false;  // any centerAt() in the tree
  bool liveOnly = false;       // snapshot(): skip per-node caches
  material::skia::Effect
      view;  // output view transform (no filter = pass-through)
  // The view as its author described it, when they described a Material.
  // Kept because how a Material LOWERS depends on the surface it lands
  // on, which is known only at draw: a view whose channels are
  // independent runs as a table on an eight-bit surface and as its
  // program anywhere else. `viewColorType` is the surface `view` was
  // lowered for, so a stable surface lowers once and every later frame
  // compares one enum.
  std::optional<material::Material> viewMaterial;
  SkColorType viewColorType = kUnknown_SkColorType;
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
  // Composer::setAutoTexturePromotion (the INTENT).
  Composer::PromotionPolicy autoPromote = Composer::PromotionPolicy::ByCost;
  /** Composer::setBakeDensity: device pixels per layout unit every pixel
   *  bake is taken at, whatever the frame's matrix says. Zero is the
   *  coarse ladder read off that matrix. */
  float bakeDensity = 0.0f;
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
  Composer::PromotionPolicy autoPromoteEffective =
      Composer::PromotionPolicy::ByCost;
  // Promoted bakes are pixels, and a dense scene can carry many
  // full-canvas nodes at several megabytes each. A budget, carried from the
  // previous frame (paint order is stable, so the previous frame's total is
  // the right question to ask before adding one more), keeps an automatic
  // win from becoming an automatic out-of-memory.
  size_t promotedBytes = 0;      // accumulated during the current paint
  size_t promotedBytesLast = 0;  // what the previous frame ended up holding
  // >0 while painting INTO an SkPicture, or into the coverage trace's
  // offscreen raster. A node painted here is painted only when the
  // recording is taken, so nothing it observes frame over frame is a
  // history.
  int recordingDepth = 0;
  // …and of those, how many may be REPLAYED UNDER A DIFFERENT MATRIX than
  // they were made under: a recording made at, or beneath, a node whose
  // transform is declared live replays under the motion, and the coverage
  // trace rasterizes at a scale of its own. A device-space bake is blitted
  // with the matrix reset at an absolute device rect, so it may be taken
  // only while this is zero — at the root, or inside recordings that are
  // all PINNED to the matrix they were made under (Instance::pictureMatrix)
  // and remade when it changes.
  int unpinnedRecordingDepth = 0;
  // The matrix the innermost open recording's ops reach the device through
  // when it is replayed — identity outside any recording. A node inside a
  // recording composes its canvas matrix through this to find the device
  // grid it is drawn on; the inverse is what a device blit concatenates so
  // the replay lands it at the device rect it was baked for.
  SkMatrix recordingReplay = SkMatrix::I();
  SkMatrix recordingReplayInverse = SkMatrix::I();
  // Whether the outermost open recording's device matrix is the one it
  // was drawn under last frame. Inside a recording this stands in for the
  // per-frame device-rect history a device bake needs and cannot keep.
  bool recordingMatrixStable = true;
  // Accumulated over the recording being taken: how many device-space
  // blits it holds — its own nodes' and those of every held picture
  // replayed inside it — and whether a node inside was refused a device
  // bake for matrix motion alone, so the picture is retaken once the
  // matrix holds still. Stamped onto the instance when the recording ends.
  uint32_t recordingDeviceBakes = 0;
  bool recordingDeviceDeferred = false;
  // …and how many COVERAGE BOUNDARIES it holds, on the same terms. A
  // traced silhouette is a staircase of whole device pixels, so a
  // recording that froze one in is exact at the scale it was traced at
  // and stale at every other, exactly as one holding a blit is stale
  // under another matrix.
  uint32_t recordingCoverageTraces = 0;
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
  static constexpr size_t kPromotedBudget = size_t{192} * 1024 * 1024;
  /** THE MARGIN EVERY DEVICE BAKE IS ALLOCATED WITH, in device pixels, for
   *  a bake whose larger side is @p extent.
   *
   *  A bake surface sized to exactly what the node paints puts that paint
   *  flush against the surface's own edge, and Skia does not rasterize a
   *  path that reaches its clip the way it rasterizes one standing clear of
   *  it: the decision is taken on the path's CONTROL-POINT bounds, and the
   *  two routes do not answer the same antialiased coverage. A stroked
   *  curve baked flush moves by TENS of code values along its whole length
   *  — not the one an integer device offset costs — so the margin is what
   *  makes a promoted node paint the picture its live paint paints.
   *
   *  It is a FRACTION of the bake because the reach it has to cover is one:
   *  a stroker approximates an offset curve with cubics whose control
   *  points stand outside the ink they draw, by about a hundredth of the
   *  curve's own extent. A thirty-second is that with room, and the two are
   *  the floor for a bake too small for the fraction to reach a pixel. What
   *  it costs is a frame of transparent pixels around each bake. */
  static constexpr int bakeMargin(int extent) {
    return 2 + std::max(0, extent) / 32;
  }
  /** THE SURFACE A DEVICE BAKE IS TAKEN ON — one per depth of nesting, the
   *  size of the largest rect any bake has needed, and reused: a bake is
   *  taken on the canvas's own grid so that no offset enters the layer's
   *  matrix (see PaintPass::takeDeviceBake), and a surface per node at that
   *  size would be a canvas per node. What a node keeps is the image taken
   *  off this, so nothing here outlives the bake that filled it. */
  std::vector<sk_sp<SkSurface>> bakeSurfaces;
  size_t bakeDepth = 0;  ///< how many bakes are in flight above this one
  SkSurface* bakeSurface(SkCanvas& canvas, SkISize need);
  std::vector<Composer::NodeCost> profileRows;
  double profChildMs = 0;
  int profDepth = 0;
  // render()/renderSlot() phase time accumulated since the previous draw();
  // draw() publishes it as stats.reconcileMs and zeroes the accumulator.
  double reconcileAccumMs = 0;

  Impl(motion::Ticker& t, sigil::weave::FontContext& f)
      : ticker(t), fonts(f), reconciler(*this) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return clock ? clock->elapsed() : 0.0; }

  // ---- the reconciler's host (ReconcileHost.cpp) ----
  // The ReconcileHost operations, in the reconciler's terms. Reading a
  // description:
  using Description = std::shared_ptr<detail::ElementNode>;
  static const std::string& keyOf(const Description& description) {
    return description->key;
  }
  static bool equal(const Description& a, const Description& b) {
    return detail::propsEqual(*a, *b);
  }
  /** Slot content is owned by renderSlot(), not the description. */
  static bool reconcilesChildren(const Description& description) {
    return description->kind != detail::Kind::Slot;
  }
  static const std::vector<Element>& children(const Description& description) {
    return description->children;
  }
  static const Description& descriptionOf(const Element& child) {
    return child.node();
  }
  static const detail::MemoData* memoOf(const Description& description) {
    return description->memoData ? &*description->memoData : nullptr;
  }
  static Description produce(const detail::MemoData& memo) {
    return memo.invoke(memo.props).node();
  }
  // Acting on an instance:
  /** A fresh instance for @p node under @p parent, patched once. @p ordinal
   *  is its order among the children created in the same patch and @p count
   *  the parent's child count, which is what staggerChildren() cascades
   *  over; the carry that cascade accumulates is host state. */
  std::unique_ptr<detail::Instance> create(const Description& node,
                                           detail::Instance* parent,
                                           size_t ordinal, size_t count);
  /** Everything the composer does to an instance whose description changed:
   *  @p prev is null on the first patch. */
  void onPatched(detail::Instance& inst, const detail::ElementNode* prev,
                 const detail::ElementNode& next);
  /** Sorts the paint order, reattaches every child to the parent's Yoga node
   *  in `children` order, and dirties the parent when @p structureChanged —
   *  a child mounted, unmounted or moved. */
  void reorder(detail::Instance& parent, bool structureChanged);
  /** Whether a surviving @p match must be unmounted and created afresh under
   *  @p parent rather than patched in place. */
  bool remountRequired(const detail::Instance& match,
                       const detail::Instance& parent);
  /** Marks the instance's paint dirty up to the root and the content dirty. */
  void invalidate(detail::Instance& inst);
  /** An instance that left the tree: retired at once — its destructor
   *  frees its Yoga node and its motions disconnect with their outputs. */
  void destroy(std::unique_ptr<detail::Instance> inst, uint64_t frame);
  /** The key index and the edge store, rebuilt over the whole tree after
   *  every reconcile (Reconcile.cpp). */
  void rebuildKeyIndex();
  void applyLayoutProps(detail::Instance& inst);
  /** Builds the instance's Paragraph from whichever content form its
   *  description carries — plain utf8, `weave::rich()` runs, or a copy of a
   *  supplied Paragraph — and then applies the span restyles in
   *  declaration order. @p lines is the geometry a previous layout
   *  produced, which is what a `weave::sel::line` restyle addresses; empty
   *  leaves those selectors unresolved. @p columns carries the same
   *  geometry for a vertical passage, where a line IS a column. */
  void materializeText(
      detail::Instance& inst,
      std::span<const sigil::weave::LineMetrics> lines = {},
      std::span<const sigil::weave::ColumnMetrics> columns = {});
  /** The options a text node actually lays out under: the full-control
   *  overload's value where it has one, with every field a fluent setter
   *  named written over it. */
  sigil::weave::ParagraphLayoutOptions textLayoutOptions(
      const detail::Instance& inst) const;

  // ---- transitions (Transitions.cpp) ----
  /** Every lane of @p node, Slot lanes first in kSlotSpecs order, then the
   *  Span, Gate and Track families in declaration order. The overload
   *  writing into @p out refills a caller-owned vector. */
  std::vector<detail::Lane> lanes(const detail::ElementNode& node);
  void lanes(const detail::ElementNode& node, std::vector<detail::Lane>& out);
  void applyTransitions(detail::Instance& inst, const detail::ElementNode& prev,
                        const detail::ElementNode& next);
  void applyMountTransitions(detail::Instance& inst,
                             const detail::ElementNode& node);

  // ---- volatility & caching (Volatility.cpp) ----
  /** What the walk threads down to a child about the planes above it:
   *  whether a bound or transitioning transform is connected on some
   *  ancestor, whether the shared space it stands in is moving (its
   *  host's transform, or the host's own space), whether the view its
   *  parent declares is live, and whether it stands in a space at all — a
   *  node whose projection moves for any of those reasons is moving
   *  exactly as one whose own lane is.
   *
   *  A node carrying a world-space material under a moving ancestor has
   *  its node→root matrix changing off the describe clock, which is
   *  CONTENT volatility for that node — and it joins the memoized scalar
   *  lane, because that matrix is six floats, so the recording survives
   *  between ticks and the flag releases when the motion settles. */
  struct Above {
    bool moving = false;           ///< a connected transform on an ancestor
    bool spaceMoving = false;      ///< the space this node stands in moves
    bool perspectiveLive = false;  ///< the parent's perspective lane is live
    bool inSpace = false;          ///< the parent hosts a shared space
  };
  core::SubtreeVerdict computeVolatile(detail::Instance& inst, Above above);
  /** The root's walk: nothing stands above it. */
  core::SubtreeVerdict computeVolatile(detail::Instance& inst) {
    return computeVolatile(inst, Above{});
  }
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
  /** The picture tier's seam value: the kernel decides bake, replay or
   *  live, and these are the operations that carry the decision out. */
  core::Bake<PictureBakeTarget> pictureBake{PictureBake{}};
  /** Record the node's own paint into a replayable picture, freezing the
   *  leaf blend and opacity into it and stamping the values it was
   *  recorded from. The bake half of the picture tier. */
  void recordPicture(detail::Instance& inst, const SkMatrix& deviceMatrix,
                     const SkIRect& deviceClip, bool matrixStable,
                     float hostScale, SkBlendMode leafBlend, float leafOpacity,
                     detail::Instance::ContentScalars&& scalars);

  // ---- layout (Layout.cpp) ----
  bool applyCustomLayouts(detail::Instance& inst);
  SkSize minimumSizeOf(detail::Instance& child);
  bool applyCenterPins(detail::Instance& inst);
  /** The passes, as the runner sees them. Each returns whether it changed
   *  geometry; the non-converging ones answer false. */
  bool phaseYoga();           ///< Yoga's calculate pass over the root
  bool phaseCustomLayouts();  ///< custom layout() containers, when any
  bool phaseCenterPins();     ///< centerAt() pins, when any
  bool phaseDerive();         ///< flow exclusions and routes, when any
  bool phasePathMarks();      ///< mark() on path-laid runs
  bool phaseSyncRects();      ///< invalidate recordings whose rect moved
  /** The runner's list: Yoga, the converging group, then the post-layout
   *  passes. The derive family (connector, rail, band, flowAround) reaches
   *  the schedule ONLY as the `derive` entry of the converging group — the
   *  registration IS its seam — and the runner's settle step re-runs it
   *  after every relayout so a routed plate is drawn against settled
   *  geometry. */
  static constexpr core::Phase<Impl> phases[] = {
      {"yoga", &Impl::phaseYoga, false},
      {"customLayouts", &Impl::phaseCustomLayouts, true},
      {"centerPins", &Impl::phaseCenterPins, true},
      {"derive", &Impl::phaseDerive, true},
      {"pathMarks", &Impl::phasePathMarks, false},
      {"syncRects", &Impl::phaseSyncRects, false},
  };
  /** Rounds the converging group may run before the runner gives up: what
   *  guarantees termination if two writers ever disagree permanently. */
  static constexpr int kConvergeRounds = 3;
  /** Runs the phase list when the tree needs layout: the converging group
   *  repeats until a round changes nothing, relaying out and settling the
   *  routes between rounds. */
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
  /** Walks every frame chain in order, handing each frame the cursor the
   *  one before it left. True when a cursor moved. */
  bool resolveThreads();
  bool resolveTethers();
  /** What a run of a chain came to when it was filled at one depth: the
   *  lines it placed, whether the last of them still had something over,
   *  and the word the run stopped at. */
  struct ChainFill {
    uint32_t lines = 0;
    uint32_t cursor = 0;
    bool overflowed = false;
  };
  ChainFill fillRun(const std::vector<detail::Instance*>& run, size_t first,
                    size_t last, float depth, uint32_t cursor);
  bool balanceRuns(const std::vector<detail::Instance*>& chain);
  /** How many times a balanced run's depth is halved. A fixed count leaves
   *  the answer a hair deeper than the tightest depth and costs the same
   *  whatever the story is. */
  static constexpr int kBalanceSteps = 8;
  /** Sorts the derive lists into the order their declared reads imply —
   *  stable, so a list whose members read none of each other is untouched. */
  void orderDerivedByReads();
  void deriveRoute(detail::Instance& inst);

  // ---- the node's paint transform, resolved once (Bounds.cpp) ----
  /** The node's lanes for THIS frame (NodeTransform.h), resolved once so
   *  paint's matrix, recordBounds's child union and hitInstance's inverse
   *  all describe the same matrix. */
  NodeTransform transformOf(detail::Instance& inst);

  // ---- depth (Depth.cpp): the plane a node is, and the space it hosts ----
  /** The node's 4x4 in the plane its PARENT paints on: the parent's
   *  perspective, then the layout offset, then the node's own lanes about
   *  its origin — `Persp(parent) · T(rect) · matrix44`. The parent's
   *  perspective is the child's business and is folded here, once, so no
   *  consumer composes it on its own. A node inside a shared space
   *  prepends that space's accumulation to this. */
  SkM44 depthMatrixOf(detail::Instance& inst, const NodeTransform& tf,
                      const SkRect& rect);
  /** Do this node's children share its space — preserve3d(), and none of
   *  the grouping properties that flatten it (a clip, an opacity below 1,
   *  a blend, an effect, a backdrop, a mask, a coverage boundary, an
   *  explicit bake)? Asked by paint, the hit test, the bounds walk and the
   *  volatility walk, and answered by ONE body, because the four must
   *  agree about which plane a child is drawn on. */
  bool hostsSpace(detail::Instance& inst);
  /** THE DEPTH ORDER of a hosting node's children: its paint order (zIndex,
   *  then declaration) stable-sorted by the depth of each child's centre
   *  in the space — farthest first, so a nearer plane covers a farther
   *  one wherever the two overlap. `space` is the host's own 4x4 in the
   *  plane the space is drawn on, which every child's matrix begins with.
   *  Planes are never intersected: a child crossing another is drawn
   *  whole, in this order. */
  void depthOrder(detail::Instance& host, const SkM44& space,
                  std::vector<size_t>& out);
  /** A SHARED SPACE, open while a hosting node's children are painted or
   *  hit. The canvas stays at the plane the space is drawn on — the
   *  hosting node concatenates nothing for its children — and every node
   *  in the space places itself with `accum · depthMatrixOf` flattened,
   *  relative to that plane. That is what makes the space free of any
   *  inverse: a host turned edge-on has a singular plane of its own and
   *  its children still stand where the space puts them. `rootToPlane` is
   *  the node→root matrix of that plane, which a node in the space builds
   *  its own node→root from. */
  struct Space {
    SkM44 accum;           ///< the plane the space is drawn on → the host
    SkMatrix rootToPlane;  ///< …and that plane's own node→root
  };
  /** The space the node being painted stands in — set by its parent while
   *  that parent hosts one, null otherwise. Saved and restored around each
   *  paint() frame. */
  const Space* curSpace = nullptr;
  /** The plane a HOSTING node's own paint concatenates inside paintContent:
   *  paint() leaves the canvas at the plane the space is drawn on, so the
   *  children can place themselves, and the host's own marks, fill and
   *  content are drawn under this instead. Absent for every other node. */
  std::optional<SkMatrix> curOwnPlane;
  /** …and whether that own plane is drawn at all: a host facing away with
   *  its backface hidden, or turned edge-on, paints nothing of its own and
   *  still paints the children its space holds. */
  bool curOwnHidden = false;
  /** Where on its motion path this node sits, in its PARENT's space, and
   *  the auto-orient angle in degrees. Nullopt when no path is engaged
   *  (absent, empty, or resolving to no measurable length) — the
   *  translate lanes then stand. Rebuilds the instance's arc-length table
   *  when the Shape value or the parent size no longer matches. */
  std::optional<std::pair<SkPoint, float>> motionPathSample(
      detail::Instance& inst, const SkSize& frame);

  // ---- the text painter, as the kernel reaches it ----
  /** The engine a text description installed, or null for text the kernel
   *  draws at rest by itself. */
  static const TextPainterOps* textPainterOf(const detail::Instance& inst) {
    const detail::ElementNode* node = inst.description.get();
    return node && node->textData ? node->textData->painter.get() : nullptr;
  }
  /** Resolves the node's mark() rects through its painter; a node with no
   *  painter anchors nothing. */
  void resolveTextMarks(detail::Instance& inst) {
    if (const TextPainterOps* painter = textPainterOf(inst))
      painter->marks(inst);
    else
      inst.textMarkRects.clear();
  }
  /** Lays out the node's annotate() readings against the layout its letters
   *  are drawn from. The engine answers even for a passage that dresses
   *  nothing else, because a reading IS the dressing and the base may
   *  carry no other. */
  void resolveTextAnnotations(detail::Instance& inst) {
    inst.textAnnotations.clear();
    if (!inst.description || !inst.description->textData ||
        inst.description->textData->annotations.empty())
      return;
    const TextPainterOps* painter = textPainterOf(inst);
    if (!painter) painter = detail::registeredTextEngine();
    if (painter) painter->annotations(inst);
  }

  // ---- paint (StackingPainter.cpp and the paint-phase files beside it) ----
  float hostScale = 1.0f;  // device px per layout px at draw() entry
  void paint(detail::Instance& inst, SkCanvas& canvas);
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
  /** @p deferLayerEffect leaves the node's own layer effect OUT of what is
   *  emitted, for a bake that is going to be filtered at its blit instead.
   *  Everything else is unchanged, so the bake holds exactly the pixels the
   *  effect's saveLayer would have been handed. */
  void paintContent(detail::Instance& inst, SkCanvas& canvas,
                    float contentScale,
                    SkBlendMode leafBlend = SkBlendMode::kSrcOver,
                    float leafOpacity = 1.0f, Phase phase = Phase::All,
                    bool deferLayerEffect = false);
  const SkPath& resolveOutline(detail::Instance& inst, SkSize size) const;
  /** THE COVERAGE BOUNDARY (Coverage.cpp): the silhouette of what this
   *  node's layer drew, in the node's own space.
   *
   *  The node's fill, content and children are rasterised into an alpha
   *  surface of their own and the covered pixels are traced back into a
   *  path, so the answer is the visible extent of an image with a cut-out,
   *  of a clipped or masked subtree, of anything a shape and a glyph run
   *  cannot describe. Cached on the instance and re-traced only when the
   *  layer that produced it is invalidated.
   *
   *  The surface covers `ownPaintBounds`, not `size`: a silhouette is the
   *  ink, and a raster allocated at the box cuts the boundary square
   *  wherever a carrier stands outside it. `size` is the box the content
   *  was laid out against, which the trace keeps only to know the node is
   *  sized at all and to re-trace when it changes. The path comes back in
   *  the node's own local space either way, so a node whose ink stays
   *  inside its box traces where it always did and one whose ink reaches
   *  past it traces wider. */
  const SkPath& coverageOutline(detail::Instance& inst, SkSize size,
                                float contentScale);
  /** WHAT A NODE SAYS ITS EDGE IS, in its own space — its glyph outlines
   *  under `Boundary::Glyphs`, the silhouette of what it drew under
   *  `Boundary::Coverage`, its declared shape otherwise. Empty when the
   *  node declares no silhouette at all and its box is the whole answer.
   *
   *  One reading, for the node's own decorations and for anything that
   *  borrows its edge, so a node cannot be dressed along one outline and
   *  flowed around along another. */
  SkPath boundaryOutlineOf(detail::Instance& target, float width, float height);
  /** The node whose coverage is being traced RIGHT NOW, if any.
   *
   *  A coverage boundary is what the node drew, and the node's own marks
   *  are what dress that boundary: drawing them into the trace would make
   *  the boundary a function of itself. So paintContent emits no marks for
   *  this one node while it is set, and asks it for no coverage boundary
   *  either — which is also what keeps the trace from re-entering itself.
   *  Its children, and their marks, are drawn: they are part of what the
   *  node drew, and none of them reads this node's boundary. */
  const detail::Instance* coverageTrace = nullptr;
  /** What the node paints BY ITSELF, in its own local space: its box grown
   *  by every decoration's declared bleed, any routed path and the shape it
   *  declares, and NOTHING from its children. The split bake sizes its
   *  layer with this — and the independence from the children is the
   *  load-bearing part, not an optimisation: `recordBounds` unions the
   *  children in, so it changes every frame a child moves, and a bake rect
   *  that changes every frame is a bake remade every frame. */
  SkRect ownPaintBounds(detail::Instance& inst);
  /** The rect a node's recording must cover — in its own local plane, or,
   *  for a node hosting a shared space, in the plane that space is drawn
   *  on, which is where its children stand. `space` is the accumulation of
   *  the space the node itself stands in, null under a flat parent; a
   *  hosting node nested in a space needs it to place its own plane. */
  SkRect recordBounds(detail::Instance& inst, const SkM44* space = nullptr,
                      bool forBake = false);
  /** The same union, for the rect a SURFACE is allocated to rather than the
   *  rect a layer or a recording is bounded by: every layer effect in the
   *  subtree is given the reach its own filter answers, because the skirt a
   *  blur, a glow or a shadow puts outside the content it filters is drawn
   *  INTO the allocation and a surface sized to the unfiltered content cuts
   *  it off square. A LAYER is not sized with this — Skia grows a filtered
   *  saveLayer for its filter already, and growing it here too would
   *  composite the layer over ground the picture does not stand on. */
  SkRect bakeBounds(detail::Instance& inst);
  /** WHERE THE SHAPE A NODE DECLARES ACTUALLY REACHES, outset by the same
   *  bleed its box is — empty on a node that declares none. A Shape is a
   *  function of a size and nothing requires what it returns to stand
   *  inside the box that size came from: a generator anchored on a centre
   *  of its own, a ring of contours drawn at radii the box knows nothing
   *  about. The node's surface is filled with that path and every
   *  decoration dresses it, so the ink is where the path is. It is a
   *  carrier of `ownPaintBounds` and therefore bounds EVERY rect a node is
   *  sized to — the allocation a device or local bake is made at, and the
   *  LAYERS too: a group's opacity layer, an effect's, the surface a lifted
   *  filter is run over. A layer whose bounds cut the node's own shape cuts
   *  the drawing, which is the same failure as an allocation that does; the
   *  clip a node's drawing is bounded by is the one it carries in, never
   *  the rect it was given room in. */
  SkRect declaredShapeBounds(detail::Instance& inst);

  // ---- hit testing / queries (Query.cpp) ----
  bool shapeContains(detail::Instance& inst, SkPoint local, SkSize size) const;
  /** The hit test's view of a shared space: the host's accumulation, and
   *  the point being tested in the plane the space is drawn on — a node
   *  in the space maps THAT point back through its own full projection,
   *  since its parent's local plane is not the plane it stands on. */
  struct HitSpace {
    SkM44 accum;
    SkPoint planePt;
  };
  /** @p parentPt is the point in the parent's local plane, read when the
   *  node stands on it; @p space is the shared space the parent hosts,
   *  null under a flat parent. */
  std::optional<std::string> hitInstance(detail::Instance& inst,
                                         SkPoint parentPt,
                                         const std::string* inheritedKey,
                                         const HitSpace* space);
};

}  // namespace sigil::compose
