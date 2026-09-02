#pragma once

/** @file
 * Internal to SigilCompose — the element description payload shared by
 * the element builders (Element.cpp and its siblings) and the reconciler.
 */

#include <sigilcompose/core/Material.h>
#include <sigilcore/compute/Noise.h>
#include <sigilcore/reconcile/Memo.h>
#include <sigilcore/reconcile/Reads.h>
#include <sigilmotion/values/Animated.h>
#include <sigilweave/paragraph/Paragraph.h>

#include <array>
#include <vector>

#include "sigilcompose/Compose.h"

namespace sigil::compose::detail {

enum class Kind : uint8_t { Box, Stack, Text, Image, Custom, Slot };

struct EdgeValues {
  float left = 0, top = 0, right = 0, bottom = 0;
  bool operator==(const EdgeValues&) const = default;
};

/** Per-edge Dims for absolute insets: Auto = that side is unpinned. */
struct EdgeDims {
  Dim left, top, right, bottom;
  bool operator==(const EdgeDims&) const = default;
};

struct LayoutProps {
  bool row = false;
  bool wrap = false;
  float gap = 0;
  EdgeValues padding, margin;
  Dim width, height, minWidth, maxWidth, minHeight, maxHeight, basis;
  float aspect = 0;
  float grow = 0, shrink = 1;
  Align alignItems = Align::Stretch;
  Align alignSelf = Align::Auto;
  Justify justify = Justify::Start;
  bool absolute = false;
  bool hasInsets = false;
  /** positioned() container: children (and their subtrees) get NO Yoga
   *  nodes; instanceRect() resolves their rects straight from these
   *  props. */
  bool positioned = false;
  EdgeDims insets;
  std::optional<SkPoint> centerAt;  // absolute: center ON this point
                                    // (resolved post-measure)
  bool operator==(const LayoutProps&) const = default;
};

struct PaintProps {
  std::optional<Animatable<Fill>> fill;
  Animatable<float> opacity = 1.0f;
  SkBlendMode blendMode = SkBlendMode::kSrcOver;
  Animatable<float> translateX = 0.0f, translateY = 0.0f;
  Animatable<float> rotate = 0.0f, scale = 1.0f;
  // Per-axis scale, multiplied INTO `scale`. Bars, wipes, meters,
  // cooldown sweeps and drain rings are the most common animated
  // primitive in a UI and none of them are uniform.
  Animatable<float> scaleX = 1.0f, scaleY = 1.0f;
  Animatable<float> skewX = 0.0f, skewY = 0.0f;  // degrees (shear)
  float originX = 0.5f, originY = 0.5f;
  bool originPx = false;  // origin in node-local px instead of fractions
  int zIndex = 0;
};

/** Value-semantic heap box for ElementNode's rare-field blocks: absent
 *  costs one null pointer; copying deep-copies a present block (the COW
 *  clone in Element::NodeHandle::operator-> relies on ElementNode's
 *  defaulted copy constructor). ensure() is the builder-side entry. */
template <class T>
class Box {
 public:
  Box() = default;
  Box(const Box& other)
      : m_ptr(other.m_ptr ? std::make_unique<T>(*other.m_ptr) : nullptr) {}
  Box(Box&&) noexcept = default;
  Box& operator=(const Box& other) {
    m_ptr = other.m_ptr ? std::make_unique<T>(*other.m_ptr) : nullptr;
    return *this;
  }
  Box& operator=(Box&&) noexcept = default;

  explicit operator bool() const { return m_ptr != nullptr; }
  T* operator->() { return m_ptr.get(); }
  const T* operator->() const { return m_ptr.get(); }
  T& operator*() { return *m_ptr; }
  const T& operator*() const { return *m_ptr; }
  T& ensure() {
    if (!m_ptr) m_ptr = std::make_unique<T>();
    return *m_ptr;
  }

