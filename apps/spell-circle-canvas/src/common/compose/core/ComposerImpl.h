#pragma once

/** @file
 * Internal to the kernel — Composer::Impl, the retained state behind the
 * facade and the method set every phase translation unit defines its slice
 * of.
 */

#include <include/core/SkBlendMode.h>
#include <sigilcore/cache/Bake.h>
#include <sigilcore/cache/Volatility.h>
#include <sigilcore/reconcile/Phases.h>
#include <sigilcore/reconcile/Reconciler.h>

#include <boost/unordered/unordered_flat_map.hpp>

#include "Instance.h"
#include "Lanes.h"
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
  std::vector<detail::Instance*> flowInstances;  // flowAround() text nodes
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
      view;  // output view transform (null filter = pass-through)
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
  static const std::string& keyOf(const Description& description) { return description->key; }
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
  static const Description& descriptionOf(const Element& child) { return child.node(); }
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
  /** @p movingAbove: a bound or transitioning transform is connected on
   *  some ancestor. A node carrying a world-space material below one has
   *  its node→root matrix changing off the describe clock, which is CONTENT
   *  volatility for that node — and it joins the memoized scalar lane,
   *  because that matrix is six floats, so the recording survives between
   *  ticks and the flag releases when the motion settles. Threaded down the
   *  existing recursion; everything else ignores it. */
  /** What the walk threads down to a child about the planes above it,
   *  beside `movingAbove`: whether the shared space it stands in is
   *  moving (its host's transform, or the host's own space), whether the
   *  view its parent declares is live, and whether it stands in a space at
   *  all — a node whose projection moves for any of those reasons is
   *  moving exactly as one whose own lane is. */
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
                     bool matrixStable,
                     float hostScale, SkBlendMode leafBlend, float leafOpacity,
                     detail::Instance::ContentScalars&& scalars);

  // ---- layout (Layout.cpp) ----
  bool applyCustomLayouts(detail::Instance& inst);
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
  /** Sorts the derive lists into the order their declared reads imply —
   *  stable, so a list whose members read none of each other is untouched. */
  void orderDerivedByReads();
  void deriveRoute(detail::Instance& inst);

  // ---- the node's paint transform, resolved once (Bounds.cpp) ----
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
    // The depth lanes: the plane's turn about x and y, its depth, and its
    // depth scale. At rest they are exactly the identity, and a node at
    // rest in all four is a 2D node in every consumer.
    float rx = 0, ry = 0, tz = 0, sz = 1;
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
    /** Has a depth lane left rest? Then the node is a PLANE turned or moved
     *  in depth, its matrix is the 4x4 of matrix44() flattened, and the
     *  2D producers below are not asked. THE ONE DEFINITION, for the same
     *  reason pivoted() is: a consumer that spells its own and omits a lane
     *  draws a plane where it cannot be hit. */
    bool spatial() const { return rx != 0 || ry != 0 || tz != 0 || sz != 1; }
    /** The matrix these lanes describe, prepended with `anchor` (the
     *  layout offset — pass {0, 0} for node-local): the translate lanes,
     *  then — gated on pivoted(), NOT a copy of it — the origin-pivoted
     *  rotate → scale → skew stack. THE ONE PRODUCER for recordBounds()'s
     *  child union and hitInstance()'s inverse. The anchor folds into the
     *  FIRST translate rather than being post-concatenated, because the two
     *  associate their float multiplies differently and recordBounds()'s
     *  results must stay bitwise stable.
     *
     *  The 2D producer: a node whose depth lanes have left rest is placed
     *  by matrix44() instead, and every consumer asks spatial() first. */
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
    /** The SAME stack as a 4x4, with the depth lanes in it: the translate
     *  lanes (z included), then about the 3D origin the CSS rotation list
     *  `rotateX · rotateY · rotateZ`, the scale with its depth factor, and
     *  the skew. The 2D lanes sit in this product exactly where matrix()
     *  puts them, so a node that turns about y keeps the rotate, scale and
     *  skew it had while flat. THE ONE 4x4 PRODUCER: paint's flattening,
     *  the bounds union, the hit test's inverse, the depth sort and the
     *  node→root accumulation all read this, in this order of operations,
     *  and the settle compare between two of them needs the products to
     *  agree bit for bit. `depth` is the node's block, null on a node
     *  without one (then the origin has no z). */
    SkM44 matrix44(SkPoint anchor, const detail::PaintProps& p,
                   const detail::DepthData* depth, float w, float h) const {
      SkM44 m = SkM44::Translate(anchor.x() + tx, anchor.y() + ty, tz);
      if (pivoted() || spatial()) {
        const SkPoint origin = detail::resolveOrigin(p, w, h);
        const float oz = depth ? depth->originZ : 0.0f;
        m.preTranslate(origin.x(), origin.y(), oz);
        if (rx != 0) m.preConcat(detail::rotateXMatrix(rx));
        if (ry != 0) m.preConcat(detail::rotateYMatrix(ry));
        if (rot != 0) m.preConcat(detail::rotateZMatrix(rot));
        if (scl != 1 || sx != 1 || sy != 1 || sz != 1)
          m.preScale(scl * sx, scl * sy, sz);
        if (skx != 0 || sky != 0)
          m.preConcat(detail::skewMatrix(std::tan(skx * 0.017453293f),
                                         std::tan(sky * 0.017453293f)));
        m.preTranslate(-origin.x(), -origin.y(), -oz);
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
      auto& [tx, ty, rot, scl, sx, sy, skx, sky, rx, ry, tz, sz] = v;
      static_assert(
          std::tuple_size_v<decltype(std::tie(tx, ty, rot, scl, sx, sy, skx,
                                              sky, rx, ry, tz, sz))> == 12,
          "NodeTransform gained or lost a lane — put it in pivoted() or "
          "spatial() above (unless it is a pure translate), in matrix()'s "
          "and matrix44()'s builds, and in transformOf()'s resolve, then "
          "bump this count.");
    }
  };
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
  void paintContent(detail::Instance& inst, SkCanvas& canvas,
                    float contentScale,
                    SkBlendMode leafBlend = SkBlendMode::kSrcOver,
                    float leafOpacity = 1.0f, Phase phase = Phase::All);
  const SkPath& resolveOutline(detail::Instance& inst, SkSize size) const;
  /** THE COVERAGE BOUNDARY (Coverage.cpp): the silhouette of what this
   *  node's layer drew, in the node's own space.
   *
   *  The node's fill, content and children are rasterised into an alpha
   *  surface of their own and the covered pixels are traced back into a
   *  path, so the answer is the visible extent of an image with a cut-out,
   *  of a clipped or masked subtree, of anything a shape and a glyph run
   *  cannot describe. Cached on the instance and re-traced only when the
   *  layer that produced it is invalidated. */
  const SkPath& coverageOutline(detail::Instance& inst, SkSize size,
                                float contentScale);
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
   *  by every decoration's declared bleed and any routed path, and NOTHING
   *  from its children. The split bake sizes its layer with this — and the
   *  independence from the children is the load-bearing part, not an
   *  optimisation: `recordBounds` unions the children in, so it changes
   *  every frame a child moves, and a bake rect that changes every frame is
   *  a bake remade every frame. */
  SkRect ownPaintBounds(detail::Instance& inst);
  /** The rect a node's recording must cover — in its own local plane, or,
   *  for a node hosting a shared space, in the plane that space is drawn
   *  on, which is where its children stand. `space` is the accumulation of
   *  the space the node itself stands in, null under a flat parent; a
   *  hosting node nested in a space needs it to place its own plane. */
  SkRect recordBounds(detail::Instance& inst, const SkM44* space = nullptr);

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
