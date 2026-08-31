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
  std::unordered_map<std::string, detail::Instance*> bySlot;
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
  void scanReleasedScalars();  // defined in Volatility.cpp beside the memos
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
  using Desc = std::shared_ptr<detail::ElementNode>;
  static const std::string& keyOf(const Desc& desc) { return desc->key; }
  static bool equal(const Desc& a, const Desc& b) {
    return detail::propsEqual(*a, *b);
  }
  /** Slot content is owned by renderSlot(), not the description. */
  static bool reconcilesChildren(const Desc& desc) {
    return desc->kind != detail::Kind::Slot;
  }
  static const std::vector<Element>& children(const Desc& desc) {
    return desc->children;
  }
  static const Desc& descOf(const Element& child) { return child.node(); }
  static const detail::MemoData* memoOf(const Desc& desc) {
    return desc->memoData ? &*desc->memoData : nullptr;
  }
  static Desc produce(const detail::MemoData& memo) {
    return memo.invoke(memo.props).node();
  }
  // Acting on an instance:
  /** A fresh instance for @p node under @p parent, patched once. @p ordinal
   *  is its order among the children created in the same patch and @p count
   *  the parent's child count, which is what staggerChildren() cascades
   *  over; the carry that cascade accumulates is host state. */
  std::unique_ptr<detail::Instance> create(const Desc& node,
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
  core::SubtreeVerdict computeVolatile(detail::Instance& inst,
                                       bool movingAbove = false);
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
  void recordPicture(detail::Instance& inst, float hostScale,
                     SkBlendMode leafBlend, float leafOpacity,
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

  // ---- the text painter, as the kernel reaches it ----
  /** The engine a text description installed, or null for text the kernel
   *  draws at rest by itself. */
  static const TextPainterOps* textPainterOf(const detail::Instance& inst) {
    const detail::ElementNode* node = inst.desc.get();
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