 private:
  std::unique_ptr<T> m_ptr;
};

// ---- ElementNode blocks: rare/kind-specific fields live out-of-line so a
// plain box costs a fraction of what one flat struct would, and each phase's
// inputs are visible in the type. HOT fields every kind touches
// (layout/paint/corners/decorations/children) stay inline. Composer.cpp
// caps sizeof(ElementNode) with a static assertion, which is the rule that
// keeps a rare field from being added inline.

/** One Element::spanPaint() / Element::spanStyle() declaration: a selector
 *  and what it does to the range it finds. Ordered — later declarations win
 *  on overlap — and comparable, so a re-described list prunes. */
struct SpanRestyle {
  Selector where;
  sigil::weave::TextStyle style;  ///< paintOnly reads `style.paint` alone
  /** setPaint (never re-shapes) rather than setStyle. */
  bool paintOnly = false;
  bool operator==(const SpanRestyle& other) const {
    return where == other.where && paintOnly == other.paintOnly &&
           (paintOnly ? style.paint == other.style.paint
                      : style == other.style);
  }
};

/** One Element::mark() declaration: which unit the mark anchors to, and the
 *  key of the child that anchors there. The child itself is an ordinary
 *  child of the text node — this is only the back-index the layout reads to
 *  learn which rect that child's box is. Comparable, so a re-described mark
 *  list prunes. */
struct MarkAnchor {
  Selector where;
  std::string key;
  bool operator==(const MarkAnchor&) const = default;
};

/** The Element layout-option setters, as a comparable value plus a record
 *  of WHICH of them the description actually set.
 *
 *  The mask is what makes the setters override a full-control overload's
 *  ParagraphLayoutOptions field by field: an unset field leaves whatever
 *  the caller passed alone, and there is no way to tell "never asked for"
 *  from "asked for the default value" without it. */
struct TextOptions {
  enum Field : uint16_t {
    kAlignment = 1u << 0u,
    kLineBreak = 1u << 1u,
    kHyphenation = 1u << 2u,
    kEllipsis = 1u << 3u,
    kMaxLines = 1u << 4u,
    kLastLine = 1u << 5u,
    kWritingMode = 1u << 6u,
    kBlocks = 1u << 7u,
    kFrame = 1u << 8u,
    kJustification = 1u << 9u,
    kTabStops = 1u << 10u,
    kLive = 1u << 11u,
    kLineTables = 1u << 12u,
    kReserved = 1u << 13u,
    kLineBreakLocale = 1u << 14u,
  };
  uint16_t set = 0;  ///< which fields below were written

  sigil::weave::TextAlignment alignment = sigil::weave::TextAlignment::kStart;
  /// Not a ParagraphLayoutOptions field — the writing mode belongs to the
  /// Paragraph, so applyTo() cannot carry it and materializeText writes it
  /// on the materialized paragraph instead. The mask rule is the same: an
  /// unset mode leaves a passed-in paragraph's own mode alone.
  sigil::weave::WritingMode writingMode =
      sigil::weave::WritingMode::kHorizontal;
  sigil::weave::LineBreakStrategy lineBreak =
      sigil::weave::LineBreakStrategy::kGreedy;
  sigil::weave::HyphenationOptions hyphenation;
  std::u16string ellipsis;
  int maxLines = 0;
  sigil::weave::TextAlignment lastLineAlignment =
      sigil::weave::TextAlignment::kStart;
  bool justifyLastLine = false;
  /// paragraphs(): one entry per BLOCK — the text between two hard breaks
  /// — in block order. A block past the end of the list is set by the
  /// layout-wide fields alone, so one style here sets the first block and
  /// leaves the rest plain, which is what a heading over a body wants.
  std::vector<sigil::weave::ParagraphStyle> blocks;
  sigil::weave::FrameOptions frame;
  sigil::weave::JustificationOptions justification;
  sigil::weave::TabStopOptions tabStops;
  /// live(): this layout is one of a run of them. The budget rides with it
  /// because they are one statement — a text that says it is moving is the
  /// only one for which running out of time is a normal event.
  bool live = false;
  float budgetMicroseconds = 0;
  /// The three tables a house's own setting is stated in, and the fraction
  /// beside the third. One mask bit for all four: they are the same
  /// declaration made in four places, and a caller who sets one and expects
  /// a full-control overload's others to survive has no way to say so.
  sigil::weave::KinsokuTable kinsoku;
  sigil::weave::HangingTable hanging;
  sigil::weave::MojikumiTable mojikumi;
  float tsume = 0;
  /// reserve(): room beside every line of this passage, on top of whatever
  /// an annotation reserves.
  sigil::weave::ReservedBand reserved;
  /// Not a ParagraphLayoutOptions field either — the line-break tailoring
  /// belongs to the Paragraph, for the same reason the writing mode does,
  /// and materializeText writes it there under the same mask rule.
  std::string lineBreakLocale;

