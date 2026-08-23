#pragma once
// Internal to SigilCompose — the element description payload shared by
// the builders (Compose.cpp) and the reconciler (Composer.cpp).

#include <sigilweave/Paragraph.h>

#include <array>
#include <tuple>
#include <utility>
#include <vector>

#include "sigilcompose/Compose.h"
#include "sigilcompose/Material.h"

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
  enum Field : uint8_t {
    kAlignment = 1 << 0,
    kLineBreak = 1 << 1,
    kHyphenation = 1 << 2,
    kEllipsis = 1 << 3,
    kMaxLines = 1 << 4,
    kLastLine = 1 << 5,
    kWritingMode = 1 << 6,
  };
  uint8_t set = 0;  ///< which fields below were written

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

  /** Writes every SET field over @p options, leaving the rest alone. */
  void applyTo(sigil::weave::ParagraphLayoutOptions& options) const;

  bool operator==(const TextOptions& other) const {
    return set == other.set && alignment == other.alignment &&
           writingMode == other.writingMode && lineBreak == other.lineBreak &&
           hyphenation.enabled == other.hyphenation.enabled &&
           hyphenation.penalty == other.hyphenation.penalty &&
           ellipsis == other.ellipsis && maxLines == other.maxLines &&
           lastLineAlignment == other.lastLineAlignment &&
           justifyLastLine == other.justifyLastLine;
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
  Stagger::From staggerFrom = Stagger::From::Start;
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

struct MemoData {
  std::any props;
  std::function<bool(const std::any&, const std::any&)> equal;
  std::function<Element(const std::any&)> invoke;
  /** The `env::` bindings in scope where this memo was WRITTEN. A memo is
   *  the one deferred describe in the library, so it is also the one place
   *  an inherited value could go stale: the snapshot rides in the memo's
   *  key (resolveMemo compares it before the props) and is re-established
   *  around the invoke. Empty — hence free — when nothing is bound. */
  EnvSnapshot env;
};

struct ElementNode {
  Kind kind = Kind::Box;
  std::string key;
  LayoutProps layout;
  PaintProps paint;
  Corners corners;
  Shape shapeFn;  // custom silhouette; overrides corners. A comparable
                  // scheme prunes; a raw callable never compares equal, so
                  // its node re-patches on every describe.
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
// THE MECHANISM. A structured binding names every direct non-static data
// member of a struct, and the count is a hard error the moment the struct
// changes —
//
//     error: type 'PaintProps' decomposes into 16 elements,
//            but only 15 names were provided
//
// — which is why the decompositions below are the pin. They are EXACT where
// the obvious alternatives are not: a `static_assert(sizeof(T) == N)` is
// walked straight past by a `bool` dropped into tail padding, and an
// aggregate-arity probe (`T{Any{}...}`) counts the BRACE-ELIDED flattening
// of nested aggregates rather than the fields. A structured binding does
// neither.
//
// WHAT A PIN COSTS AND WHAT IT BUYS. Adding a field breaks the
// decomposition HERE (name it), then the field-count `static_assert` in
// Reconcile.cpp (rule on it in the comparator, then bump the count), and
// then — for the two blocks whose fields are all comparable lanes — the
// field walk `EveryPaintPropsFieldParticipatesInEquality` /
// `EveryBoundFloatFieldParticipatesInEquality` in ComposeTestKernel.cpp
// picks the new field up AUTOMATICALLY and fails until the comparator
// notices it. Three gates, two of them the compiler's.
//
// NO PIN IS NEEDED for a struct whose equality is `= default`
// (LayoutProps, Corners, MarkLabel, MarkAnchor, Echo, Anchor, Across, Parts,
// Span, ContentScalars): the compiler writes the exhaustive comparison
// and cannot forget a field. A pin exists only for a comparator a human
// wrote by hand — and the honest way to retire a pin is to give the struct
// a defaulted `operator==`.
//
// CLASSES WITH PRIVATE STATE (Material, Effect, Region, Animatable, Shape,
// Decoration, Profile) are NOT pinned here — a structured binding needs
// access to the members. Their hand-written comparators sit in the same
// header or translation unit as their members, so a field and its
// comparison are read together; PaintProps (here) and propsEqual
// (Reconcile.cpp) are the pair that can drift apart unseen.

inline auto fields(PaintProps& v) {
  auto& [fill, opacity, blendMode, translateX, translateY, rotate, scale,
         scaleX, scaleY, skewX, skewY, originX, originY, originPx, zIndex] = v;
  return std::tie(fill, opacity, blendMode, translateX, translateY, rotate,
                  scale, scaleX, scaleY, skewX, skewY, originX, originY,
                  originPx, zIndex);
}
inline auto fields(TextData& v) {
  auto& [hasTextStroke, textStrokeWidth, textStrokeFill, utf8, style, rich,
         paragraphOverride, layoutOptions, options, spanRestyles, tracks,
         metricFill, onPath, marks] = v;
  return std::tie(hasTextStroke, textStrokeWidth, textStrokeFill, utf8, style,
                  rich, paragraphOverride, layoutOptions, options, spanRestyles,
                  tracks, metricFill, onPath, marks);
}
inline auto fields(SpanRestyle& v) {
  auto& [where, style, paintOnly] = v;
  return std::tie(where, style, paintOnly);
}
inline auto fields(TextOptions& v) {
  auto& [set, alignment, writingMode, lineBreak, hyphenation, ellipsis,
         maxLines, lastLineAlignment, justifyLastLine] = v;
  return std::tie(set, alignment, writingMode, lineBreak, hyphenation, ellipsis,
                  maxLines, lastLineAlignment, justifyLastLine);
}
inline auto fields(ImageData& v) {
  auto& [asset, region, sampling] = v;
  return std::tie(asset, region, sampling);
}
inline auto fields(CustomData& v) {
  auto& [program, key] = v;
  return std::tie(program, key);
}
inline auto fields(DeriveData& v) {
  auto& [placeFn, flowAroundKeys, flowAroundMargin, connectFrom, connectTo,
         router, connectorGap, railAnchors, railRouter, bandSpine, bandAround,
         bandWidth, bandFormation, spanFitKeys, borrowedPathKeys] = v;
  return std::tie(placeFn, flowAroundKeys, flowAroundMargin, connectFrom,
                  connectTo, router, connectorGap, railAnchors, railRouter,
                  bandSpine, bandAround, bandWidth, bandFormation, spanFitKeys,
                  borrowedPathKeys);
}
inline auto fields(StrokePass& v) {
  auto& [where, what, name, half] = v;
  return std::tie(where, what, name, half);
}
inline auto fields(StrokeData& v) {
  auto& [passes] = v;
  return std::tie(passes);
}
inline auto fields(FxData& v) {
  auto& [layerEffect, backdropEffect, echoes, staggerChildrenMs, staggerFrom,
         overlays, masks, markNames] = v;
  return std::tie(layerEffect, backdropEffect, echoes, staggerChildrenMs,
                  staggerFrom, overlays, masks, markNames);
}
inline auto fields(MaterialData& v) {
  auto& [live, recipe] = v;
  return std::tie(live, recipe);
}
inline auto fields(MemoData& v) {
  auto& [props, equal, invoke, env] = v;
  return std::tie(props, equal, invoke, env);
}
inline auto fields(ElementNode& v) {
  auto& [kind, key, layout, paint, corners, shapeFn, clipContent, hitTestable,
         cacheMode, bakeScale, nodeTransition, backgrounds, foregrounds,
         textData, imageData, customData, deriveData, fxData, materialData,
         strokeData, memoData, motionData, children] = v;
  return std::tie(kind, key, layout, paint, corners, shapeFn, clipContent,
                  hitTestable, cacheMode, bakeScale, nodeTransition,
                  backgrounds, foregrounds, textData, imageData, customData,
                  deriveData, fxData, materialData, strokeData, memoData,
                  motionData, children);
}
// Public values with hand-written comparators. They live in Compose.h /
// Animation.h; the pin lives here because the comparator does (Spans:: and
// Gate::operator== are defined in Reconcile.cpp beside propEqual, and
// BoundFloat/Transition/Transitioned/MotionPath/Fill/Mask are compared by
// the helpers there).
inline auto fields(MotionPath& v) {
  auto& [path, t, lookAhead] = v;
  return std::tie(path, t, lookAhead);
}
inline auto fields(TextPath& v) {
  auto& [path, at, align, offset, autoFlip, orient, exactTangent] = v;
  return std::tie(path, at, align, offset, autoFlip, orient, exactTangent);
}
inline auto fields(BoundFloat& v) {
  auto& [source, inScale, inOffset, curve, clampInput, envelope, riseStart,
         holdStart, holdEnd, fallEnd, steps, scale, offset, clamped, lo, hi,
         wiggleAmount, wiggleFrequency, wiggleSeed, wiggleOctaves,
         wiggleFalloff, wrapPeriod] = v;
  return std::tie(source, inScale, inOffset, curve, clampInput, envelope,
                  riseStart, holdStart, holdEnd, fallEnd, steps, scale, offset,
                  clamped, lo, hi, wiggleAmount, wiggleFrequency, wiggleSeed,
                  wiggleOctaves, wiggleFalloff, wrapPeriod);
}
inline auto fields(Transition& v) {
  auto& [duration, ease, delay] = v;
  return std::tie(duration, ease, delay);
}
template <typename T>
auto fields(Transitioned<T>& v) {
  auto& [value, spec, from, waypoints] = v;
  return std::tie(value, spec, from, waypoints);
}
inline auto fields(Fill& v) {
  auto& [kind, colorValue, shaderValue] = v;
  return std::tie(kind, colorValue, shaderValue);
}
inline auto fields(Spans::Term& v) {
  auto& [rule, begin, end, offset, arm, angleDeg, duty, margin, count, index,
         key] = v;
  return std::tie(rule, begin, end, offset, arm, angleDeg, duty, margin, count,
                  index, key);
}
inline auto fields(Gate& v) {
  auto& [kind, where, angleDeg, fraction, region, outside, channel, coverage] =
      v;
  return std::tie(kind, where, angleDeg, fraction, region, outside, channel,
                  coverage);
}
inline auto fields(Mask& v) {
  auto& [what, with] = v;
  return std::tie(what, with);
}
inline auto fields(Stagger& v) {
  auto& [eachMs, amountMs, cueMs, durationMs, loopMs, from, over, beatsOver,
         distribution, inner] = v;
  return std::tie(eachMs, amountMs, cueMs, durationMs, loopMs, from, over,
                  beatsOver, distribution, inner);
}
inline auto fields(Track& v) {
  auto& [where, effect, stagger, progress, reach, continuous] = v;
  return std::tie(where, effect, stagger, progress, reach, continuous);
}

/** How many direct non-static data members @p T has, as the pinned
 *  decomposition above sees them. `static_assert(kFieldCount<X> == N)` beside
 *  a hand-written comparator is this file's `std::variant_size_v`. */
template <class T>
inline constexpr std::size_t kFieldCount =
    std::tuple_size_v<decltype(fields(std::declval<T&>()))>;

/** THE STRUCTURAL PRUNE (Reconcile.cpp). Declared here — rather than kept
 *  in Reconcile.cpp's anonymous namespace — so the field-participation
 *  tests can call the comparator DIRECTLY. Inferring a prune from
 *  `stats().patchedNodes` instead requires re-describing the SAME node,
 *  because keyed siblings never prune into one another; a test that
 *  compares two different nodes will report a difference whatever the
 *  comparator does, and so passes even when the field is unread. */
bool propsEqual(const ElementNode& a, const ElementNode& b);
/** The shaped-binding half of the same comparator (see its doc comment). */
bool boundMapEqual(const BoundFloat& a, const BoundFloat& b);

/** Clamp to [0,1], drop empties, sort and merge — the one normal form
 *  every span answer is in, so overlap tests and complements are honest
 *  interval arithmetic and not a pile of special cases. */
std::vector<Span> normalizeSpans(std::vector<Span> spans);
/** Everything in [0,1] the input does not cover (already normalized). */
std::vector<Span> complementSpans(const std::vector<Span>& spans);
/** THE INTERSECTION LAW, as arithmetic: the runs BOTH sets cover. Two
 *  masks on one target must both pass, and a span-qualified pass under a
 *  span-gated mask claims `where ∩ gate` — so the sweep that lights up a
 *  set of reticle brackets is one line and no re-authoring. Both inputs
 *  are normalized; the answer is too. */
std::vector<Span> intersectSpans(const std::vector<Span>& a,
                                 const std::vector<Span>& b);
/** Do these two normalized sets share more than float noise? Returns the
 *  first shared run, or nullopt. */
std::optional<Span> spansOverlap(const std::vector<Span>& a,
                                 const std::vector<Span>& b);
/** The sub-geometry of `src` covered by `spans` (fractions of the path's
 *  TOTAL arc length — SkTrimPathEffect's coordinate, so a span reveal and
 *  a trim of the same numbers describe the same run). */
SkPath spanPath(const SkPath& src, const std::vector<Span>& spans);
/** The region a spine sweeps at `width` across it, on `formation`'s side.
 *  Empty when the profile is zero everywhere. */
SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation);

/** Constant, binding, or transitioned — flattened for the reconciler. */
template <typename T>
struct ResolvedProp {
  T target{};
  const choreograph::Output<T>* binding = nullptr;
  const Transition* transition = nullptr;  // from with() or node default
};

template <typename T>
ResolvedProp<T> resolveProp(const Animatable<T>& v,
                            const std::optional<Transition>& nodeDefault) {
  ResolvedProp<T> out;
  if (const T* plain = v.plain()) {
    out.target = *plain;
    if (nodeDefault) out.transition = &*nodeDefault;
  } else if (const Transitioned<T>* tr = v.transitioned()) {
    out.target = tr->value;
    out.transition = &tr->spec;
  } else {
    out.binding = v.binding();
  }
  return out;
}

// ---------------------------------------------------------------------------
// TEXT FX — the runtime side of the fx() seam (TextFx.cpp)

/** Equal only when PROVABLY identical: two easing curves compare equal when
 *  both are the same plain function pointer. A lambda-valued curve compares
 *  unequal, conservatively, because a std::function holding one cannot be
 *  inspected. One body, because a second spelling of this rule would let
 *  two comparators disagree about whether a node may prune. */
bool easeEqual(const choreograph::EaseFn& a, const choreograph::EaseFn& b);

/** ONE WALK'S GLYPHS, and which unit of each granularity they fall in.
 *
 *  Built once per paint from the finished layout and shared by every track
 *  on the element, because the expensive parts — walking the placed glyphs,
 *  numbering the words and lines — do not depend on which track is asking.
 *  Reused across frames: build() keeps the allocations. */
struct GlyphStructure {
  static constexpr size_t kUnits = 5;  ///< one lane per Unit enumerator

  std::vector<GlyphInfo> glyphs;  ///< in draw order, structure filled in
  /** Per Unit: glyph index → the unit it belongs to, numbered from 0 in
   *  draw order. */
  std::array<std::vector<uint32_t>, kUnits> unitOf;
  std::array<uint32_t, kUnits> unitCounts{};

  void build(const sigil::weave::ParagraphLayout& layout,
             const sigil::weave::Paragraph& paragraph);
};

/** ONE `rich()` RUN THAT WAS WRITTEN UNDER A STYLE NAME, and the text it
 *  occupies — what `sel::style` resolves against.
 *
 *  The name is tied to the run's TEXT rather than to the style span it
 *  produced, and that is the whole reason the answer holds up. Spans are
 *  cut and merged by every `spanPaint` and `spanStyle` the leaf declares,
 *  so a span index is a number about the paragraph's current normal form;
 *  a run's extent is a fact about the content that only new content
 *  changes. Re-registering the name against a different style, or a restyle
 *  slicing across the run, leaves this untouched.
 *
 *  Built by materializeText as the runs are appended, in declaration order.
 *  Empty for every content form that carries no names. */
struct NamedRun {
  std::string name;
  sigil::weave::CharRange chars;
};

/** Which glyphs a selector addresses: one byte per glyph, in walk order.
 *  A pattern that does not compile answers all-zero and warns once, and so
 *  does an `sel::style` name @p named does not carry. */
std::vector<uint8_t> resolveSelection(const Selector& selector,
                                      const GlyphStructure& structure,
                                      const sigil::weave::Paragraph& paragraph,
                                      std::span<const NamedRun> named);
/** The once-per-pattern diagnostic behind an unresolvable selector. */
void warnBadSelectorPattern(const std::u8string& pattern);
/** The once-per-name diagnostic behind an `sel::style` no run answers to. */
void warnNoSuchStyleName(const std::u8string& name);
/** The once-per-shape diagnostic behind a cue table that does not have one
 *  entry per unit: the tail either piles on the last cue or goes unread,
 *  and both are a table cut against the wrong text. */
void warnCueTableMismatch(size_t cueCount, size_t unitCount);
/** The once-per-process diagnostic behind `onPath` plus a vertical
 *  `writingMode`: a path run's baseline is its own geometry, so there are
 *  no columns to advance and the path wins. */
void warnWritingModeOnPath();
/** The once-per-process diagnostic behind `flowAround` on vertical text:
 *  exclusions are cut out of horizontal line bands, so the columns run
 *  without them. */
void warnFlowAroundVertical();

/** WHICH TEXT A SELECTOR ADDRESSES, as UTF-16 ranges rather than glyphs —
 *  the form span restyling needs, because a restyle happens on the
 *  Paragraph, before there are glyphs to point at.
 *
 *  Sorted, merged and non-overlapping. `|`, `&` and `!` are interval
 *  arithmetic over the text; the complement is taken against the whole
 *  text. `sel::line` reads @p lines, or @p columns where the passage is
 *  vertical and a line IS a column — the geometry a previous layout
 *  produced, passed as plain values rather than as a layout because the
 *  paragraph that layout belongs to is the one being replaced — and
 *  addresses nothing when both are empty. `Selector::take`/`drop` slice
 *  glyphs inside a unit, which no text range can express: an `sel::each`
 *  selector answers with its whole units and the slice warns once.
 *  `sel::style` reads @p named, which is why the table is built before the
 *  restyles that consume it run. */
std::vector<sigil::weave::CharRange> resolveTextRanges(
    const Selector& selector, sigil::weave::Paragraph& paragraph,
    sigil::weave::FontContext& fonts,
    std::span<const sigil::weave::LineMetrics> lines,
    std::span<const sigil::weave::ColumnMetrics> columns,
    std::span<const NamedRun> named);

/** Does this selector reach for a LINE, and therefore need a layout to
 *  resolve against? The question the second layout pass is gated on. */
bool selectorNeedsLayout(const Selector& selector);

/** UTF-8 to the UTF-16 the weave layer speaks. */
std::u16string toUtf16(std::u8string_view utf8);

/** WHERE INDEX `i` OF `count` SITS IN A CASCADE, in multiples of the
 *  per-step delay — 0,1,2… from Start, reversed from End, the two
 *  symmetric V shapes for Center and Edges, and a seeded permutation for
 *  Random.
 *
 *  ONE BODY for two callers: an fx() track's units and a container's
 *  staggered children. A second spelling would let `Stagger::From` mean
 *  two different orders depending on what it was attached to. */
void cascadeOrder(Stagger::From from, uint32_t count, std::vector<float>& out);

/** ONE TRACK'S CASCADE, resolved for a frame's unit counts: the delay
 *  ladder, the beat length, and the virtual span the master progress maps
 *  onto. Built per track per paint; localTime() is then a few adds per
 *  glyph. */
struct Cascade {
  std::vector<float> outerOrder;  ///< outer unit → its place in the cascade
  std::vector<float> innerOrder;  ///< inner unit → the same, within a beat
  /** The author's start-time table at each level, in ms, or empty for the
   *  even ladder above. A table names delays outright, so the order, the
   *  spacing and the distribution curve have nothing left to say. */
  std::vector<float> outerCue, innerCue;
  choreograph::EaseFn outerDistribution, innerDistribution;
  float outerEach = 0;  ///< ms between outer starts
  float innerEach = 0;  ///< ms between inner starts
  float duration = 1;   ///< ms one unit's own motion lasts
  float beatMs = 1;     ///< ms one outer beat occupies
  /** Ms the master progress spans: the one-shot closing span, or the loop
   *  PERIOD when the cascade loops — either way, `master · totalMs` is the
   *  virtual time every local clock reads. */
  float totalMs = 1;
  /** The wrapping period (`Stagger::loopMs`), or 0 for a one-shot cascade.
   *  When set, `totalMs` IS this period and localTime() folds each unit's
   *  elapsed time mod it, so every beat re-opens once per cycle. */
  float loopMs = 0;

  void build(const Stagger& spec, uint32_t outerCount, uint32_t innerCount);
  /** When this unit's beat opens, in ms from the start of the master
   *  progress — the outer delay plus, under a nested cascade, the inner
   *  one. THE one place the schedule is arithmetic; everything that reports
   *  a start time reads it here. */
  [[nodiscard]] float startMs(uint32_t outerUnit, uint32_t innerUnit) const;
  /** The local 0→1 this unit sees at master progress `master`. Clamped at
   *  both ends for a one-shot cascade; a looping one folds the unit's
   *  elapsed time mod `loopMs` first, so the answer re-opens at 0 once per
   *  cycle and rests at 1 between its beat's close and its next opening. */
  [[nodiscard]] float localTime(float master, uint32_t outerUnit,
                                uint32_t innerUnit) const;
};

/** ONE TRACK'S CASCADE RESOLVED AGAINST A LAID-OUT PARAGRAPH: which beat
 *  every glyph falls in at each level, and the ladder those beats run on.
 *
 *  ONE BODY for the painter and for the `beatsOf` query. A second spelling
 *  would let a mark travelling beside a cascade be told a different
 *  schedule from the glyphs it is marking, which is the whole defect the
 *  query exists to close. Reused in place across frames: build() assigns
 *  into the per-glyph lanes rather than clearing them, so a page of
 *  animated type does not mint a pair of vectors per track per frame. */
struct TrackCascade {
  Cascade cascade;
  std::vector<uint32_t> outerUnit;  ///< glyph → its beat
  std::vector<uint32_t> innerUnit;  ///< glyph → its beat inside that beat;
                                    ///< empty without a nested cascade

  void build(const Stagger& spec, const GlyphStructure& structure,
             const std::vector<uint8_t>& selected);
};

/** The composition algebra, in one place: offsets, rotations and shears ADD,
 *  scale, alpha and the colour multiplier MULTIPLY, the additive colour term
 *  ADDS and the screen term SCREENS. Stacked tracks, fx::mix, a seq
 *  crossfade and a keys segment all go through these two, so they cannot
 *  drift apart. */
void compose(GlyphMod& into, const GlyphMod& next);
GlyphMod lerpMod(const GlyphMod& a, const GlyphMod& b, float w);
/** FIELD PIN for GlyphMod (see the FIELD PINS block above) — defined beside
 *  the two functions it guards, never called. */
void glyphModFieldPin(GlyphMod& v);
/** The seed an effect's Rng is constructed from — the glyph's identity plus
 *  the operand lane inside a composite. */
uint64_t glyphSeed(const GlyphInfo& g, uint32_t lane = 0);

}  // namespace sigil::compose::detail
