#pragma once

/** @file
 * Internal to the kernel — Composer::Impl, the retained state behind the
 * facade and the method set every phase translation unit defines its slice
 * of. Its state is grouped by subject into the parts it derives from — the
 * root of the cascade, the tree's flat lists, change tracking, the
 * volatility walk, the surface map and view, the counters, the pixel bakes
 * and the paint walk — beside the reconciler's reading of a description
 * and the kernel's reach into a text painter. The composer itself carries
 * the destination, the tree, its reconciler and key index, and every
 * phase's methods.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>
#include <sigilcore/cache/Bake.h>
#include <sigilcore/cache/Volatility.h>
#include <sigilcore/reconcile/Phases.h>
#include <sigilcore/reconcile/Reconciler.h>
#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/Type.h>

#include <boost/unordered/unordered_flat_map.hpp>
#include <limits>
#include <optional>

#include "BakeSurfaces.h"
#include "CascadeRoot.h"
#include "ChangeTracking.h"
#include "DescriptionReading.h"
#include "DrawStatistics.h"
#include "Instance.h"
#include "Lanes.h"
#include "NodeTransform.h"
#include "PaintWalk.h"
#include "PictureBake.h"
#include "SlotSpecs.h"
#include "SurfaceMapView.h"
#include "TextPainterAccess.h"
#include "Transforms.h"
#include "TreeLists.h"
#include "VolatilityState.h"
#include "paint/BakeFormat.h"
#include "paint/GlyphInk.h"
#include "paint/RecordingState.h"

namespace sigil::compose {

// fields are grouped by what they belong to, not by size
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
struct Composer::Impl : detail::CascadeRoot,
                        detail::TreeLists,
                        detail::ChangeTracking,
                        detail::VolatilityState,
                        detail::SurfaceMapView,
                        detail::DrawStatistics,
                        detail::BakeSurfaces,
                        detail::PaintWalk,
                        detail::DescriptionReading,
                        detail::TextPainterAccess {
  motion::Engine& engine;
  sigil::weave::FontContext& fonts;

  SkSize size = SkSize::MakeEmpty();
  /** The owning draw's destination format, including while a picture canvas
   *  records commands without its own pixel format. */
  SkImageInfo outputInfo = SkImageInfo::MakeN32Premul(1, 1);
  /** Recordings may hold sampled device images from this destination. The
   *  recorder's identity also distinguishes a reused recorder address. */
  skgpu::graphite::Recorder* outputRecorder = nullptr;
  uint32_t outputRecorderId = 0;
  GrRecordingContext* outputContext = nullptr;
  /** Transparent pixel bakes keep the destination's precision and profile. */
  SkImageInfo bakeInfo(SkISize dimensions) const {
    return detail::pixelBakeInfo(outputInfo, dimensions);
  }
  /** Whether any node in the tree declares a canvas-relative length, and the
   *  canvas those lengths were last resolved against. A resize is the only
   *  thing that moves them, and it moves every one of them at once. */
  bool anyCanvasLengths = false;
  SkSize canvasLengthsAt = SkSize::MakeEmpty();
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
  bool liveOnly = false;  // snapshot(): skip per-node caches
  // A pixel bake holds what was painted as pixels, which a surface map
  // canvas cannot tell lit content in from content that is its own colour,
  // so a page painted as a map keeps recordings and takes no pixel bake.
  bool bakesPixels() const { return !liveOnly && !surfaceMap; }

  // ---- what a draw changes (Changes.cpp) ----
  void changedUnder(detail::Instance& inst, ChangeWalk& walk,
                    std::vector<SkRect>& region, SkRect& extent);
  /** The rects the next draw changes, in canvas units, none of them
   *  touching another; none when it is the whole canvas. With @p commit
   *  the extents are kept. */
  std::optional<std::vector<SkRect>> changedRegion(bool commit);
  /** Layout, the cascade and the volatility walk, as a draw runs them. */
  void settleForDraw();

  // ---- the pixel bakes' surfaces ----
  /** The device or raster layer that receives this canvas's commands. */
  SkCanvas& paintDestination(SkCanvas& canvas) const;
  /** A surface a local bake is taken into: on the device the canvas's paint
   *  reaches when that is a device, a bake inside a recording included, and
   *  otherwise where the canvas makes one, or a raster. */
  sk_sp<SkSurface> localBakeSurface(SkCanvas& canvas,
                                    const SkImageInfo& info) const;
  SkSurface* bakeSurface(SkCanvas& canvas, SkISize need);

  Impl(motion::Engine& t, sigil::weave::FontContext& f)
      : engine(t), fonts(f), reconciler(*this) {
    yogaConfig = YGConfigNew();
  }
  ~Impl() {
    root.reset();
    YGConfigFree(yogaConfig);
  }

  double elapsed() const { return engine.elapsed().count(); }

  // ---- the reconciler's host (ReconcileHost.cpp) ----
  // The ReconcileHost operations, in the reconciler's terms. Reading a
  // description is DescriptionReading's; the children a node reconciles:
  ChildRange children(const detail::Instance& inst,
                      const Description& description) const;
  // Acting on an instance:
  /** A fresh instance for @p node under @p parent, patched once. @p ordinal
   *  is its order among the children created in the same patch and @p count
   *  the parent's child count: the place a staggered tween on it resolves
   *  against. */
  std::unique_ptr<detail::Instance> create(const Description& node,
                                           detail::Instance* parent,
                                           size_t ordinal, size_t count);
  /** Everything the composer does to an instance whose description changed:
   *  @p prev is null on the first patch. */
  void onPatched(detail::Instance& inst, const detail::ElementNode* prev,
                 const detail::ElementNode& next);
  /** Sorts the paint order and reattaches Yoga children when
   *  @p structureChanged — a child mounted, unmounted or moved — then
   *  dirties the parent. */
  void reorder(detail::Instance& parent, bool structureChanged);
  /** Whether a surviving @p match must be unmounted and created afresh under
   *  @p parent rather than patched in place. */
  bool remountRequired(const detail::Instance& match,
                       const detail::Instance& parent);
  /** Marks the instance's paint dirty up to the root and the content dirty. */
  void invalidate(detail::Instance& inst);

  // ---- exits (Exits.cpp) ----
  /** WHAT THE LAST RECONCILE OF @p parent's CHILDREN MEANS FOR ITS EXITS:
   *  a leaving child the description names again turns round, and a child
   *  it no longer names, kept by children(), begins its exit — the
   *  children beginning together as one run played backwards. A child
   *  with nothing to play backwards is moved out into @p dropped, for the
   *  caller to free once the layout no longer holds it. Answers whether
   *  the list of children changed. */
  bool settleExits(detail::Instance& parent,
                   std::vector<std::unique_ptr<detail::Instance>>& dropped);
  /** Drops every leaving node whose exit has landed, reattaching its
   *  parent's children. Answers whether anything was dropped. */
  bool dropLandedExits(detail::Instance& parent);
  /** An instance that left the tree: retired at once — its destructor
   *  frees its Yoga node and its motions disconnect with their outputs. */
  void destroy(std::unique_ptr<detail::Instance> inst, uint64_t frame);
  /** The key index and the edge store, rebuilt over the whole tree after
   *  every reconcile (Reconcile.cpp). */
  void rebuildKeyIndex();
  void applyLayoutProps(detail::Instance& inst);
  /** @p length in pixels for @p inst: a pixel as it stands, a relative
   *  unit against the font in force at the node, a custom property
   *  looked up and resolved the same way. A percent answers itself and
   *  is the caller's to hand Yoga as one; @p relative is raised when the
   *  answer depended on the font or on a property.
   *
   *  A SUM's percentage term is measured against @p percentBasis, the
   *  extent the calling property's percentages are of. A caller with no
   *  such extent leaves it unstated, and a sum holding a percentage then
   *  answers its other terms alone and says so once. */
  float resolveLength(
      const detail::Instance& inst, const Dimension& length, bool& relative,
      float percentBasis = std::numeric_limits<float>::quiet_NaN()) const;
  /** textIndent's @p indent resolved at @p inst: pixels written into
   *  @p block's first-line indent, or a percentage of the measure kept in
   *  @p percent for the layout, a custom property read once on the way. */
  void resolveTextIndent(const detail::Instance& inst, const Dimension& indent,
                         sigil::weave::ParagraphBlock& block,
                         std::optional<float>& percent) const;
  /** Writes every canvas-relative length in the subtree into its flex style
   *  again, for a canvas that has changed size. */
  void reapplyCanvasLengths(detail::Instance& inst);

  // ---- the cascade (Cascade.cpp) ----
  /** Resolves the font, the ink and the custom properties in force at
   *  every node, top-down from the root's, and applies what changed: an
   *  inheriting text leaf is re-materialised or repainted, a node with
   *  relative lengths has its style rewritten, and everything whose
   *  colour moved is marked to repaint. Runs before layout whenever
   *  `cascadeDirty` says the answers may have moved. */
  void runCascade();
  void resolveCascade(detail::Instance& inst,
                      const sigil::weave::Type& parentFont,
                      float parentLineHeight,
                      const std::shared_ptr<const VarTable>& parentVars,
                      const sigil::weave::ParagraphBlock& parentBlock,
                      const std::optional<SkSamplingOptions>& parentSampling,
                      const detail::SheetChain& parentSheets,
                      const detail::InkInForce& parentInkPaint,
                      const std::optional<material::Color>& parentInkTarget);
  /** WHAT THE SHEETS IN FORCE SAY ABOUT EACH NAME @p leaf's rich runs and
   *  paragraph styles were written with, each matched as a virtual child
   *  of the leaf and folded weakest first: the font partial and the ink a
   *  run named so is set in, with the face a family or an italic the rules
   *  state chooses, and the block partial a paragraph named so is laid out
   *  in, with an indent in another unit resolved at the leaf. A name no
   *  rule speaks about is left out (NamedStyles.cpp). */
  void namedStylesOf(const detail::Instance& leaf,
                     const detail::SheetChain& chain,
                     sigil::weave::TypeSheet& runs,
                     std::vector<detail::NamedParagraphStyle>& blocks) const;
  /** An inheriting text leaf whose ink alone changed: the new colour set
   *  on its inherited ranges in place, the restyles replayed over them,
   *  and nothing re-shaped or re-broken. */
  void refreshInheritedInk(detail::Instance& inst);
  /** THE STYLE A LEAF'S TEXT IS SET IN before any restyle or reading is
   *  laid over it: the font in force for an inheriting leaf — the root's
   *  before the cascade has run — the base of a rich passage, or the whole
   *  style a leaf was written with. What a partial restyle, a reading's
   *  partial and a nested style overlay. */
  [[nodiscard]] sigil::weave::TextStyle leafStyle(
      const detail::Instance& inst) const;
  /** WHAT A SPAN LAYS OVER THE RANGE IT FINDS: its partial over @p base,
   *  with the ink it reads from a custom property in force at @p inst and
   *  the shader it states as the glyphs' own. */
  [[nodiscard]] sigil::weave::TextStyle styleOfSpan(
      const sigil::weave::TextStyle& base, const detail::SpanRestyle& span,
      const detail::Instance& inst) const;
  /** The face's own line height at @p font, px. */
  float lineHeightAt(const sigil::weave::Type& font);
  /** The advance of "0" in @p font's face at its size, px. */
  float zeroAdvanceAt(const sigil::weave::Type& font);
  /** Builds the instance's Paragraph from whichever content form its
   *  description carries — plain utf8, `weave::rich()` runs, or a copy of a
   *  supplied Paragraph — and then applies the span restyles in
   *  declaration order. @p lines is the geometry a previous layout
   *  produced, which is what a `weave::selectors::line` restyle addresses;
   * empty leaves those selectors unresolved. @p columns carries the same
   *  geometry for a vertical passage, where a line IS a column. */
  void materializeText(
      detail::Instance& inst,
      std::span<const sigil::weave::LineMetrics> lines = {},
      std::span<const sigil::weave::ColumnMetrics> columns = {});
  /** The options a text node actually lays out under: the full-control
   *  overload's value where it has one, with every field a fluent setter
   *  named written over it. */
  sigil::weave::ParagraphLayoutOptions textLayoutOptions(
      const detail::Instance& inst, float measure) const;

  // ---- transitions (Transitions.cpp) ----
  /** Every lane of @p styled, Slot lanes first in kSlotSpecs order, then
   *  the Span, Gate and Track families in declaration order. The overload
   *  writing into @p out refills a caller-owned vector. */
  std::vector<detail::Lane> lanes(detail::StyledNode styled);
  void lanes(detail::StyledNode styled, std::vector<detail::Lane>& out);
  /** Retargets every PROPERTY lane of @p inst — the slot table's rows and
   *  the fill's synthesized progress — from the style it stood in onto the
   *  one it stands in now, which is the instance's own. Called wherever
   *  the computed style moves: at the patch for a node whose description
   *  changed, and in the cascade pass for one whose answer came from
   *  above. @p prevLanes and @p nextLanes are the caller's scratch, left
   *  holding the two sides so a caller with positional families to
   *  retarget walks each description only once; a lane points INTO its
   *  description, so neither list may outlive @p prev. */
  void retargetProperties(detail::Instance& inst, detail::StyledNode prev,
                          std::vector<detail::Lane>& prevLanes,
                          std::vector<detail::Lane>& nextLanes);
  /** THE INK LANE. @p target is where the node's colour is headed: the
   *  colour its own fold resolved, or for a node that inherits, the
   *  target its parent is headed for. It becomes the target in force; the
   *  lane eases from the target that stood before it, under
   *  @p nodeTransition, and stands down where there is none. An unset
   *  @p target is a node that resolves no colour and runs no lane.
   *  @p recordOnly keeps the target and starts nothing, for a node
   *  resolving its colour for the first time, which has none to ease
   *  from. */
  void retargetInk(detail::Instance& inst,
                   const std::optional<material::Color>& target,
                   const std::optional<motion::Tween<float>>& nodeTransition,
                   bool recordOnly);
  /** A patch: the property lanes, then the positional families a changed
   *  description brings. */
  void applyTransitions(detail::Instance& inst, detail::StyledNode prev);
  void applyMountTransitions(detail::Instance& inst);
  /** Whether some leaf mounted a track whose progress waits for its span,
   *  as `Instance::trackSpansOwedSince` records. */
  bool trackSpansOwed = false;
  /** Starts every progress waiting for its span in @p inst's subtree whose
   *  text is laid out, caught up to the time since its leaf mounted.
   *  Answers whether any still waits. */
  bool startTrackSpans(detail::Instance& inst);
  /** The span track @p trackIndex of @p inst maps its progress onto, as
   *  the text engine resolves it against the layout in force; zero where
   *  the track runs nothing. */
  motion::Duration trackSpanOf(detail::Instance& inst, size_t trackIndex);
  /** ONE ENTRANCE IN A LEAVING SUBTREE: the motion that holds it, the
   *  description's value it was declared on (null for the fill's own
   *  progress), the entrance resolved where its node stands, and when that
   *  entrance ends, counted from the mount. */
  struct EntranceRun {
    std::unique_ptr<detail::HeldMotion>* held = nullptr;
    const motion::Animatable<float>* value = nullptr;
    motion::Tween<float> entrance;
    motion::Duration end{};
  };
  /** Every entrance declared in @p inst's subtree, @p inst's own resolved
   *  at @p place and each descendant's at the place it mounted in; a
   *  descendant that is leaving on its own is left to its own exit. */
  void entrancesUnder(detail::Instance& inst, motion::Place place,
                      std::vector<EntranceRun>& out);
  /** Plays @p run backwards from where its value stands, after @p lead.
   *  Answers whether a motion is writing it. */
  bool playBackwards(const EntranceRun& run, motion::Duration lead);
  /** Plays @p run forwards from where its value stands, at once. */
  void playForwards(const EntranceRun& run);

  // ---- volatility & caching (Volatility.cpp) ----
  void scanReleasedScalars();  // defined in Volatility.cpp beside the memos
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
  /** The picture tier's seam value: the kernel decides bake, replay or
   *  live, and these are the operations that carry the decision out. */
  core::Bake<PictureBakeTarget> pictureBake{PictureBake{}};
  /** Record the node's own paint into a replayable picture, freezing the
   *  leaf blend and opacity into it and stamping the values it was
   *  recorded from. The bake half of the picture tier. */
  void recordPicture(detail::Instance& inst, SkCanvas& destination,
                     const SkMatrix& deviceMatrix, const SkIRect& deviceClip,
                     bool matrixStable, float hostScale, SkBlendMode leafBlend,
                     float leafOpacity,
                     detail::Instance::ContentScalars&& scalars);

  // ---- layout (Layout.cpp) ----
  /** Temporary phase records: authored flex inputs and the preceding
   *  placement are kept separate while an arrangement writes its output. */
  struct PlacedChild {
    detail::Instance* instance = nullptr;
    SkRect input = SkRect::MakeEmpty();
    SkRect previous = SkRect::MakeEmpty();
    bool wasAbsolute = false;
  };
  void calculateYoga();
  void applyCustomLayouts(detail::Instance& inst,
                          std::span<PlacedChild> placed);
  SkSize minimumSizeOf(detail::Instance& child);
  bool applyCenterPins(detail::Instance& inst);
  /** The passes, as the runner sees them. Each returns whether it changed
   *  geometry; the non-converging ones answer false. */
  bool phaseYoga();           ///< initial resolved geometry
  bool phaseCustomLayouts();  ///< custom layout() containers, when any
  bool phaseCenterPins();     ///< centerAt() pins, when any
  bool phaseDerive();         ///< flow exclusions and routes, when any
  bool phasePathMarks();      ///< textAttach() on path-laid runs
  // ---- the additions (Additions.cpp) ----
  /** THE ADDING OPERATORS OVER THE SETTLED TREE: every node applying one
   *  is handed its scope, what the operators attach is reconciled beside
   *  the owner's authored children, and true says something changed and
   *  the layout must run again with the additions standing. */
  bool phaseAdditions();
  /** Fills @p scope with every node under @p from as an adding operator
   *  sees it — closed at a node with operators of its own, blind to a
   *  node an operator added — with the owner instance of each record in
   *  @p owners at the same index. */
  void collectScope(detail::Instance& from, SkPoint origin, Scope& scope,
                    std::vector<detail::Instance*>& owners);
  bool phaseSyncRects();  ///< invalidate recordings whose rect moved
  /** The runner's list: Yoga, the converging group, then the post-layout
   *  passes. The derive family (connector, rail, band, contentFlowAround)
   * reaches the schedule ONLY as the `derive` entry of the converging group —
   * the registration IS its seam — and the runner's settle step re-runs it
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
  /** AN EXTENT THAT DOES NOT BOUND: the depth a leaf that grows down the
   *  page is laid out in, and the measure an unconstrained one is measured
   *  at. One value rather than a large number spelled per call site,
   *  because it is also the cache key a layout is held valid for — a
   *  second spelling a hair away from this one re-lays the passage out
   *  every frame and nothing says so. */
  static constexpr float kUnbounded = 1.0e6f;
  /** Lays the node's text out inside @p constraint px across and
   *  @p downConstraint px down — the CONTENT box, which is what Yoga's
   *  measure callback is handed. A horizontal passage reads the first as
   *  its measure and ignores the second; a vertical one reads the first as
   *  where its rightmost column stands and the second as how far a column
   *  may run before the next one starts. */
  void layoutText(detail::Instance& inst, float constraint,
                  float downConstraint = kUnbounded);
  /** The same, for a caller holding the node's BOX rather than the room
   *  inside it — every caller outside the measure callback. The node's own
   *  padding is the difference. */
  void layoutTextInBox(detail::Instance& inst, float boxWidth,
                       float boxHeight = kUnbounded);
  /** THE NODE'S OWN PADDING IN PIXELS — the inset from its box to the
   *  content box its children, and its own paragraph, are laid out in.
   *  Yoga resolves it for a node in the flex world, percents against the
   *  container included; a node in a positioned subtree carries no Yoga
   *  node and its lengths are resolved the way its rect is. */
  detail::Insets paddingOf(const detail::Instance& inst) const;
  SkRect instanceRect(const detail::Instance& inst) const;
  SkRect positionedRect(const detail::Instance& inst) const;
  SkRect absoluteRect(const detail::Instance& inst) const;

  // ---- derive (Derive.cpp) ----
  /** One pass over the flat flow and borrow lists — no tree recursion.
   *  Returns true when a text exclusion changed (second layout pass
   *  needed). */
  bool resolveDerived();
  bool deriveFlow(detail::Instance& inst);
  /** Walks every frame chain in order, handing each frame the cursor the
   *  one before it left. True when a cursor moved. */
  bool resolveThreads();
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
  /** Everything one node borrows off another's settled geometry: the
   *  boxes a span gate is sized from and the paths a decoration reads. */
  void deriveBorrows(detail::Instance& inst);

  // ---- the node's paint transform, resolved once (Bounds.cpp) ----
  /** The node's lanes for THIS frame (NodeTransform.h), resolved once so
   *  paint's matrix, recordBounds's child union and hitInstance's inverse
   *  all describe the same matrix. */
  NodeTransform transformOf(detail::Instance& inst);
  /** An origin written as lengths, resolved for @p inst: a percentage as
   *  a fraction of its box, every other unit as pixels against its font,
   *  its custom properties and the canvas. @p z is the depth, null where
   *  the origin has none. */
  detail::Pivot pivotOf(const detail::Instance& inst, const Dimension& x,
                        const Dimension& y, const Dimension* z) const;

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
  /** Where on its motion path this node sits, in its PARENT's space, and
   *  the auto-orient angle in degrees. Nullopt when no path is engaged
   *  (absent, empty, or resolving to no measurable length) — the
   *  translate lanes then stand. Rebuilds the instance's arc-length table
   *  when the Shape value or the parent size no longer matches. */
  std::optional<std::pair<SkPoint, float>> motionPathSample(
      detail::Instance& inst, const SkSize& frame);

  // ---- paint (StackingPainter.cpp and the paint-phase files beside it) ----
  void paint(detail::Instance& inst, SkCanvas& canvas);
  /** The ink a text leaf's glyphs are painted with this draw: the
   *  glyph-paint override ink(paint)/textStroke() ask for, and the paint an
   *  ink restarting per unit lays on each unit. ONE body, read by the
   *  resting draw and by the textFx() draw — a letter in flight is painted
   *  exactly as a resting one is. */
  detail::TextInk textInkOf(detail::Instance& inst,
                            const PaintContext& paintCtx);
  /** How much of a node's paint to emit.
   *
   *  The node's own paint is a CONTIGUOUS PREFIX of paintContent —
   *  backgrounds, clip, fill, echoes, overlays, leaf content — ending
   *  exactly at the children loop. A coverage trace of the node's own paint
   *  alone emits that prefix and skips the children and the foregrounds
   *  painted over them. */
  enum class Phase : uint8_t {
    All,      ///< the whole node, unchanged
    OwnOnly,  ///< the prefix: no children, no foregrounds
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
  /** The raster trace itself: @p phase of @p inst's paint over @p covers
   *  at @p scale, cut at @p threshold, as a path in the node's own space. */
  SkPath traceCoverage(detail::Instance& inst, const SkRect& covers,
                       float scale, float contentScale, float threshold,
                       Phase phase);
  /** A silhouette composed from held parts — the node's own paint traced
   *  alone, and each child's own silhouette placed by its transform — for
   *  a node whose children move rigidly and change nothing else; nothing
   *  where that does not hold. */
  std::optional<SkPath> composedCoverage(detail::Instance& inst,
                                         const SkRect& covers, float scale,
                                         float contentScale, float threshold);
  /** WHAT A NODE SAYS ITS EDGE IS, in its own space — its glyph outlines
   *  under `Boundary::Glyphs`, the silhouette of what it drew under
   *  `Boundary::Coverage`, its declared shape otherwise. Empty when the
   *  node declares no silhouette at all and its box is the whole answer.
   *
   *  One reading, for the node's own decorations and for anything that
   *  borrows its edge, so a node cannot be dressed along one outline and
   *  flowed around along another. */
  SkPath boundaryOutlineOf(detail::Instance& target, float width, float height);
  /** What the node paints BY ITSELF, in its own local space: its box grown
   *  by every decoration's declared bleed, any routed path and the shape it
   *  declares, and NOTHING from its children: what a coverage trace of the
   *  node's own paint and the repaint region read, which must not move
   *  when only a child does. */
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