  /** Writes every SET field over @p options, leaving the rest alone. */
  void applyTo(sigil::weave::ParagraphLayoutOptions& options) const;

  bool operator==(const TextOptions& other) const {
    return set == other.set && alignment == other.alignment &&
           writingMode == other.writingMode && lineBreak == other.lineBreak &&
           hyphenation == other.hyphenation && ellipsis == other.ellipsis &&
           maxLines == other.maxLines &&
           lastLineAlignment == other.lastLineAlignment &&
           justifyLastLine == other.justifyLastLine && blocks == other.blocks &&
           frame == other.frame && justification == other.justification &&
           tabStops == other.tabStops && live == other.live &&
           budgetMicroseconds == other.budgetMicroseconds &&
           kinsoku == other.kinsoku && hanging == other.hanging &&
           mojikumi == other.mojikumi && tsume == other.tsume &&
           reserved == other.reserved &&
           lineBreakLocale == other.lineBreakLocale;
  }
};

struct TextData {
  // Element::textStroke(): a stroke pass on the GLYPHS, under the fill.
  bool hasTextStroke = false;
  float textStrokeWidth = 0.0f;
  Fill textStrokeFill;
  std::u8string utf8;
  sigil::weave::TextStyle style;
  // text(RichText): several runs, several styles, one comparable value. Empty
  // on every other content form.
  RichText rich;
  // Full-control overload: identity (the pointer) is the change signal.
  std::shared_ptr<sigil::weave::Paragraph> paragraphOverride;
  sigil::weave::ParagraphLayoutOptions layoutOptions;
  // The fluent setters (textAlign, lineBreak, hyphenation, ellipsis,
  // maxLines, lastLine), which override `layoutOptions` field by field.
  TextOptions options;
  // spanPaint()/spanStyle(): the type treatment, addressed by selector and
  // applied to the materialized paragraph in declaration order.
  std::vector<SpanRestyle> spanRestyles;
  // Element::fx(): the ordered track list. Empty on ordinary text.
  // Element::variationDrive() appends one of these too — a driven axis is a
  // per-glyph deviation like any other, and has no plumbing of its own.
  std::vector<Track> tracks;
  // textFill(): glyph paint in text-metric space (unit square → cap band).
  // Resolved at paint from the line metrics; live materials re-resolve per
  // frame; static ones compare by recipe for the prune.
  std::optional<Material> metricFill;
  // onPath(): the run's baseline IS a path. Resolved at paint against the
  // node's box, walked with SkContourMeasure, one RSXform per glyph.
  std::optional<TextPath> onPath;
  // mark(): a child of this text node whose box is the rect a selector
  // resolves to, in declaration order. The rects themselves live on the
  // Instance (textMarkRects) because they are an answer of the layout.
  std::vector<MarkAnchor> marks;
  // annotate(): readings set beside the type, in declaration order. A
  // reserving one is a LAYOUT INPUT — its band reaches the strut before the
  // text is broken — and the placed readings live on the Instance, because
  // where each one landed is an answer of the layout.
  std::vector<Annotation> annotations;
  // thread(): the key of the frame this one fills INTO. A chain of frames
  // over one story; the cursor each frame starts at lives on the Instance,
  // because where a fill stopped is an answer of the layout.
  std::string threadTo;
  // THE TEXT ENGINE, as the description carries it: installed by the verbs
  // that dress type (fx, onPath, mark, spanStyle, spanPaint,
  // variationDrive), read by the kernel wherever it needs more than the
  // paragraph drawn at rest. Excluded from structural equality — it is the
  // same engine on every text that has one — so a field pin names it and
  // the comparator skips it.
  TextPainter painter;

