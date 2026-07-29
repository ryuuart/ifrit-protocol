#pragma once
// Internal to SigilCompose — the element description payload shared by
// the builders (Compose.cpp) and the reconciler (Composer.cpp).

#include "sigilcompose/Compose.h"
#include "sigilcompose/Material.h"

#include <sigilweave/Paragraph.h>

#include <tuple>
#include <utility>

namespace sigil::compose::detail {

enum class Kind : uint8_t { Box, Stack, Text, Image, Custom, Slot };

struct EdgeValues {
  float left = 0, top = 0, right = 0, bottom = 0;
  bool operator==(const EdgeValues &) const = default;
};

/** Per-edge Dims for absolute insets: Auto = that side is unpinned. */
struct EdgeDims {
  Dim left, top, right, bottom;
  bool operator==(const EdgeDims &) const = default;
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
  std::optional<SkPoint> centerAt; // absolute: center ON this point
                                   // (resolved post-measure)
  bool operator==(const LayoutProps &) const = default;
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
  Animatable<float> skewX = 0.0f, skewY = 0.0f; // degrees (shear)
  float originX = 0.5f, originY = 0.5f;
  bool originPx = false; // origin in node-local px instead of fractions
  int zIndex = 0;
};

/** Value-semantic heap box for ElementNode's rare-field blocks: absent
 *  costs one null pointer; copying deep-copies a present block (the COW
 *  clone in Element::NodeHandle::operator-> relies on ElementNode's
 *  defaulted copy constructor). ensure() is the builder-side entry. */
template <class T> class Box {
public:
  Box() = default;
  Box(const Box &other)
      : m_ptr(other.m_ptr ? std::make_unique<T>(*other.m_ptr) : nullptr) {}
  Box(Box &&) noexcept = default;
  Box &operator=(const Box &other) {
    m_ptr = other.m_ptr ? std::make_unique<T>(*other.m_ptr) : nullptr;
    return *this;
  }
  Box &operator=(Box &&) noexcept = default;

  explicit operator bool() const { return m_ptr != nullptr; }
  T *operator->() { return m_ptr.get(); }
  const T *operator->() const { return m_ptr.get(); }
  T &operator*() { return *m_ptr; }
  const T &operator*() const { return *m_ptr; }
  T &ensure() {
    if (!m_ptr)
      m_ptr = std::make_unique<T>();
    return *m_ptr;
  }

private:
  std::unique_ptr<T> m_ptr;
};

// ---- ElementNode blocks: rare/kind-specific fields live out-of-line so a
// plain box costs a fraction of the monolith (2752 B → see sizeof test) and
// each phase's inputs are visible in the type. HOT fields every kind touches
// (layout/paint/corners/decorations/children) stay inline.

struct TextData {
  // Element::textStroke(): a stroke pass on the GLYPHS, under the fill.
  bool hasTextStroke = false;
  float textStrokeWidth = 0.0f;
  Fill textStrokeFill;
  std::u8string utf8;
  sigil::weave::TextStyle style;
  // Full-control overload: identity (the pointer) is the change signal.
  std::shared_ptr<sigil::weave::Paragraph> paragraphOverride;
  sigil::weave::ParagraphLayoutOptions layoutOptions;
  // Kinetic typography
  std::optional<GlyphFx> glyphFx;
  // VariationDrive: a variable-font axis driven at DRAW time (paint-only;
  // the paint phase probes advance-invariance per font and refuses axes
  // that would move advances — GRAD yes, wght no).
  char driveTag[4] = {0, 0, 0, 0};
  const choreograph::Output<float> *driveValue = nullptr;
  // textFill(): glyph paint in text-metric space (unit square → cap band).
  // Resolved at paint from the line metrics; live materials re-resolve per
  // frame; static ones compare by recipe for the prune.
  std::optional<Material> metricFill;
  // onPath(): the run's baseline IS a path. Resolved at paint against the
  // node's box, walked with SkContourMeasure, one RSXform per glyph.
  std::optional<TextPath> onPath;
};

struct ImageData {
  std::shared_ptr<const sigil::image::ImageAsset> asset;
  std::optional<SkRect> region; // atlas sub-rect, source px
  // Element::sampling(). Every blessed image path hardcoded kLinear, so
  // pixel art, tilemaps and simulation buffers drawn through image() were
  // silently blurred and the only escape was Material::image()'s own
  // sampling parameter — discoverable by diffing two signatures.
  SkSamplingOptions sampling{SkFilterMode::kLinear};
};

struct CustomData {
  PaintProgram program;
  // custom(key, program): the program's declared identity (§14). Empty =
  // unkeyed = conservatively unequal, as ever.
  std::string key;
};

struct DeriveData {
  // Custom layout (layout() containers)
  std::function<std::vector<SkRect>(const LayoutInput &)> placeFn;
  std::vector<std::string> flowAroundKeys;
  float flowAroundMargin = 0;
  std::string connectFrom, connectTo;
  Router router;
  std::vector<Anchor> railAnchors; // rail(): ordered waypoints
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
 *  forms stay ordinary foregrounds/backgrounds — they overlay and never
 *  claim, so old scenes cannot become overlap errors (the §27 alias-first
 *  law).
 *
 *  ONE ledger, two z-halves. `half` says only WHERE the pass paints;
 *  claims, the no-overlap law, append order and rest() read the whole
 *  list, because they are statements about ONE boundary and a boundary
 *  does not have two of itself. */
struct StrokePass {
  enum class Half : uint8_t {
    Background, ///< with the backgrounds, below the fill and the children
    Foreground, ///< with the foregrounds, above the children
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
  bool operator==(const MarkLabel &) const = default;
};

struct FxData {
  std::optional<Effect> layerEffect;
  std::optional<Effect> backdropEffect;
  // Misprint echoes (offset flat-color re-stamps under fill/text)
  std::vector<Echo> echoes;
  float staggerChildrenMs = 0; // extra order·each mount delay per subtree
  Stagger::From staggerFrom = Stagger::From::Start;
  // Element::overlay(): decorations painted OVER the fill and UNDER the
  // content and children. Lives in this block rather than beside
  // backgrounds/foregrounds so sizeof(ElementNode) does not grow — the
  // rare-fields rule Composer.cpp's static_assert enforces.
  std::vector<Decoration> overlays;
  // Element::mask(): the appearance-gating family, in declaration order.
  // Masks whose selections overlap INTERSECT on the overlap, and each
  // carries its own animation slots (Instance::maskAnims), so three masks
  // on one node may run at three different rates.
  //
  // Both of this block's departed tenants — trim()'s arc window and
  // wipe()'s half-plane — are members of this list now: a trim is
  // `{parts::all(), by::spans(...)}` and a wipe is `{parts::all(),
  // by::edge(...)}`. That is why the masks live HERE rather than in a new
  // block: the family replaced two fixed field groups with one vector, and
  // ElementNode did not grow a pointer for it.
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
  std::function<bool(const std::any &, const std::any &)> equal;
  std::function<Element(const std::any &)> invoke;
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
  Shape shapeFn; // custom silhouette; overrides corners. A comparable
                 // scheme prunes; a raw callable stays conservative (§3).
  bool clipContent = false;
  // Element::hitTestable(false): the node and its own box are skipped by
  // hitTest, though its CHILDREN are still tested. A keyed full-bleed
  // layout shell with no fill otherwise swallows every hit in the frame,
  // silently and totally.
  bool hitTestable = true;
  Cache cacheMode = Cache::Auto;
  float bakeScale = 1.0f; // Texture-bake resolution multiplier (see Element)
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
  Box<StrokeData> strokeData; // span-qualified stroke passes (rare)
  Box<MemoData> memoData;     // present ⇔ this is a memo shell
  // Element::travel(): the node's position IS a curve. A block rather than
  // a PaintProps field because a Shape plus an Animatable is ~120 B on
  // every node in the tree for a property a handful of them use, and
  // Composer.cpp's static_assert is the rule that says so.
  Box<MotionPath> motionData;