  /** The alignment this leaf actually lays out under: `textAlign()`'s value
   *  where it was written, otherwise whatever the full-control overload's
   *  options carry. */
  sigil::weave::TextAlignment alignment() const {
    return (options.set & TextOptions::kAlignment) ? options.alignment
                                                   : layoutOptions.alignment;
  }
};

struct ImageData {
  std::shared_ptr<const sigil::image::ImageAsset> asset;
  std::optional<SkRect> region;  // atlas sub-rect, source px
  // Element::sampling(). Linear by default; the reason this is settable per
  // node is that pixel art, tilemaps and simulation buffers need nearest,
  // and drawing them through a linear filter blurs them with no diagnostic.
  SkSamplingOptions sampling{SkFilterMode::kLinear};
};

struct CustomData {
  PaintProgram program;
  // custom(key, program): the program's declared identity. A callable
  // cannot be compared, so an empty key means the node is conservatively
  // unequal to every other and never prunes.
  std::string key;
};

struct DeriveData {
  // Custom layout (layout() containers)
  std::function<std::vector<SkRect>(const LayoutInput&)> placeFn;
  std::vector<std::string> flowAroundKeys;
  float flowAroundMargin = 0;
  std::string connectFrom, connectTo;
  Router router;
  /** connector()'s terminal gap: px pulled back along the routed path at
   *  EACH end, Anchor::gap's clamp (never more than 45% of the route per
   *  end, so a short wire keeps a visible run). */
  float connectorGap = 0.0f;
  std::vector<Anchor> railAnchors;  // rail(): ordered waypoints
  RailRouter railRouter;
  // band(): the spine is guide DATA — either authored here, or borrowed
  // from a keyed element's resolved shape by the derive pass. The width
  // profile's presence is what makes this node a band. A Shape, so a
  // comparable spine prunes (same seam as shapeFn).
  Shape bandSpine;
  std::string bandAround;
  std::optional<Across> bandWidth;
  Formation bandFormation = Formation::Centered;
  // spans::fit(key): the keyed boxes a stroke pass sizes its gap from,
  // resolved to this node's local space per frame (the flowAround
  // pattern applied to a boundary). Declared here rather than beside the
  // passes so the ONE derive registration walk sees them.
  std::vector<std::string> spanFitKeys;
  // strand::from(key): the keyed PATHS a decoration borrows, declared by
  // the decoration itself (BorrowingDecoration) because a Decoration is
  // type-erased by the time the element holds it. Kept apart from
  // spanFitKeys on purpose: a rect borrow is one absoluteRect() call, a
  // path borrow re-evaluates the target's shape generator, so the two
  // costs are not paid for each other.
  std::vector<std::string> borrowedPathKeys;
  /** WHAT THIS NODE READS OFF ANOTHER, declared where the derivation is
   *  WRITTEN — one entry per keyed node this one's answer is a function
   *  of, and which facet of that node it needs.
   *
   *  Every field above says what a pass will RESOLVE; this says what the
   *  node DEPENDS ON, and the two are different questions with different
   *  readers. The resolve pass reads the fields, because it needs the
   *  margin, the gap, the router and the formation beside each key. The
   *  ordering reads this, because all it needs is the edges — and reading
   *  them here rather than reconstructing them from which fields happen to
   *  be non-empty is what keeps a new derivation from being ordered a pass
   *  behind by a chain that never heard of it. A verb that borrows a key
   *  adds its read in the same statement that stores the key.
   *
   *  A frame chain declares its read here too, though its key lives in
   *  TextData: a node has ONE list of what it reads, whatever block holds
   *  the data behind it.
   *
   *  Excluded from structural equality on purpose — every entry is a
   *  function of fields the comparator already compares, so a description
   *  that differs in a read differs in the field that produced it. */
  std::vector<sigil::core::Read> reads;
};

/** One span-qualified pass — Element::stroke(where, what, name) or
 *  Element::background(where, what, name). The unqualified whole-boundary
 *  forms stay ordinary foregrounds/backgrounds: they overlay and never
 *  CLAIM part of the boundary, so they can never trip the no-overlap
 *  diagnostic below.
 *
 *  ONE list of passes, two z-halves. `half` says only WHERE the pass
 *  paints; claims, the no-overlap law, append order and rest() all read the
 *  whole list, because they are statements about ONE boundary and a
 *  boundary does not have two of itself. */
struct StrokePass {
  enum class Half : uint8_t {
    Background,  ///< with the backgrounds, below the fill and the children
    Foreground,  ///< with the foregrounds, above the children
  };
  Spans where;
  Decoration what;
  std::string name;
  Half half = Half::Foreground;
};

struct StrokeData {
  std::vector<StrokePass> passes;
  // The stroke grammar's engine, installed with the first span-qualified
  // pass; excluded from structural equality.
  StrokeResolver resolver;
};

/** Which unqualified mark slot a local label belongs to. Span passes carry
 *  their own `name` on StrokePass; these three lists are the labels the
 *  UNqualified slots grew so `parts::named()` can address them too. */
enum class MarkSlot : uint8_t { Background, Overlay, Foreground };

/** Element::foreground/overlay/background/stroke(d, name): one local label
 *  bound to one mark, by slot and index. A vector of these rather than a
 *  name beside every Decoration, because naming a mark is rare and
 *  Decoration is a hot value in three inline lists. */
struct MarkLabel {
  MarkSlot slot = MarkSlot::Foreground;
  uint32_t index = 0;
  std::string name;
  bool operator==(const MarkLabel&) const = default;
};

struct FxData {
  std::optional<Effect> layerEffect;
  std::optional<Effect> backdropEffect;
  // Misprint echoes (offset flat-color re-stamps under fill/text)
  std::vector<Echo> echoes;
  float staggerChildrenMs = 0;  // extra order·each mount delay per subtree
  motion::Spread::From staggerFrom = motion::Spread::From::Start;
  // Element::overlay(): decorations painted OVER the fill and UNDER the
  // content and children. Lives in this block rather than beside
  // backgrounds/foregrounds so sizeof(ElementNode) does not grow — the
  // rare-fields rule Composer.cpp's static assertion enforces.
  std::vector<Decoration> overlays;
  // Element::mask(): the appearance-gating family, in declaration order.
  // Masks whose selections overlap INTERSECT on the overlap, and each
  // carries its own animation slots (Instance::maskAnims), so three masks
  // on one node may run at three different rates.
  //
  // One vector covers what would otherwise be several fixed field groups:
  // an arc window is `{parts::all(), by::spans(...)}` and a half-plane
  // reveal is `{parts::all(), by::edge(...)}`. That is why the masks live
  // in this existing block rather than in a new one — ElementNode does not
  // grow a pointer for them.
  std::vector<Mask> masks;
  // Local labels for the UNqualified marks (see MarkLabel).
  std::vector<MarkLabel> markNames;
};

struct MaterialData {
  // Live material fill: a Material with a ch::Output-bound uniform, resolved
  // per frame. Supersedes paint.fill when present (a static Material
  // collapses to paint.fill instead). Declares the node volatile.
  std::optional<Material> live;
  // The comparable recipe behind paint.fill when it was set via
  // fill(Material): propsEqual compares this structurally, so a re-described
  // material fill prunes even though each describe minted a fresh shader.
  std::optional<Material> recipe;
};

/** The memo shell's payload: SigilCore's Memo, producing an Element. The
 *  reconciler compares its captured `env` and then its props against the
 *  memo the node was last described from, and runs `invoke` under that
 *  environment on a miss. */
using MemoData = core::Memo<Element>;

struct ElementNode {
  Kind kind = Kind::Box;
  std::string key;
  LayoutProps layout;
  PaintProps paint;
  Corners corners;
  Shape shapeFn;  // custom silhouette; overrides corners. A comparable
                  // scheme prunes; a raw callable never compares equal, so
                  // its node re-patches on every describe.
  // Element::boundary(): what this node's decorations dress — its own
  // shape, or (on a text leaf) the outline of its glyphs.
  Boundary boundary = Boundary::Auto;
  bool clipContent = false;
  // Element::hitTestable(false): the node and its own box are skipped by
  // hitTest, though its CHILDREN are still tested. A keyed full-bleed
  // layout shell with no fill otherwise swallows every hit in the frame,
  // silently and totally.
  bool hitTestable = true;
  Cache cacheMode = Cache::Auto;
  float bakeScale = 1.0f;  // Texture-bake resolution multiplier (see Element)
  std::optional<Transition> nodeTransition;

  // Decoration layers (kernel seam; primitives live in Decorations.h)
  std::vector<Decoration> backgrounds;
  std::vector<Decoration> foregrounds;

  // Rare/kind-specific blocks (see the struct docs above).
  Box<TextData> textData;
  Box<ImageData> imageData;
  Box<CustomData> customData;
  Box<DeriveData> deriveData;
  Box<FxData> fxData;
  Box<MaterialData> materialData;
  Box<StrokeData> strokeData;  // span-qualified stroke passes (rare)
  Box<MemoData> memoData;      // present ⇔ this is a memo shell
  // Element::travel(): the node's position IS a curve. A block rather than
  // a PaintProps field because a Shape plus an Animatable would cost that
  // much on EVERY node in the tree for a property a handful of them use,
  // which is what Composer.cpp's size assertion exists to prevent.
  Box<MotionPath> motionData;

  std::vector<Element> children;

  bool isMemo() const { return (bool)memoData; }
  bool hasMasks() const { return fxData && !fxData->masks.empty(); }
  bool hasStrokePasses() const {
    return strokeData && !strokeData->passes.empty();
  }
  const Across* bandWidth() const {
    return deriveData && deriveData->bandWidth ? &*deriveData->bandWidth
                                               : nullptr;
  }
};

// ---------------------------------------------------------------------------
// FIELD PINS — a field added to a props block is a BUILD FAILURE
//
// THE FAILURE THIS CLOSES IS INVISIBLE BY CONSTRUCTION. `propsEqual()` and
// its helpers compare a description field by field; a field left out makes
// two DIFFERENT descriptions compare EQUAL, so the patch prunes,
// `markPaintDirtyUp()` never runs, a stale picture replays, and
// `applyTransitions()` — which only runs inside the `own` branch — never
// ramps an `animate()` on that property. Nothing errors. No test fails.
// A per-axis scale omitted from `propsEqual` and from `recordBounds()`'s
// transform gate is the shape this takes in practice: the property works
// on first paint and then quietly stops responding.
//
// THE MECHANISM is SigilCore's `kFieldCount<T>`, which reads the number of
// direct non-static data members straight off an aggregate. It is EXACT
// where the obvious alternative is not: a `static_assert(sizeof(T) == N)`
// is walked straight past by a `bool` dropped into tail padding, while a
// field count changes the moment a field does. Nested aggregates count as
// ONE field each, not as their flattened members.
//
// WHAT A PIN COSTS AND WHAT IT BUYS. Adding a field breaks the field-count
// `static_assert` in Reconcile.cpp (rule on it in the comparator —
// participate, or a stated reason not to — then bump the count), and then
// — for the blocks whose fields are all comparable lanes — the field walk
// `EveryPaintPropsFieldParticipatesInEquality` /
// `EveryBoundFloatFieldParticipatesInEquality` picks the new field up
// AUTOMATICALLY and fails until the comparator notices it. Two gates, one
// of them the compiler's.
//
// NO PIN IS NEEDED for a struct whose equality is `= default`
// (LayoutProps, Corners, MarkLabel, MarkAnchor, Echo, Anchor, Across, Parts,
// Span, ContentScalars): the compiler writes the exhaustive comparison
// and cannot forget a field. A pin exists only for a comparator a human
// wrote by hand — and the honest way to retire a pin is to give the struct
// a defaulted `operator==`.
//
// CLASSES WITH PRIVATE STATE (Material, Effect, Region, Animatable, Shape,
// Decoration, Profile) CANNOT be pinned — reading a field count needs an
// aggregate. Their hand-written comparators sit in the same header or
// translation unit as their members, so a field and its comparison are
// read together; PaintProps (here) and propsEqual (Reconcile.cpp) are the
// pair that can drift apart unseen.

using ::sigil::core::kFieldCount;

/** THE STRUCTURAL PRUNE (Reconcile.cpp). Declared here — rather than kept
 *  in Reconcile.cpp's anonymous namespace — so the field-participation
 *  tests can call the comparator DIRECTLY. Inferring a prune from
 *  `stats().patchedNodes` instead requires re-describing the SAME node,
 *  because keyed siblings never prune into one another; a test that
 *  compares two different nodes will report a difference whatever the
 *  comparator does, and so passes even when the field is unread. */
bool propsEqual(const ElementNode& a, const ElementNode& b);
/** The shaped-binding half of the same comparator, SigilMotion's: every
 *  field of BoundFloat participates, under the pin beside its body. */
using ::sigil::motion::boundMapEqual;
/** An Animatable compared where every other animated slot is:
 *  SigilMotion's form-by-form comparator. */
using ::sigil::motion::propEqual;

/** Constant, binding, or transitioned — one animatable flattened. */
using ::sigil::motion::ResolvedProp;
using ::sigil::motion::resolveProp;

// ---------------------------------------------------------------------------
// TEXT FX — the runtime side of the fx() seam (TextFx.cpp)

/** Equal only when PROVABLY identical: two easing curves compare equal when
 *  both are the same plain function pointer, and a lambda-valued curve
 *  compares unequal, conservatively. SigilMotion's one body, so no second
 *  spelling of the rule can let two comparators disagree about whether a
 *  node may prune. */
using ::sigil::motion::easeEqual;
/** Same duration, same delay, same curve under easeEqual. */
using ::sigil::motion::transitionEqual;
/** Did the DESCRIBED transform change between two descriptions? The lanes
 *  mirror propsEqual's transform block plus travel(). Defined in
 *  Reconcile.cpp beside the comparators it is built from. */
bool describedTransformEqual(const ElementNode& a, const ElementNode& b);

/** UTF-8 to the UTF-16 the weave layer speaks. */
std::u16string toUtf16(std::u8string_view utf8);

/** The stateless splitmix64 of one key — the avalanche over the key
 *  offset by the gamma, which is the same mixer `Rng` steps, used to
 *  order units rather than to shape a glyph. */
inline uint64_t mix64Value(uint64_t z) {
  return core::noise::mix64(z + core::noise::kMix64Gamma);
}
/** The once-per-process diagnostic behind `onPath` plus a vertical
 *  `writingMode`: a path run's baseline is its own geometry, so there are
 *  no columns to advance and the path wins. */
void warnWritingModeOnPath();

/** The once-per-name diagnostic behind a paragraph style name that
 *  resolves to nothing — no set in scope, or a set that does not carry it.
 *  A block set in a default nobody asked for is the silent no-op this
 *  library refuses to ship. */
void warnNoSuchParagraphStyle(std::string_view name, bool anySetInScope);

/** Does this selector reach for a LINE, and therefore need a layout to
 *  resolve against? The question the second layout pass is gated on. */
bool selectorNeedsLayout(const Selector& selector);

}  // namespace sigil::compose::detail