  std::vector<Element> children;

  bool isMemo() const { return (bool)memoData; }
  bool hasMasks() const { return fxData && !fxData->masks.empty(); }
  bool hasStrokePasses() const {
    return strokeData && !strokeData->passes.empty();
  }
  const Across *bandWidth() const {
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
// It happened twice on one feature: `scaleX`/`scaleY` were missing from
// `propsEqual` from the day they landed until e37d58d, and the same pair is
// still missing from `recordBounds()`'s transform gate (filed, ROADMAP).
//
// THE MECHANISM. `kPopOpPso[]` (world/) is index-aligned per variant
// alternative under `static_assert(std::size(...) ==
// std::variant_size_v<pop::Op>)`, so appending an op without ruling on its
// row does not compile. The equivalent for a STRUCT's fields is a
// structured binding: it names every direct non-static data member, and the
// count is a hard error the moment the struct changes —
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
// (LayoutProps, Corners, MarkLabel, Echo, Anchor, Across, Parts, Span,
// TextPath, ContentScalars): the compiler writes the exhaustive comparison
// and cannot forget a field. A pin exists only for a comparator a human
// wrote by hand — and the honest way to retire a pin is to give the struct
// a defaulted `operator==`.
//
// CLASSES WITH PRIVATE STATE (Material, Effect, Region, Animatable, Shape,
// Decoration, Profile) are NOT pinned here — a structured binding needs
// access. Their hand-written comparators sit in the same header or TU as
// their members, which is a far shorter distance than PaintProps (here) to
// propsEqual (Reconcile.cpp); see the ROADMAP entry for the argument.

inline auto fields(PaintProps &v) {
  auto &[fill, opacity, blendMode, translateX, translateY, rotate, scale,
         scaleX, scaleY, skewX, skewY, originX, originY, originPx, zIndex] = v;
  return std::tie(fill, opacity, blendMode, translateX, translateY, rotate,
                  scale, scaleX, scaleY, skewX, skewY, originX, originY,
                  originPx, zIndex);
}
inline auto fields(TextData &v) {
  auto &[hasTextStroke, textStrokeWidth, textStrokeFill, utf8, style,
         paragraphOverride, layoutOptions, glyphFx, driveTag, driveValue,
         metricFill, onPath] = v;
  return std::tie(hasTextStroke, textStrokeWidth, textStrokeFill, utf8, style,
                  paragraphOverride, layoutOptions, glyphFx, driveTag,
                  driveValue, metricFill, onPath);
}
inline auto fields(ImageData &v) {
  auto &[asset, region, sampling] = v;
  return std::tie(asset, region, sampling);
}
inline auto fields(CustomData &v) {
  auto &[program, key] = v;
  return std::tie(program, key);
}
inline auto fields(DeriveData &v) {
  auto &[placeFn, flowAroundKeys, flowAroundMargin, connectFrom, connectTo,
         router, railAnchors, railRouter, bandSpine, bandAround, bandWidth,
         bandFormation, spanFitKeys, borrowedPathKeys] = v;
  return std::tie(placeFn, flowAroundKeys, flowAroundMargin, connectFrom,
                  connectTo, router, railAnchors, railRouter, bandSpine,
                  bandAround, bandWidth, bandFormation, spanFitKeys,
                  borrowedPathKeys);
}
inline auto fields(StrokePass &v) {
  auto &[where, what, name, half] = v;
  return std::tie(where, what, name, half);
}
inline auto fields(StrokeData &v) {
  auto &[passes] = v;
  return std::tie(passes);
}
inline auto fields(FxData &v) {
  auto &[layerEffect, backdropEffect, echoes, staggerChildrenMs, staggerFrom,
         overlays, masks, markNames] = v;
  return std::tie(layerEffect, backdropEffect, echoes, staggerChildrenMs,
                  staggerFrom, overlays, masks, markNames);
}
inline auto fields(MaterialData &v) {
  auto &[live, recipe] = v;
  return std::tie(live, recipe);
}
inline auto fields(MemoData &v) {
  auto &[props, equal, invoke, env] = v;
  return std::tie(props, equal, invoke, env);
}
inline auto fields(ElementNode &v) {
  auto &[kind, key, layout, paint, corners, shapeFn, clipContent, hitTestable,
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
inline auto fields(MotionPath &v) {
  auto &[path, t, lookAhead] = v;
  return std::tie(path, t, lookAhead);
}
inline auto fields(BoundFloat &v) {
  auto &[source, inScale, inOffset, curve, clampInput, steps, scale, offset,
         clamped, lo, hi, wiggleAmount, wiggleFrequency, wiggleSeed,
         wiggleOctaves, wiggleFalloff] = v;
  return std::tie(source, inScale, inOffset, curve, clampInput, steps, scale,
                  offset, clamped, lo, hi, wiggleAmount, wiggleFrequency,
                  wiggleSeed, wiggleOctaves, wiggleFalloff);
}
inline auto fields(Transition &v) {
  auto &[duration, ease, delay] = v;
  return std::tie(duration, ease, delay);
}
template <typename T> auto fields(Transitioned<T> &v) {
  auto &[value, spec, from, waypoints] = v;
  return std::tie(value, spec, from, waypoints);
}
inline auto fields(Fill &v) {
  auto &[kind, colorValue, shaderValue] = v;
  return std::tie(kind, colorValue, shaderValue);
}
inline auto fields(Spans::Term &v) {
  auto &[rule, begin, end, offset, arm, angleDeg, duty, margin, count, index,
         key] = v;
  return std::tie(rule, begin, end, offset, arm, angleDeg, duty, margin, count,
                  index, key);
}
inline auto fields(Gate &v) {
  auto &[kind, where, angleDeg, fraction, region, outside, channel, coverage] =
      v;
  return std::tie(kind, where, angleDeg, fraction, region, outside, channel,
                  coverage);
}
inline auto fields(Mask &v) {
  auto &[what, with] = v;
  return std::tie(what, with);
}

/** How many direct non-static data members @p T has, as the pinned
 *  decomposition above sees them. `static_assert(kFieldCount<X> == N)` beside
 *  a hand-written comparator is this file's `std::variant_size_v`. */
template <class T>
inline constexpr std::size_t kFieldCount =
    std::tuple_size_v<decltype(fields(std::declval<T &>()))>;

/** THE STRUCTURAL PRUNE (Reconcile.cpp). Declared here — rather than kept
 *  in Reconcile.cpp's anonymous namespace — so the field-participation
 *  controls can call the comparator DIRECTLY. Inferring it from
 *  `stats().patchedNodes` needs a re-describe of the SAME node (keyed
 *  siblings never prune into one another), which is exactly the trap that
 *  made nine pins pass their own positive control this session. */
bool propsEqual(const ElementNode &a, const ElementNode &b);
/** The shaped-binding half of the same comparator (see its doc comment). */
bool boundMapEqual(const BoundFloat &a, const BoundFloat &b);

/** Clamp to [0,1], drop empties, sort and merge — the one normal form
 *  every span answer is in, so overlap tests and complements are honest
 *  interval arithmetic and not a pile of special cases. */
std::vector<Span> normalizeSpans(std::vector<Span> spans);
/** Everything in [0,1] the input does not cover (already normalized). */
std::vector<Span> complementSpans(const std::vector<Span> &spans);
/** THE INTERSECTION LAW, as arithmetic: the runs BOTH sets cover. Two
 *  masks on one target must both pass, and a span-qualified pass under a
 *  span-gated mask claims `where ∩ gate` — so the sweep that lights up a
 *  set of reticle brackets is one line and no re-authoring. Both inputs
 *  are normalized; the answer is too. */
std::vector<Span> intersectSpans(const std::vector<Span> &a,
                                 const std::vector<Span> &b);
/** Do these two normalized sets share more than float noise? Returns the
 *  first shared run, or nullopt. */
std::optional<Span> spansOverlap(const std::vector<Span> &a,
                                 const std::vector<Span> &b);
/** The sub-geometry of `src` covered by `spans` (fractions of the path's
 *  TOTAL arc length — SkTrimPathEffect's coordinate, so a span reveal and
 *  a trim of the same numbers describe the same run). */
SkPath spanPath(const SkPath &src, const std::vector<Span> &spans);
/** The region a spine sweeps at `width` across it, on `formation`'s side.
 *  Empty when the profile is zero everywhere. */
SkPath bandRegion(const SkPath &spine, const Across &width,
                  Formation formation);

/** Constant, binding, or transitioned — flattened for the reconciler. */
template <typename T> struct ResolvedProp {
  T target{};
  const choreograph::Output<T> *binding = nullptr;
  const Transition *transition = nullptr; // from with() or node default
};

template <typename T>
ResolvedProp<T> resolveProp(const Animatable<T> &v,
                            const std::optional<Transition> &nodeDefault) {
  ResolvedProp<T> out;
  if (const T *plain = v.plain()) {
    out.target = *plain;
    if (nodeDefault)
      out.transition = &*nodeDefault;
  } else if (const Transitioned<T> *tr = v.transitioned()) {
    out.target = tr->value;
    out.transition = &tr->spec;
  } else {
    out.binding = v.binding();
  }
  return out;
}

} // namespace sigil::compose::detail
