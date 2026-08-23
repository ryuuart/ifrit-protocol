#pragma once

/** @file
 * SigilCompose kernel — data-driven, cacheable, animated drawable
 * components for any SkCanvas.
 *
 * The kernel is: Element descriptions built by fluent value builders,
 * component functions over plain data (+ memo), and a Composer that
 * reconciles by key, lays out via Yoga (SigilWeave-measured text leaves),
 * paints with explicit stacking, caches provably-static subtrees as
 * SkPictures automatically, and animates through Choreograph driven by
 * an sigil::motion::Ticker.
 *
 * THE TWO WRITE PATHS, and the reason they are separate. Structure and
 * discrete state arrive by DESCRIBE — `Composer::render()` /
 * `renderSlot()`, reconciled by key. Per-frame values arrive by BIND — a
 * non-owning pointer to a live `choreograph::Output` the host steps —
 * and a binding is paint-only: it never relayouts. Because both paths are
 * DECLARED, the library can decide, without running anything, which
 * subtrees cannot have changed, and cache exactly those. Every automatic
 * cache in this file rests on that property. The corollary is an author
 * obligation: anything that changes without a re-describe must say so,
 * because nothing introspects a type-erased value.
 */

#include <include/core/SkBlendMode.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypes.h>
#include <sigilmotion/Animation.h>
#include <sigilmotion/FrameClock.h>
#include <sigilmotion/Ticker.h>
#include <sigilweave/ParagraphLayout.h>
#include <sigilweave/Style.h>

#include <any>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

class SkCanvas;
class SkImage;
class SkImageFilter;
class SkPicture;
class SkRuntimeEffect;

namespace sigil::image {
class ImageAsset;
}

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

namespace detail {
struct ElementNode;
struct Instance;
}  // namespace detail

// The polymorphic paint value (<sigilcompose/Material.h>) — supersedes Fill as
// the authoring value for fill(); a static Material collapses to a Fill.
class Material;

// ---------------------------------------------------------------------------
// Animation values

/** ANIMATION VALUES — Transition, the `ease::` curves, the animate()
 *  keyframe builders and the shaped `bind()` binding — are defined in
 *  SigilMotion (<sigilmotion/Animation.h>). None of them touches Skia,
 *  Yoga or the kernel, so other libraries can speak them without linking a
 *  drawing library; SigilCompose already links SigilMotion.
 *
 *  The re-export is permanent, not a shim. These types appear in compose's
 *  own signatures (Element::transition(), animate()'s spec argument, every
 *  Animatable property), so they are part of compose's authoring surface
 *  whoever defines them. `compose::Transition` and `motion::Transition`
 *  name one entity, not two competing spellings. */
using motion::animate;
using motion::bind;
using motion::Bound;
using motion::BoundFloat;
using motion::From;
using motion::from;
using motion::FromTo;
using motion::through;
using motion::To;
using motion::to;
using motion::Transition;
using motion::Transitioned;
using motion::Waypoints;
using motion::wiggle;
namespace ease = motion::ease;
/** The `floor(t·hz)/hz` time quantizer — the same arithmetic
 *  `Material::quantizeTime(hz)` applies to a shader's uTime, re-exported
 *  so host steppables quantizing their OWN schedules spell it the same
 *  way instead of hand-rolling it.
 *
 *  The derivation verb itself is `Ticker::derive(dst, bind(&src)…)`, a
 *  Ticker member rather than a free factory, so it needs no re-export and
 *  cannot collide with compose's `derive::` namespace (the geometry derive
 *  phase below), which already owns that word at namespace scope. */
using motion::quantizeTime;

/** `Animatable<T>` — THE PROPERTY SLOT: a value that can move. Four
 *  forms: a plain T, a Transitioned<T>, a live `choreograph::Output<T>*`,
 *  or that binding shaped through bind(). Defined in SigilMotion
 *  (<sigilmotion/Animation.h>) for the same reason as everything above —
 *  no Skia, Yoga or kernel type appears anywhere in it.
 *
 *  The two write paths meet in this one type. A plain T (or a
 *  Transitioned<T>) is DESCRIBED state: it changes only when the author
 *  describes again, and the reconciler's property comparison can see that
 *  it did. A bound `Output<T>*` is a per-frame value the host writes, and
 *  it compares BY IDENTITY — the pointer, not the number behind it — so a
 *  node holding one is declared volatile and does not cache. That is why
 *  handing back a freshly constructed Output at a new address breaks
 *  pruning even when the value is unchanged: the address is the property.
 *
 *  Compose owns RESOLUTION, not the value: an Animatable is resolved
 *  against a PaintContext, taking node transitions, stagger, mount
 *  entrances and the per-frame composer state into account. SigilMotion
 *  supplies the value; compose decides what a described change means to a
 *  node. */
using motion::Animatable;

// ---------------------------------------------------------------------------
// Paint values

/** A paint slot: nothing, a color, or anything Skia can shade (gradient
 *  helpers live in util, SkSL via SkRuntimeEffect works here). */
struct Fill {
  enum class Kind : uint8_t { None, Color, Shader };

  static Fill color(SkColor4f c) { return {Kind::Color, c, nullptr}; }
  static Fill shader(sk_sp<SkShader> s);
  static Fill none() { return {}; }

  Kind kind = Kind::None;
  SkColor4f colorValue = {0, 0, 0, 0};
  sk_sp<SkShader> shaderValue;

  bool operator==(const Fill& o) const {
    return kind == o.kind && colorValue == o.colorValue &&
           shaderValue == o.shaderValue;
  }
};

/** Corner radii, clockwise from top-left. `{r}` rounds all four; the
 *  four-value form dresses each corner independently. For shapes whose
 *  corners aren't box corners (stars, polygons, custom outlines), use
 *  shapes::rounded() around the outline generator instead. */
struct Corners {
  float topLeft = 0.0f, topRight = 0.0f, bottomRight = 0.0f, bottomLeft = 0.0f;

  Corners() = default;
  Corners(float all)  // NOLINT: implicit by design (.corners({8}))
      : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}
  Corners(float tl, float tr, float br, float bl)
      : topLeft(tl), topRight(tr), bottomRight(br), bottomLeft(bl) {}

  bool any() const {
    return topLeft > 0 || topRight > 0 || bottomRight > 0 || bottomLeft > 0;
  }
  bool operator==(const Corners&) const = default;
};

/** The one paint-program context: custom leaves (and, in extensions,
 *  decorations and contour walks) all receive this. `elapsedSeconds` is
 *  the Ticker's FrameClock time — pause/time-scale affect it. `fonts`
 *  is the owning composer's FontContext (null only when a decoration
 *  is painted outside a composer) — what element stamps and ad-hoc
 *  SigilWeave drawing inside paint programs lay text out with. */
struct PaintContext {
  SkSize size = SkSize::MakeEmpty();
  SkPath outline;
  double elapsedSeconds = 0.0;
  float contentScale = 1.0f;
  /** Is the composer's Ticker running anything at all this frame, as
   *  read by a node that REPAINTS this frame (a cached node replays its
   *  recording and keeps its last-read value) — the
   *  WHOLE tree's answer, not this node's. A program that wants cheap
   *  chrome while something moves reads it; nothing in the library does.
   *  False outside a composer (a decoration painted standalone), which is
   *  the honest answer there: there is no ticker to be active. */
  bool animating = false;
  sigil::weave::FontContext* fonts = nullptr;
  /** Paths this node BORROWED from keyed elements in the derive phase, in
   *  its own local space — what `strand::from(key)` reads. Null outside a
   *  composer, or when the node borrowed nothing. Non-owning: valid for
   *  the duration of the paint call only.
   *
   *  A decoration declares what it borrows (see BorrowingDecoration
   *  below) so the element can register the keys without introspecting a
   *  type-erased value; the derive pass then resolves them on the same
   *  flat edge-store walk connectors and flowAround ride. */
  const std::vector<std::pair<std::string, SkPath>>* borrowed = nullptr;

  /** The borrowed path for `key`, or an empty path. */
  SkPath borrowedPath(const std::string& key) const {
    if (borrowed)
      for (const auto& [k, p] : *borrowed)
        if (k == key) return p;
    return SkPath();
  }

  /** The instance's stamp-bake store — null outside a composer (standalone
   *  decoration paints fall back to the brush value's own cache). Mutable
   *  through a const context on purpose: a bake is a cache write, not a
   *  paint output. */
  class StampCache* stamps = nullptr;

  /** The node→composer-root matrix, i.e. the forward accumulation of
   *  paint()'s own transform stack; the hit test walks its inverse. This
   *  is what `Material::worldSpace` anchors against. Identity outside a
   *  composer, which degrades deterministically: a world-space material
   *  resolved standalone anchors node-locally and draws the same picture
   *  as the unflagged one. Layout-derived, like `size` — never part of any
   *  prune signature. */
  SkMatrix toRoot = SkMatrix::I();
  /** The composer root's laid-out size in canvas px — what uResolution
   *  becomes for a world-space material (a canvas-unit ramp spans the
   *  canvas). Empty outside a composer; resolve falls back to `size`. */
  SkSize rootSize = SkSize::MakeEmpty();
};

using PaintProgram = std::function<void(SkCanvas&, const PaintContext&)>;

/** The INSTANCE-SIDE bake store for stamped brushes: tile bakes live with
 *  the NODE, not inside the brush value. A brush value constructed fresh
 *  by every describe would otherwise re-rasterize its art each time — the
 *  one place where re-describing costs raster work rather than a diff.
 *  Keeping the bake on the instance means the rebuilt value finds it.
 *
 *  Keyed on the art Element's node WITH A WEAK GUARD, and the guard is
 *  load-bearing: a plain map on the raw pointer would let a freed node's
 *  recycled address silently inherit the wrong art's bake. Locking the
 *  weak handle and comparing identity makes that impossible — a recycled
 *  key fails the check and re-bakes. Entries carry either a picture
 *  (pattern and scatter tiles) or a rastered image plus its logical size;
 *  each consumer reads only its own kind. */
class StampCache {
 public:
  struct Entry {
    sk_sp<SkPicture> pic;
    sk_sp<SkImage> image;
    SkSize artSize{0, 0};
  };
  /** The entry for `key`, or null — never a recycled address's entry. */
  const Entry* get(const std::shared_ptr<const void>& key) const {
    auto it = m_entries.find(key.get());
    if (it == m_entries.end()) return nullptr;
    if (it->second.first.lock() != key)
      return nullptr;  // the address was recycled: not this art's bake
    return &it->second.second;
  }
  void put(const std::shared_ptr<const void>& key, Entry entry) {
    // A node's stamp arts are few; a runaway map means keys churn every
    // frame, and keeping stale bakes alive would pin their nodes' memory.
    if (m_entries.size() >= 16) m_entries.clear();
    m_entries[key.get()] = {std::weak_ptr<const void>(key), std::move(entry)};
  }

 private:
  std::unordered_map<const void*, std::pair<std::weak_ptr<const void>, Entry>>
      m_entries;
};

/** A CALLER-OWNED UNIFORM BUFFER WITH A REVISION — the live form of an
 *  array uniform, for per-frame data no scalar `Output` can carry: a
 *  particle table, a per-bar spectrum, a set of rects a simulation moves.
 *
 *  Own it where you own your model, write `values()`, then `commit()` to
 *  publish. The binding (`Material::uniform` / `Effect::uniform` with a
 *  block) reads the CURRENT values at every paint and declares volatility
 *  the way a bound scalar `Output*` does, so the node paints live and no
 *  cache can freeze the table; the revision is what lets the resolve memo
 *  see that an uncommitted frame changed nothing and keep the built shader.
 *
 *  LIFETIME AND EQUALITY follow the bound-scalar rules exactly. The
 *  binding is a shared_ptr, so the buffer cannot dangle, but it compares
 *  by IDENTITY: a block recreated every describe reads as a new binding
 *  each time and re-patches its node — hold the block beside your model,
 *  not in the describe. The values belong to the system and never enter
 *  the prune comparison. Not thread-safe, deliberately: one owner, one
 *  writer, matching PixelBuffer. */
class UniformBlock {
 public:
  /** `floatCount` is the buffer's length in FLOATS, and it must equal the
   *  declared uniform's total float count — 3 float4s is 12. The size is
   *  fixed for the block's life, because the declared array's is. */
  explicit UniformBlock(size_t floatCount) : m_values(floatCount, 0.0f) {}
  /** The floats, yours to write. Publish with commit(). */
  std::span<float> values() { return m_values; }
  std::span<const float> values() const { return m_values; }
  size_t size() const { return m_values.size(); }
  /** PUBLISH the edit: the next paint resolves a fresh shader from the new
   *  values (an uncommitted frame reuses the previous one). */
  void commit() { ++m_revision; }
  uint64_t revision() const { return m_revision; }

 private:
  std::vector<float> m_values;
  uint64_t m_revision = 0;
};

/**
 * Post-processing at stacking-context boundaries. `filter` wraps any
 * SkImageFilter (blur, displacement, lighting, compose chains);
 * `shader` wraps an SkSL runtime effect whose child shader is the
 * rendered layer. Attach with Element::effect() (the node's own layer)
 * or Element::backdrop() (what's already painted beneath the node's
 * bounds). Under Cache::Texture an effect() is baked into the snapshot
 * — expensive filters on static content are paid once.
 */
class Effect {
 public:
  static Effect filter(sk_sp<SkImageFilter> f);
  /** @p uniforms are float uniforms set by name on the SkSL effect;
   *  the layer arrives as the child shader named "content".
   *
   *  A name the effect does not declare as a float uniform — a typo, or a
   *  float2/float4/array, none of which this door can fill — is warned
   *  about once and IGNORED, never a debug abort: one typo in a
   *  live-reloaded sketch must not take the host process down. */
  static Effect shader(
      sk_sp<SkRuntimeEffect> effect,
      std::vector<std::pair<std::string, float>> uniforms = {});
  /** A blur that smears ALONG one direction: @p sigma along the axis at
   *  @p angleDeg (degrees, Element::rotate's screen sense — 0 smears
   *  horizontally, 90 vertically, 45 down-right), @p across
   *  perpendicular to it (default 0, a pure streak). A spatial filter,
   *  not motion blur: it knows nothing about how the node moved.
   *
   *  Built entirely from existing filters, no new SkSL. At an
   *  axis-aligned angle it IS `SkImageFilters::Blur(x, y)`, bit-identical;
   *  at any other angle it is a rotate → Blur → unrotate sandwich, three
   *  nodes the filter DAG composes.
   *
   *  Unlike a raw filter() this carries a comparable RECIPE, so a
   *  re-described equal directionalBlur PRUNES where filter() — which can
   *  only compare its already-built filter by pointer — does not. The
   *  named parameters "sigma" / "angle" / "across" accept
   *  uniform(name, &output) below, so an animated smear angle rides the
   *  live channel instead of re-describing per frame. */
  static Effect directionalBlur(float sigma, float angleDeg, float across = 0);
  /** A blur whose SIGMA VARIES ACROSS THE NODE — a depth-of-field
   *  falloff, a lens edge, a tube's curvature. @p sigmaMap is a Material
   *  read as a NUMBER rather than as paint: its RED channel at a pixel,
   *  times @p maxSigma, is the blur radius there. The natural authoring is
   *  therefore a unit-space ramp — `Material::linearUnit({0,0}, {1,0},
   *  {{0, black}, {1, white}})` is "sharp at the left edge, softest at the
   *  right" over whatever box the layout decides — and any sksl() material
   *  is an arbitrary field.
   *
   *  Written as its own effect rather than left to Effect::shader because
   *  a hand-written SkSL kernel would have to pay the WORST sigma at every
   *  pixel: SkSL has no cheap dynamic loop bound, so the kernel must be
   *  sized for the largest radius anywhere in the node, and a Gaussian
   *  stops being separable once sigma varies. This spends a fixed number
   *  of passes instead, so cost grows far more slowly with @p maxSigma.
   *  How it spends them is the library's business and deliberately absent
   *  from this signature: the author says "blur varying by this map".
   *
   *  Rides the same rails as directionalBlur: a comparable RECIPE (an
   *  equal re-described blur PRUNES, and the sigma map's Material
   *  participates in that equality), the named parameter "maxSigma"
   *  accepts uniform(name, &output), and a LIVE sigma map (a bound
   *  uniform, uTime) makes the whole effect isAnimated() by tier
   *  inheritance — so a bake can never sample the map once and freeze it.
   *  `child("sigma", otherMap)` re-aims the map on an existing blur. */
  static Effect blur(Material sigmaMap, float maxSigma);
  /** THE CHILD SLOT — `Material::child` on the effect seam: same name,
   *  same shape, same semantics. The effect declares `uniform shader
   *  NAME;` and this fills it with a Material, so the SkSL can read a
   *  source the node has NOT painted: a parameter field, a mask channel, a
   *  gradient, a second texture. `Effect::shader` fills exactly one child
   *  itself — `content`, the node's own rendered layer — and this is how
   *  any further declared `uniform shader` gets a source. The Material
   *  resolves against THIS NODE's box, so unit-space authoring
   *  (linearUnit / glowUnit) works here as it does on a fill.
   *
   *  TIER INHERITANCE is the load-bearing half, and it calls Material's
   *  own recursion rather than repeating its rule: a live child makes the
   *  effect isAnimated(), so the node is declared volatile and no cache
   *  can freeze the parameter; the children also ride the prune signature,
   *  so two effects with different children never compare equal.
   *
   *  SILENT-ISH GUARDRAILS, matching Material::child. A name the effect
   *  does not declare as `uniform shader` warns and is IGNORED. On an
   *  effect kind with no child to fill — `filter()`, which wraps an
   *  already-built SkImageFilter, or a bare `directionalBlur()` — the call
   *  is a no-op with a warning, exactly as uniform() is there. On a
   *  blur() the one fillable name is "sigma", its sigma map. */
  Effect& child(std::string name, Material source);
  /** A LIVE float uniform — Material's contract, on the effect seam. The
   *  value is read from the Output at every paint, and the node repaints
   *  every frame while the effect is attached: a bound uniform declares
   *  volatility exactly as a live material does, which is what lets a
   *  ripple phase or a bloom threshold animate without re-describing.
   *
   *  Meaningful on a shader() effect (any declared float uniform), a
   *  directionalBlur() ("sigma" / "angle" / "across") or a blur()
   *  ("maxSigma"). A filter() has no uniform to receive the value: the
   *  binding warns and is ignored there, and no volatility is declared, so
   *  nothing animates. Every other rejection behaves the same way — a name
   *  a shader() effect does not declare as a float uniform, an unknown
   *  recipe name on the other kinds, a null @p value: warned about once,
   *  not recorded, and no volatility declared for it, because a binding
   *  nothing reads must not cost a repaint per frame forever. */
  Effect& uniform(std::string name, const choreograph::Output<float>* value);
  /** CONSTANT uniforms after construction — Material::uniform's shapes on
   *  the effect seam, for the sizes the shader() constructor list cannot
   *  carry. The float form is the constructor list's late spelling; the
   *  float2 and float4 forms fill `uniform float2` / `uniform float4`
   *  declarations; the vector form fills a declared ARRAY, matched by
   *  TOTAL float count, so 12 floats fill `float4 uRect[3]` and
   *  `float uWeights[12]` alike.
   *
   *  Meaningful on a shader() effect only — the other kinds have no named
   *  declarations to fill. Guardrails are uniform()'s: an undeclared name,
   *  or one whose declared size is not the value's, warns once and is
   *  IGNORED, never a debug abort. The recipe stays comparable: constants
   *  participate in operator==, so a re-described equal effect prunes. */
  Effect& uniform(std::string name, float value);
  Effect& uniform(std::string name, std::array<float, 2> value);
  Effect& uniform(std::string name, std::array<float, 4> value);
  Effect& uniform(std::string name, std::vector<float> values);
  /** A LIVE ARRAY — a `UniformBlock` the caller owns, writes and
   *  commit()s, read at every paint. Declares volatility exactly as a
   *  bound scalar Output does: the node paints live while the effect is
   *  attached, and no cache can freeze the table. The binding compares by
   *  block identity; the values belong to the system and never prune.
   *  Size-checked at store time against the declared array's total float
   *  count, because the builder refuses a partial array write. */
  Effect& uniform(std::string name, std::shared_ptr<const UniformBlock> block);
  /** Chain: apply `next` AFTER this effect (SkImageFilters::Compose) —
   *  e.g. the DWM glass formula: Effect::filter(Blur(3,3)).then(
   *  Effect::shader(colorize)). Static chains precompose once; a chain
   *  with a live side re-composes at each paint. */
  Effect then(const Effect& next) const;

  const sk_sp<SkImageFilter>& imageFilter() const { return m_filter; }
  /** The filter with any bound uniforms resolved NOW — what the paint
   *  phase applies. Identical to imageFilter() for a static effect.
   *  @p ctx is the painting node's PaintContext, which child() materials
   *  resolve against (its box, its clock) — exactly the context
   *  Material::child hands its children. Null is the context-free form:
   *  static children keep their snapshot, and it is what a caller holding
   *  an Effect outside a paint can ask for. */
  sk_sp<SkImageFilter> resolvedImageFilter(
      const PaintContext* ctx = nullptr) const;
  /** THE VOLATILITY DECLARATION — one word across the whole library: does
   *  this effect change without a re-describe? True while any uniform is
   *  bound, or while any child Material is live. The tier inheritance
   *  calls `Material::isAnimated()`'s own recursion rather than repeating
   *  its rule. */
  bool isAnimated() const;
  /** Does any child Material anchor to the composer root
   *  (`Material::worldSpace`)? The reconcile walk asks this so it can mark
   *  the node's world matrix stale when an ancestor's static transform is
   *  re-described. Same tier-inheritance shape as isAnimated(). */
  bool usesWorldSpace() const;
  /** Structural equality for the reconciler. A static shader effect
   *  compares by RECIPE — runtime-effect pointer plus constant uniforms —
   *  so a re-described effect prunes as long as the caller holds ONE
   *  SkRuntimeEffect and rebuilds only the wrapper around it. A live
   *  effect never compares equal, conservatively, like a live material.
   *  A filter() effect compares by filter pointer, since an already-built
   *  SkImageFilter carries no recipe to compare. */
  bool operator==(const Effect& o) const;

 private:
  /** directionalBlur()'s comparable recipe — what operator== compares
   *  (structural, like a shader recipe) and what bound "sigma" / "angle"
   *  / "across" uniforms rebuild from per paint. */
  struct DirectionalBlur {
    float sigma = 0, angleDeg = 0, across = 0;
    bool operator==(const DirectionalBlur&) const = default;
  };
  /** blur()'s comparable recipe — the parameter's RANGE only. The sigma
   *  MAP itself lives in m_children under "sigma", so one child vector
   *  carries every Material an effect samples: one equality, one tier
   *  walk, one resolve loop, and `child("sigma", …)` re-aims the map for
   *  free. */
  struct ParamBlur {
    float maxSigma = 0;
    bool operator==(const ParamBlur&) const = default;
  };

  sk_sp<SkImageFilter> m_filter;
  // The shader recipe (kept so bound uniforms can rebuild per paint and
  // so equality can compare structurally).
  sk_sp<SkRuntimeEffect> m_effect;
  std::vector<std::pair<std::string, float>> m_uniforms;
  // The wider constant shapes, one lane per declared size the builder
  // distinguishes; arrays are stored flat and matched by total float count.
  std::vector<std::pair<std::string, std::array<float, 2>>> m_uniforms2;
  std::vector<std::pair<std::string, std::array<float, 4>>> m_uniforms4;
  std::vector<std::pair<std::string, std::vector<float>>> m_uniformArrays;
  std::vector<std::pair<std::string, const choreograph::Output<float>*>>
      m_bound;
  // Live arrays: caller-owned UniformBlocks, read at every paint. Their
  // presence makes the effect isAnimated(), like a bound scalar.
  std::vector<std::pair<std::string, std::shared_ptr<const UniformBlock>>>
      m_blocks;
  std::optional<DirectionalBlur> m_dirBlur;  // directionalBlur()'s recipe
  std::optional<ParamBlur> m_paramBlur;      // blur()'s recipe
  // The child slots: `uniform shader NAME` → Material. Held by
  // shared_ptr because Material is only FORWARD-DECLARED here (Material.h
  // includes this header, so it cannot be included back) — the surface is
  // still child(name, Material) by value, and a copied Effect never
  // mutates a shared child, it replaces the pointer.
  std::vector<std::pair<std::string, std::shared_ptr<const Material>>>
      m_children;
  // then()-chain retained only when a side is live (static chains
  // precompose into m_filter and carry no nodes).
  std::shared_ptr<const Effect> m_chainA, m_chainB;

  /** Does any child need a PaintContext to resolve (live or geometry
   *  tier)? Material::build's memo asks exactly this of its own children,
   *  for the same reason: a static child's snapshot is already correct,
   *  and a context-needing one must be rebuilt per paint or it freezes. */
  bool anyChildNeedsContext() const;
  /** The child slot @p name as a shader, resolved against @p ctx. */
  sk_sp<SkShader> childShaderFor(std::string_view name,
                                 const PaintContext* ctx) const;
  /** The recipe's filter, built unconditionally — the store-time snapshot
   *  (null ctx) and the per-paint resolve are one construction. */
  sk_sp<SkImageFilter> buildFilter(const PaintContext* ctx) const;

  /** FIELD PIN (see ComposeInternal.h's FIELD PINS block). operator== is
   *  hand-written in Compose.cpp and reads these members directly; the state
   *  is private, so the decomposition lives inside the class. */
  static void fieldPin(Effect& v) {
    auto& [filter, effect, uniforms, uniforms2, uniforms4, uniformArrays, bound,
           blocks, dirBlur, paramBlur, children, chainA, chainB] = v;
    static_assert(std::tuple_size_v<decltype(std::tie(
                          filter, effect, uniforms, uniforms2, uniforms4,
                          uniformArrays, bound, blocks, dirBlur, paramBlur,
                          children, chainA, chainB))> == 13,
                  "Effect gained or lost a member — rule on it in "
                  "Effect::operator== (Compose.cpp), then bump this count. "
                  "(m_filter is EXCLUDED on the shader, directionalBlur and "
                  "blur paths because it is derived from m_effect + the "
                  "constant lanes / m_dirBlur / m_paramBlur + m_children; "
                  "m_bound and m_blocks make the effect isAnimated(), which "
                  "operator== already refuses; m_chainA/B only exist on a "
                  "live chain, ditto.)");
  }
};

// ---------------------------------------------------------------------------
// TEXT FX — the multi-track per-glyph seam (presets in
// <sigilcompose/TextFx.h>)
//
// A text() element carries an ordered list of TRACKS. One track is
// (selector, effect, stagger, progress): WHICH glyphs it addresses, WHAT
// deviation from rest it asks of them, HOW their start times spread, and
// the master 0→1 that drives the whole thing. Several tracks on one
// element compose per glyph — offsets and rotations ADD, scale and alpha
// MULTIPLY — so a rise, a per-word wobble and a colour-independent fade
// are three independent values rather than one hand-merged lambda.
//
// Everything on the seam is a COMPARABLE VALUE. That is not decoration:
// the reconciler prunes a re-described element only when it can prove the
// description is the same one, and a `std::function` cannot be compared.
// A preset carries its name and its parameters; an ad-hoc lambda carries
// the key its author gave it.

/** The granularity a selector slices and a stagger beats over.
 *
 *  `Cluster` is the default and the one that keeps text correct: a base
 *  letter and its combining marks, or the several glyphs an emoji
 *  sequence shapes to, are ONE cluster and move together. `Glyph` is the
 *  raw shaping unit and will separate those marks from what they sit on. */
enum class Unit : uint8_t { Glyph, Cluster, Word, Line, Sentence };

/** The granularity names, spelled the way tracks read: `unit::Word`. */
namespace unit {
inline constexpr Unit Glyph = Unit::Glyph;
inline constexpr Unit Cluster = Unit::Cluster;
inline constexpr Unit Word = Unit::Word;
inline constexpr Unit Line = Unit::Line;
inline constexpr Unit Sentence = Unit::Sentence;
}  // namespace unit

/** A deterministic generator, handed to every effect.
 *
 *  Seeded from the GLYPH's identity, so the same letter of the same text
 *  draws the same random numbers on every frame and across every
 *  relayout — a scatter that is stable is a scatter that can be cached,
 *  and one reseeded per frame would jitter forever and never settle.
 *  Reseeded fresh for each glyph, so an effect may draw as many values as
 *  it likes without the sequence depending on how many its neighbours
 *  drew.
 *
 *  Not a cryptographic generator and not a substitute for one. */
class Rng {
 public:
  explicit Rng(uint64_t seed) : m_state(seed) {}
  /** The next 32 bits (SplitMix64's finalizer over a 64-bit counter). */
  uint32_t bits() {
    m_state += 0x9e3779b97f4a7c15ull;
    uint64_t z = m_state;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return (uint32_t)((z ^ (z >> 31)) >> 32);
  }
  /** The next value in [0, 1). */
  float unit() { return (float)(bits() >> 8) * (1.0f / 16777216.0f); }
  /** The next value in [-1, 1). */
  float signedUnit() { return unit() * 2.0f - 1.0f; }
  /** The next value in [lo, hi). */
  float range(float lo, float hi) { return lo + unit() * (hi - lo); }

 private:
  uint64_t m_state;
};

/** What an effect sees for one glyph.
 *
 *  Enumeration order is stable across relayouts while the text is
 *  unchanged, and every index here is a fact about the glyph's place in
 *  THIS layout of THIS text — which is what lets an effect address "the
 *  third letter of its word" or "everything on line two" without the
 *  author counting glyphs by hand. */
struct GlyphInfo {
  size_t index = 0;    ///< glyph position in the paragraph
  size_t count = 1;    ///< total glyphs
  SkPoint rest;        ///< the glyph's laid-out origin (pen position)
  float advance = 0;   ///< the glyph's advance width
  float fontSize = 0;  ///< the glyph's font size (em-relative effects)

  uint32_t glyphInWord = 0;     ///< index of this glyph within its word
  uint32_t wordGlyphCount = 1;  ///< glyphs in that word
  uint32_t cluster = 0;         ///< UTF-16 cluster offset inside its run;
                                ///< a base and its marks share one value
  uint32_t textIndex = 0;       ///< that cluster as an offset into the text
  uint32_t wordIndex = 0;       ///< index of its word in the paragraph
  uint32_t lineIndex = 0;       ///< the flow line it landed on
  /** Index of its style span in the materialized paragraph.
   *
   *  A NUMBER FOR AN EFFECT TO READ, NOT A HANDLE TO ADDRESS BY: spans are
   *  cut and merged by every span restyle the leaf declares, so this
   *  renumbers when a `spanPaint` anywhere ahead of it splits one — and the
   *  restyle resolver runs while that list is being edited, so the two
   *  resolvers could not be made to agree on what a given index names. The
   *  handle on a treatment is the NAME the run was written under, which
   *  `sel::style` addresses and which only new content changes. */
  uint32_t styleIndex = 0;
  uint32_t sentenceIndex = 0;  ///< 0-based sentence
  /** Which beat of the track's own stagger this glyph belongs to, and how
   *  many beats there are — the unit numbering the track resolved, not a
   *  paragraph-wide count. A per-word track sees word ordinals here. */
  uint32_t unitIndex = 0;
  uint32_t unitCount = 1;
};

/** One glyph's deviation from rest — what an effect returns for local
 *  progress t ∈ [0,1]. alpha 0 skips the glyph entirely.
 *
 *  This is the type the composition algebra operates on: stacked tracks,
 *  `fx::mix`, a `fx::seq` crossfade and a `fx::keys` segment all combine
 *  GlyphMods the same way — dx/dy, rotateDeg, skewXDeg and skewYDeg ADD;
 *  scale, scaleX, scaleY, alpha and colorMul MULTIPLY; and the two
 *  SUBSTITUTIONS, `axis` and
 *  `codepoint`, are last-one-wins. Substitutions do not blend because
 *  there is no half-way glyph between two outlines: a later track that
 *  names one replaces what an earlier one named, and a `fx::seq`
 *  crossfade cuts them at the middle of its window rather than lerping.
 *  (An axis coordinate is the exception inside a crossfade: two phases
 *  driving the SAME axis lerp their values, because the face does have a
 *  continuum between them.) */
struct GlyphMod {
  float dx = 0, dy = 0;
  float scale = 1;  ///< uniform; multiplies scaleX and scaleY below
  float rotateDeg = 0;
  float alpha = 1;
  /** A per-channel multiplier over EVERY pass the glyph's style draws. A
   *  pass painting a flat colour multiplies it; a pass painting a shader
   *  takes the equivalent modulation, so a gradient keeps its ramp and
   *  wears the tint over it. White is no tint. */
  SkColor4f colorMul = {1, 1, 1, 1};
  /** Non-uniform scale and shear on each axis (degrees). An RSXform encodes
   *  a rotation and ONE scale and no shear at all, so a glyph whose composed
   *  deviation uses any of these draws under its own matrix — same passes,
   *  same paint, one canvas concat — while its neighbours keep the shared
   *  transform array.
   *
   *  The two angles read as `Element::skewX` and `Element::skewY` do:
   *  positive `skewXDeg` leans the top toward −x, positive `skewYDeg`
   *  pushes the right side toward +y, and a glyph naming both takes the
   *  single shear pair `(tan x, tan y)` rather than one shear applied after
   *  the other. */
  float scaleX = 1, scaleY = 1;
  float skewXDeg = 0, skewYDeg = 0;
  /** A variable-font axis coordinate, applied at DRAW time by swapping the
   *  glyph's face for a varied clone. The shaped positions are reused as
   *  they are, so this is sound only for an ADVANCE-INVARIANT axis: the
   *  runtime probes the glyph's face once per axis and REFUSES one that
   *  moves advances, drawing the glyph at its shaped face instead (GRAD is
   *  the advance-invariant weight; wght moves advances on most faces and
   *  belongs in the shaping style, which re-shapes). Unset: the shaped
   *  face. */
  std::optional<sigil::weave::FontVariation> axis;
  /** Draw a different code point in this glyph's place, resolved through
   *  the glyph's own shaped font and drawn at the original's pen position.
   *  Sound only for an EQUAL-ADVANCE replacement — a proportional
   *  substitution would move every letter after it and needs a reshape, not
   *  a redraw — so the runtime measures both and refuses the ones that
   *  differ, drawing the original. 0 is no substitution. */
  char32_t codepoint = 0;
};

/** The raw callable behind an effect: (glyph, local progress, rng) →
 *  deviation. Wrap one in a named `TextEffect` — the seam never holds a
 *  bare function, because a bare function cannot be compared. */
using GlyphModFn = std::function<GlyphMod(const GlyphInfo&, float, Rng&)>;

/** THE EFFECT, as a comparable value: a name, its parameters, and any
 *  operand effects it was built from.
 *
 *  Two effects are equal when they carry the same name, the same
 *  parameters, equal operands and the same curves — so `fx::rise(26) ==
 *  fx::rise(26)`,
 *  `fx::rise(26) != fx::rise(30)`, and a `fx::seq` of equal phases equals
 *  another built the same way. That equality is what lets a re-described
 *  element with unchanged tracks PRUNE instead of re-recording every
 *  frame.
 *
 *  The name is a promise about the body: equal values must produce
 *  identical deviations for identical inputs. Two different lambdas given
 *  one key compare equal and one of them will silently never be used. */
class TextEffect {
 public:
  TextEffect() = default;
  /** A named effect over parameters. `reach` is how far, in pixels, this
   *  effect may push a glyph outside the element's box — the number the
   *  recording cull grows by, so a wide scatter is not truncated.
   *
   *  `curves` are the easing functions the body reads, carried on the value
   *  so they reach the comparison: a named curve is compared by identity and
   *  a lambda compares UNEQUAL, which is the rule every other curve slot in
   *  the library follows. Leaving a curve out of this list would make an
   *  effect that reshapes its motion compare equal to the one it replaced,
   *  and the reconciler would keep drawing the old one. */
  TextEffect(std::string name, std::vector<float> params, GlyphModFn fn,
             float reach, std::vector<choreograph::EaseFn> curves = {});

  /** Evaluates the deviation. An empty effect answers the identity. */
  GlyphMod operator()(const GlyphInfo& g, float t, Rng& rng) const {
    return m_state && m_state->fn ? m_state->fn(g, t, rng) : GlyphMod{};
  }
  explicit operator bool() const { return m_state && (bool)m_state->fn; }
  /** Pixels beyond the element's box this effect may paint. */
  [[nodiscard]] float reach() const { return m_state ? m_state->reach : 0.0f; }
  [[nodiscard]] const std::string& name() const;
  [[nodiscard]] std::span<const float> params() const;
  [[nodiscard]] std::span<const TextEffect> operands() const;

  bool operator==(const TextEffect& other) const;

  /** A phase of `fx::seq` ending at local `t` — `a.until(0.35f)`. */
  [[nodiscard]] class Phase until(float t) const;

  /** Builds a composite (`fx::seq`, `fx::mix`) — the operands ride the
   *  value so the result compares by structure. */
  static TextEffect composite(std::string name, std::vector<float> params,
                              std::vector<TextEffect> operands, GlyphModFn fn,
                              float reach);

  /** A PASS EFFECT: the track's evaluation is one shader pass over the
   *  addressed units' rendered pixels, not a per-glyph deviation — the
   *  factory behind `fx::pass` (TextFx.h), where the contract is
   *  documented. The material must carry SkSL SOURCE
   *  (`Material::sksl(std::string, …)`), because the runtime bakes the
   *  unit count into the compiled shader; any other material warns once
   *  and returns an EMPTY effect, so the track draws its glyphs at rest. */
  static TextEffect pass(Material material);
  /** The pass material, or null for every per-glyph effect — what the
   *  runtime dispatches on. */
  [[nodiscard]] const Material* passMaterial() const;

 private:
  struct State {
    std::string name;
    std::vector<float> params;
    std::vector<TextEffect> operands;
    std::vector<choreograph::EaseFn> curves;
    GlyphModFn fn;
    float reach = 0;
    /** Set only by pass(): the material run over the units' layer. Held by
     *  pointer because Material is declared below this class; it rides
     *  equality by VALUE (Material::operator==), like an Effect child. */
    std::shared_ptr<const Material> pass;
  };
  std::shared_ptr<const State> m_state;
};

/** One phase of a `fx::seq`: an effect, where it ends in local time, and
 *  how long it crossfades into whatever follows. */
class Phase {
 public:
  Phase(TextEffect e)  // NOLINT: implicit by design (seq(a.until(…), b))
      : m_effect(std::move(e)) {}
  Phase(TextEffect e, float endsAt)
      : m_effect(std::move(e)), m_endsAt(endsAt) {}
  /** Lerp this phase's deviation into the next one's over the last
   *  `fraction` of local time before the joint. Default is a hard cut. */
  Phase& xfade(float fraction) {
    m_overlap = fraction;
    return *this;
  }
  [[nodiscard]] const TextEffect& effect() const { return m_effect; }
  [[nodiscard]] float endsAt() const { return m_endsAt; }
  [[nodiscard]] float overlap() const { return m_overlap; }
  bool operator==(const Phase&) const = default;

 private:
  TextEffect m_effect;
  float m_endsAt = 1.0f;
  float m_overlap = 0.0f;
};

/** WHICH GLYPHS a track addresses, as a comparable value.
 *
 *  Built from `sel::` (see below), combined with `|` (union), `&`
 *  (intersection) and `!` (complement). A default-constructed selector
 *  addresses EVERYTHING, which is what a track that names none gets.
 *
 *  Resolution happens once per (content, layout, selector) and is cached
 *  on the element, so a regular expression over a paragraph is matched
 *  when the text changes or reflows rather than once per frame. A pattern
 *  that does not compile selects nothing and warns once. */
class Selector {
 public:
  Selector() = default;  ///< everything

  /** Within EACH unit of an `sel::each` selector, keep `n` glyphs from
   *  wherever `drop()` left off. `take(n)` and `drop(n)` on their own
   *  partition every unit exactly: no glyph is in both, none is in
   *  neither. */
  [[nodiscard]] Selector take(int n) const;
  /** Within each unit, skip the first `n` glyphs and keep the rest. */
  [[nodiscard]] Selector drop(int n) const;

  [[nodiscard]] Selector operator|(const Selector& other) const;
  [[nodiscard]] Selector operator&(const Selector& other) const;
  [[nodiscard]] Selector operator!() const;
  bool operator==(const Selector& other) const;

  /** The forms a selector can take. Public because the resolver is a free
   *  function over a paragraph rather than a member. */
  enum class Kind : uint8_t {
    All,
    Word,
    Words,
    Line,
    Sentence,
    Range,
    Regex,
    Text,
    Style,
    Each,
    Union,
    Intersect,
    Complement,
  };
  struct State {
    Kind kind = Kind::All;
    uint32_t lo = 0, hi = 0;  ///< Word/Words/Line/Sentence/Range bounds
    /** Regex/Text needle, or the style NAME an `sel::style` addresses — one
     *  slot, because no selector carries two of them and a second string
     *  would ride on every selector in the tree to serve one form. */
    std::u8string pattern;
    Unit each = Unit::Glyph;  ///< Each granularity
    int take = -1;            ///< Each: glyphs kept per unit (-1 = all)
    int drop = 0;             ///< Each: glyphs skipped per unit
    std::vector<Selector> operands;
    bool operator==(const State&) const = default;
  };
  /** Null for a default-constructed (everything) selector. */
  [[nodiscard]] const State* state() const { return m_state.get(); }
  static Selector of(State s);

 private:
  std::shared_ptr<const State> m_state;
};

/** THE SELECTOR VOCABULARY. Absolute forms name a position in the text;
 *  `each` slices every unit of one granularity the same way. */
namespace sel {
/** The i-th word of the paragraph (SigilWeave's line-break units). */
[[nodiscard]] Selector word(uint32_t index);
/** Words `[lo, hi)`. */
[[nodiscard]] Selector words(uint32_t lo, uint32_t hi);
/** The i-th flow line OF THE CURRENT LAYOUT — re-resolved when the text
 *  reflows, so a narrower box moves the selection with the break. */
[[nodiscard]] Selector line(uint32_t index);
/** The i-th sentence (ICU sentence segmentation). */
[[nodiscard]] Selector sentence(uint32_t index);
/** Every glyph whose cluster falls inside a UTF-16 range of the text. */
[[nodiscard]] Selector range(sigil::weave::CharRange chars);
/** Every match of an ICU regular expression (the pattern is UTF-8). A
 *  pattern that does not compile selects NOTHING and warns once. */
[[nodiscard]] Selector regex(std::u8string_view utf8Pattern);
/** Every occurrence of a literal substring. */
[[nodiscard]] Selector text(std::u8string_view utf8Substring);
/** EVERY RUN DRESSED UNDER THIS NAME — the runs a `rich()` value added with
 *  `add(utf8, styleName)`, addressed by the name rather than by the words
 *  they happen to contain.
 *
 *  This is the treatment as a handle: a glossary set in one registered
 *  style stays addressable when the copy changes, where naming the literal
 *  text means editing the selector every time an author edits a sentence.
 *
 *  It addresses the run's TEXT, so it survives everything that changes what
 *  that text looks like: re-registering the name against a different
 *  `weave::StyleSet` entry, or a `spanPaint`/`spanStyle` cutting across it,
 *  leaves the same runs selected.
 *
 *  ONLY A NAMED `rich()` RUN CARRIES A NAME. Plain `text(utf8, style)`, a
 *  `rich()` run given a style directly, and the `shared_ptr<Paragraph>`
 *  overload have none, so this addresses nothing there — as does a name no
 *  run was written with. Either way it selects nothing and warns once per
 *  name. */
[[nodiscard]] Selector style(std::string_view name);
/** Every unit of `granularity`, ready to be sliced with `.take()` /
 *  `.drop()`. Unsliced it is the same as selecting everything. */
[[nodiscard]] Selector each(Unit granularity);
}  // namespace sel

/** WHICH LIST A CASCADE NUMBERS ITS BEATS AGAINST.
 *
 *  `Selection` numbers the units the track's OWN selector resolved: a
 *  track addressing one word beats once, whatever the paragraph's word
 *  count is. That is the right answer for a track that owns its text, and
 *  the wrong one for two tracks sharing a paragraph — their beats line up
 *  only while their selections happen to resolve lists of the same length,
 *  and the frame they stop doing so the two halves of every unit start
 *  arriving at different times with no diagnostic.
 *
 *  `Text` numbers every unit of the cascade's granularity in the whole
 *  paragraph, addressed or not, so word ten is beat ten in every track
 *  that beats over words. Two tracks that partition one paragraph then
 *  share one clock BY CONSTRUCTION rather than by coincidence. */
enum class Beats : uint8_t { Selection, Text };

/** The beat-numbering names, spelled the way a cascade reads:
 *  `beats::Text`. */
namespace beats {
inline constexpr Beats Selection = Beats::Selection;
inline constexpr Beats Text = Beats::Text;
}  // namespace beats

/** THE PER-UNIT TIME REMAP (the GSAP stagger model). The track's master
 *  progress [0,1] spans `durationMs + eachMs·(N−1)` of virtual time,
 *  where N is the number of UNITS the cascade numbers; unit i starts after
 *  its delay and runs for durationMs.
 *
 *  The unit is what makes this more than per-glyph spacing: `over =
 *  unit::Word` beats once per word, and every glyph of that word shares
 *  its beat. The default, `unit::Cluster`, is per-glyph for ordinary
 *  Latin text and keeps a base letter attached to its combining marks
 *  everywhere else.
 *
 *  The spread is EVEN unless `cueMs` names the times outright. */
struct Stagger {
  float eachMs = 30;
  /** Amount-mode (mutually exclusive with eachMs; wins when > 0): the
   *  TOTAL spread, divided across however many units there are. Use it
   *  when the budget for the whole entrance is fixed and the text may
   *  change length — `eachMs` keeps per-unit spacing and lets the total
   *  grow, this keeps the total and shrinks the spacing. */
  float amountMs = 0;
  /** AN IRREGULAR TABLE: one start time per unit, in ms from the start of
   *  the track's progress, read by unit index. Caption, lyric and lip-sync
   *  timing is a table cut against a recording, and no even spread is a
   *  substitute for one.
   *
   *  Non-empty, it REPLACES the even spread: `eachMs`, `amountMs`, `from`
   *  and `distribution` say nothing, because a table already states both
   *  the order and the shape of the cascade. Everything else this struct
   *  says still holds — `over` is still what a unit is, `durationMs` is
   *  still how long one unit's own motion lasts, and `then()` still nests a
   *  second cascade inside every beat.
   *
   *  A unit past the end of the table starts at the LAST entry, so a short
   *  table piles its tail on one beat rather than inventing times; entries
   *  past the last unit are ignored. Either mismatch warns once. Build one
   *  with `cues()`. */
  std::vector<float> cueMs;
  float durationMs = 450;
  /** Where the cascade starts. `Random` is seeded from the unit count, so
   *  a scatter is the SAME scatter on every frame and after every
   *  relayout; `Edges` starts at both ends and meets in the middle. */
  enum class From : uint8_t {
    Start,
    Center,
    End,
    Random,
    Edges
  } from = From::Start;
  /** Which units get a beat. */
  Unit over = Unit::Cluster;
  /** WHICH LIST those beats are numbered against — see `Beats`. The default
   *  numbers the track's own selection, which is what a track that owns its
   *  text means; `beats::Text` numbers the paragraph, which is what two
   *  tracks partitioning one paragraph need if they are to share a clock.
   *  A NESTED cascade takes the outer one's answer: a nested `beatsOver` is
   *  ignored, as its `durationMs` is. */
  Beats beatsOver = Beats::Selection;
  /** Shapes the START TIMES across the cascade (not the per-unit motion,
   *  which the effect and the progress own): the linear ramp of delays is
   *  passed through this curve, so an ease-in distribution crowds the
   *  early units together and lets the tail spread out. Null is the
   *  uniform spacing. */
  choreograph::EaseFn distribution = nullptr;
  /** A NESTED cascade inside each of this one's beats — see `then()`.
   *  Held out of line because a Stagger cannot contain itself by value. */
  std::shared_ptr<const Stagger> inner;

  /** Compounds a second cascade inside every beat of this one:
   *  `stagger(unit::Word, {…}).then(unit::Glyph, {…})` delays each word,
   *  then delays each glyph within its word's beat. The outer
   *  `durationMs` is ignored — a beat lasts exactly as long as the inner
   *  cascade needs. */
  Stagger& then(Unit granularity, Stagger nested);

  bool operator==(const Stagger& other) const;
};

/** Names the units a cascade beats over: `stagger(unit::Word, {.eachMs =
 *  60})`. Sugar for setting `Stagger::over`, and the spelling `then()`
 *  reads against. */
[[nodiscard]] Stagger stagger(Unit granularity, Stagger spec = {});

/** AN IRREGULAR CASCADE, from a table of start times in ms:
 *  `cues({0, 340, 720, 1180, 1600}, {.durationMs = 180})`.
 *
 *  A cue table IS a Stagger — it answers only "when does unit k start", and
 *  every other thing a cascade says (what a unit is, how long one unit's
 *  motion lasts, whether a second cascade nests inside each beat, which
 *  list the beats are numbered against) is orthogonal to that and still
 *  wanted. Being one value rather than two also keeps a track's cascade one
 *  slot, with one equality and one prune.
 *
 *  So this goes wherever `stagger()` goes, and the two compose — name the
 *  granularity with `stagger(unit::Word, cues({…}))`. Sugar for setting
 *  `Stagger::cueMs`, whose documentation states what a table shorter or
 *  longer than the unit list does. */
[[nodiscard]] Stagger cues(std::vector<float> startMs, Stagger spec = {});

/** ONE TRACK: which glyphs, what deviation, how the beats spread, and the
 *  master progress that drives it.
 *
 *  `progress` takes the full Animatable treatment — a plain constant,
 *  a `with()`/`animate()` transition (retarget-safe: each track owns its
 *  own transition slot, so retargeting the second track leaves the first
 *  alone), or a `ch::Output` binding. One-shot effects consume 0→1; loop
 *  effects read a WRAPPING bound phase. While any track's progress moves
 *  the element paints live; once every track settles it caches like a
 *  static leaf. */
struct Track {
  Selector where;    ///< default: every glyph
  TextEffect effect; /**< what it does */
  Stagger stagger;
  Animatable<float> progress = 1.0f;
  /** Pixels beyond the element's box this track may paint, which the
   *  recording cull grows by. Negative means "ask the effect", which is
   *  what every preset answers for itself; set it when a keyed lambda
   *  throws glyphs further than the default allows. Over-reporting is
   *  safe, under-reporting truncates cached output with no diagnostic. */
  float reach = -1.0f;
  /** SKIP THE SNAPPING for the glyphs this track addresses. A driven
   *  rotation, alpha, colour multiplier and axis coordinate are quantized
   *  before they reach the draw, because each distinct value is both a
   *  distinct batch bucket and a distinct glyph-atlas strike. Continuous
   *  values buy smoothness with exactly that: one strike minted per value
   *  and every addressed glyph rasterized again every frame. Set it where
   *  the steps show — a slow lift at display size, a tint sweeping along a
   *  wordmark — and nowhere else. A glyph any addressing track declares
   *  continuous is continuous. */
  bool continuous = false;

  /** How far this track really reaches: its own number when it declares
   *  one, otherwise its effect's. */
  [[nodiscard]] float reachPx() const {
    return reach >= 0 ? reach : effect.reach();
  }
  /** Structural equality, EXCLUDING `progress` — an Animatable is compared
   *  where every other animated slot is, by the reconciler. */
  bool sameShape(const Track& other) const;
  /** Full equality: the shape above plus the progress. */
  bool operator==(const Track& other) const;
};

/** ONE BEAT OF A RESOLVED CASCADE — what `Composer::beatsOf` reports.
 *
 *  A stagger is otherwise an invisible remap: it numbers units, spreads
 *  them, and tells nobody. Anything that must travel WITH a cascade and is
 *  not a glyph — a bouncing ball, a playhead, a travelling underline, a
 *  caret, a per-unit meter — then has to restate `i · eachMs` in its own
 *  arithmetic, which stops agreeing with the engine the moment the cascade
 *  nests or takes a cue table. This is the schedule read back instead. */
struct Beat {
  /** The unit's laid-out rect, in the composer's coordinate space: the
   *  axis-aligned bound of the advance boxes the layout placed for the
   *  glyphs this track addresses in this beat. It follows a wrapped line,
   *  a mixed-style run's own size, a path run's curve and a vertical
   *  column's axis, because it is read off the placement rather than
   *  measured again. */
  SkRect rect = SkRect::MakeEmpty();
  /** The OUTER unit this beat belongs to, numbered as the cascade numbers
   *  it — the track's own selection under `beats::Selection`, the whole
   *  paragraph under `beats::Text`. A nested cascade reports several beats
   *  sharing one `unitIndex`, one per inner unit inside that outer beat,
   *  which is what lets a per-word mark and a per-letter one read the same
   *  list. */
  uint32_t unitIndex = 0;
  /** When this beat opens, in ms from the start of the track's progress —
   *  the COMPOUNDED delay, outer plus inner, under a nested cascade. */
  float startMs = 0;
  /** This beat's own 0→1 at the track's progress right now — the same
   *  number the effect is being handed for these glyphs. */
  float localT = 0;
  /** The beat is running: it has begun and has not finished. */
  bool active = false;

  bool operator==(const Beat&) const = default;
};

// ---------------------------------------------------------------------------
// MIXED TEXT — one value, several styles
//
// A paragraph whose words are not all set the same way is a VALUE here, not
// a document to be marked up. There is no markup language: a run of text
// carries a style, or the name of one, and that is the whole vocabulary.
// Whatever else a passage needs — a colour on the numbers, a weight on one
// phrase — is asked for by SELECTOR after the fact
// (`Element::spanPaint` / `Element::spanStyle`), so the content stays
// content and the type treatment stays in one place.
//
// The escape hatch is unchanged: `text(std::shared_ptr<Paragraph>, options)`
// hands the engine a document built by hand, for the passage too custom for
// either of these.

/** MIXED-STYLE TEXT AS A COMPARABLE VALUE — the builder `text()` takes.
 *
 *      auto p = rich(base)
 *                   .add(u8"Signal ")
 *                   .add(u8"woven", accent)
 *                   .add(u8" through ")
 *                   .add(u8"noise", mono);
 *      text(p).fx({.effect = fx::slide(-60),
 *                  .stagger = stagger(unit::Word, {.eachMs = 120})});
 *
 *  `add(utf8)` sets a run in the base style; `add(utf8, style)` sets it in
 *  its own; `add(utf8, name)` sets it in a style looked up by NAME (see
 *  below). Runs are appended in order and concatenate into one paragraph —
 *  nothing is inserted between them, so the spaces are the author's.
 *
 *  WHY IT IS A VALUE, and what that buys over the `shared_ptr<Paragraph>`
 *  overload: two rich texts describing the same runs in the same styles are
 *  EQUAL, so a component that rebuilds its text every describe prunes
 *  exactly like a static leaf. The pointer overload cannot answer that
 *  question — a fresh `make_shared` is a fresh identity and reads as
 *  changed content every time. Internally the runs materialize into one
 *  weave `Paragraph` on the instance, rebuilt only when the described value
 *  changes; the shaping cache is content-addressed, so a rebuild that
 *  changed one run re-shapes one run.
 *
 *  NAMES resolve through a `weave::StyleSet`, which comes from one of two
 *  places: `styles()` supplies one explicitly, and `env::Provide<StyleSet>`
 *  supplies one ambiently to everything described in its scope. AN EXPLICIT
 *  SET ALWAYS WINS, whichever order the two are written in. A name the set
 *  does not register resolves to the base handed to `rich()` — the base is
 *  this text's one default, and a misspelled name shows as content set in
 *  it rather than as content that did not draw.
 *
 *  Resolution happens as the run is added (and again over every named run
 *  when `styles()` arrives), so the finished value holds real styles and
 *  depends on no scope that has since ended. */
class RichText {
 public:
  /** One run of text and the style it is set in — or one INLINE SLOT, which
   *  is a run whose content is the single object-replacement character the
   *  flow anchors a reserved box at. */
  struct Run {
    std::u8string utf8;
    sigil::weave::TextStyle style;  ///< resolved: its own, its name's, or base
    std::string styleName;          ///< the name it was written with, if any
    /** Non-empty on a SLOT run: the name a child of this text node is laid
     *  out into. */
    std::string slotKey;
    SkSize slotSize = {0, 0};    ///< the box the breakers reserve
    float slotBaselineDrop = 0;  ///< the box's bottom, below the baseline
    bool operator==(const Run&) const = default;
  };

  RichText() = default;
  /** Starts a value whose unstyled runs — and unregistered names — are set
   *  in @p baseStyle. */
  explicit RichText(sigil::weave::TextStyle baseStyle)
      : m_base(std::move(baseStyle)) {}

  /** Appends a run in the base style. */
  RichText& add(std::u8string_view utf8);
  /** Appends a run in its own style. */
  RichText& add(std::u8string_view utf8, sigil::weave::TextStyle style);
  /** Appends a run in the style registered under @p styleName. */
  RichText& add(std::u8string_view utf8, std::string_view styleName);

  /** Reserves an INLINE SLOT: `size` px of blank space woven into the flow,
   *  and the name a child Element of this text node is laid out into.
   *
   *      text(rich(body).add(u8"press ").slot("key", {28, 18}).add(u8" now"))
   *          .child(box().key("key").fill(ink).corners({4}))
   *
   *  The reserved box is ONE UNBREAKABLE WORD: a line never breaks inside
   *  it, and it moves the line's height when it is taller than the type.
   *  `baselineDrop` is how far the box's BOTTOM sits below the baseline —
   *  0 stands it on the baseline like an inline image, and about the face's
   *  descent centres a pill on the x-height.
   *
   *  The child is an ordinary subtree: it animates, caches and hit-tests
   *  like any other element, and it re-lands wherever the placeholder lands
   *  when the text reflows. It is a POSITIONED subtree — the placeholder
   *  rect is its box, so flex layout does not run inside it and its own
   *  children take explicit rects, exactly as under `positioned()`.
   *
   *  A TEXT SLOT IS NOT A MOUNT SLOT. `slot()` and `Composer::renderSlot`
   *  name a hole a HOST fills from outside the description, and those names
   *  live in one registry for the whole composition. These names live in
   *  this rich-text value alone and are matched against the `key()` of this
   *  text node's own children — so two captions may both reserve a slot
   *  called "icon" without colliding, and neither is reachable by
   *  `renderSlot`. A child keyed for a slot the content does not declare
   *  draws nothing, and says so once. */
  RichText& slot(std::string key, SkSize size, float baselineDrop = 0);
  /** Supplies the style set names resolve through, beating any the
   *  environment offers, and re-resolves every named run already added. */
  RichText& styles(sigil::weave::StyleSet set);

  /** The style unstyled runs and unregistered names are set in. */
  [[nodiscard]] const sigil::weave::TextStyle& base() const { return m_base; }
  /** The runs, in the order they were added. */
  [[nodiscard]] std::span<const Run> runs() const { return m_runs; }
  [[nodiscard]] bool empty() const { return m_runs.empty(); }

  /** Equal when the base, the runs, their resolved styles and the names
   *  they were written with all match — the question the prune asks.
   *
   *  The style SET is deliberately not compared: a name is resolved as it
   *  is added, so two values that resolved to the same styles describe the
   *  same paragraph however they got there, and an entry neither of them
   *  names cannot make them differ. */
  bool operator==(const RichText& other) const {
    return m_base == other.m_base && m_runs == other.m_runs;
  }

 private:
  sigil::weave::TextStyle m_base;
  std::vector<Run> m_runs;
  sigil::weave::StyleSet m_styles;
  bool m_hasStyles = false;       // a set is in play (explicit or inherited)
  bool m_stylesExplicit = false;  // styles() gave it; env cannot replace it
};

/** Starts a mixed-text value whose default is @p base — see RichText. */
[[nodiscard]] RichText rich(sigil::weave::TextStyle base = {});

// ---------------------------------------------------------------------------
// The shape seam — a COMPARABLE silhouette value

/** A shape scheme: `SkPath path(SkSize) const`, plus equality.
 *
 *  This is the seam-value convention the library uses throughout — one
 *  named required member and a comparable value. `Shaper` spells
 *  `shape()`, `CrossingRule` spells `decide()`, a shape value spells
 *  `path()`.
 *
 *  Equality is the point, not decoration. A shaped node can only prune —
 *  skip its dirty marking, keep its recording — if the reconciler can
 *  prove the shape is the same one, and a `std::function` cannot be
 *  compared. Every stock generator in `Shapes.h` is a scheme for that
 *  reason. A scheme's equality is a contract on the author: equal values
 *  must generate identical paths at every size. */
template <typename S>
concept ShapeScheme =
    std::equality_comparable<S> && requires(const S& s, SkSize size) {
      { s.path(size) } -> std::convertible_to<SkPath>;
    };

/** THE NODE'S SILHOUETTE, type-erased: what `Element::shape()`, a
 *  `TextPath` baseline and a `band()` spine hold.
 *
 *  Two constructions, one value:
 *
 *  - a COMPARABLE scheme (any `shapes::` generator, or your own value
 *    with `path(SkSize)` + `==`) — the node prunes while the value and
 *    its size are unchanged;
 *  - a raw callable (`[](SkSize) -> SkPath`, an `OutlineFn`) — the escape
 *    hatch. It never compares equal to a separately-constructed Shape, so
 *    the node re-patches on every describe and can never prune. That is a
 *    real per-frame cost on a node that would otherwise be static; reach
 *    for it only when no value form fits. Copies of ONE Shape do compare
 *    equal (they share state), so holding the Shape and re-using it —
 *    rather than re-minting the lambda each describe — restores pruning.
 *
 *  Held as one shared immutable pointer, so a node carrying a shape costs
 *  a pointer and a copy-on-write node copy is a refcount bump. */
class Shape {
 public:
  Shape() = default;

  template <ShapeScheme S>
    requires(!std::same_as<std::remove_cvref_t<S>, Shape>)
  Shape(S scheme) {  // NOLINT: implicit by design (.shape(shapes::star(5)))
    State state;
    state.held = scheme;
    state.equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const S&>(a) == std::any_cast<const S&>(b);
    };
    state.generate = [s = std::move(scheme)](SkSize size) {
      return s.path(size);
    };
    m_state = std::make_shared<const State>(std::move(state));
  }

  /** The escape hatch: any callable over the laid-out size. Never
   *  compares equal to a separately-constructed Shape. */
  template <typename F>
    requires(!ShapeScheme<std::remove_cvref_t<F>> &&
             !std::same_as<std::remove_cvref_t<F>, Shape> &&
             std::is_invocable_r_v<SkPath, const std::remove_cvref_t<F>&,
                                   SkSize>)
  Shape(F fn) {  // NOLINT: implicit by design (.shape([](SkSize s) {...}))
    State state;
    state.generate = std::move(fn);
    m_state = std::make_shared<const State>(std::move(state));
  }

  explicit operator bool() const { return m_state && (bool)m_state->generate; }
  SkPath operator()(SkSize size) const {
    return m_state && m_state->generate ? m_state->generate(size) : SkPath();
  }
  /** Does this value participate in structural equality? (False for the
   *  callable escape hatch.) */
  bool comparable() const { return m_state && (bool)m_state->equals; }

  /** Shared state (copies of one Shape) is equal; comparable schemes of
   *  one type compare their values; anything else is conservative. */
  bool operator==(const Shape& o) const {
    if (m_state == o.m_state) return true;
    if (!m_state || !o.m_state) return false;
    if (!m_state->equals || !o.m_state->equals) return false;
    return m_state->held.type() == o.m_state->held.type() &&
           m_state->equals(m_state->held, o.m_state->held);
  }

 private:
  struct State {
    std::function<SkPath(SkSize)> generate;
    std::any held;
    bool (*equals)(const std::any&, const std::any&) = nullptr;
  };
  std::shared_ptr<const State> m_state;
};

/** A SPATIAL PATH for a node to ride — After Effects' motion model.
 *
 *  `translateX`/`translateY` are two independent lanes. Two lanes can
 *  describe a POINT; they cannot describe a TRAJECTORY, and hand-driving a
 *  curve through them means the author computing two numbers a frame,
 *  which is imperative animation wearing declarative clothes.
 *
 *  The animatable lane here is a single float, @ref t — WHERE ALONG the
 *  curve — so the whole `bind()` chain still applies, to the SCHEDULE
 *  rather than to the geometry:
 *  `.map(&choreograph::easeInOutQuad)` eases the move in and out,
 *  `.target(0, 2)` runs two laps of a closed curve, `.window(...)` makes
 *  the move a slice of a larger phase. The curve supplies the SHAPE, the
 *  lane supplies the SCHEDULE. That separation is the whole design.
 *
 *      .travel({.path = shapes::circle(),
 *               .t = bind(&phase).map(&choreograph::easeInOutQuad).target(0,
 * 1)})
 *
 *  The rules:
 *
 *  - **The curve is resolved against the PARENT's box, not the node's.**
 *    A `Shape` is a function of a size, and the size that makes a motion
 *    path mean anything is the FRAME the node moves in — `shapes::circle()`
 *    on a 40 px dot inside a 400 px card is a 400 px orbit, not a 40 px
 *    twitch. (A root node with no parent resolves against its own box,
 *    which is the canvas.)
 *  - **The transform ORIGIN is what rides the curve.** AE moves the
 *    layer's anchor point; `transformOrigin()` is Compose's anchor point,
 *    and it is already the pivot rotate/scale/skew turn about, so the
 *    point on the curve is fixed under all of them. The default (0.5,
 *    0.5) rides the node's centre.
 *  - **PRECEDENCE: whatever the path drives, it drives outright.** It
 *    always drives position, so `translateX`/`translateY` are IGNORED
 *    while a path is engaged — not blended, not treated as an offset (a
 *    lane that half-contradicts a curve can only place the node off it).
 *    Dropping the path hands the very same lanes back, live.
 *  - **Auto-orient ADDS to `rotate()`, it does not replace it.** The
 *    tangent angle sets the base orientation and the authored rotation
 *    composes on top, as it does in AE — so `rotate(&spin)` on a
 *    travelling node spins it AS it banks.
 *  - **WRAP on a closed curve, CLAMP on an open one.** On a closed
 *    outline 0 and 1 are the same point, so `t` past 1 comes round (and
 *    negative `t` runs backwards) — that is what makes `.target(0, 2)`
 *    read as two laps with no extra API. An open curve parks at its ends.
 *    A path is closed when EVERY contour it resolved to is closed.
 *  - **ARC LENGTH is the only parameterisation.** `t` is a fraction of
 *    the path's TOTAL arc length across every contour — the same
 *    coordinate `bandPointAt`, `spans::` and `SkTrimPathEffect` speak, so
 *    a motion path and a span reveal driven by the same numbers describe
 *    the same run. There is no flag to switch it off because there is no
 *    alternative: an `SkPath` has no native parameter, only length.
 *  - **A path that resolves to no length is not engaged.**
 *
 *  Paint-only, like the lanes it outranks: a travelling node never
 *  relayouts, and its content picture replays under the new transform.
 *
 *  PRUNING follows the shape seam it is built from: a comparable scheme
 *  (any `shapes::` generator) prunes, and the raw-callable escape hatch
 *  never compares equal, so the node re-patches every describe — the same
 *  contract and the same cost `Element::shape()` documents. */
struct MotionPath {
  /** The curve, resolved against the PARENT's laid-out box. */
  Shape path;
  /** WHERE along it, as a fraction of total arc length. One float, so
   *  every `bind()`/`animate()` verb still applies. */
  Animatable<float> t = 0.0f;
  /** Auto-orient: how far ahead the node looks, in the same units as
   *  @ref t. Non-zero adds `atan2` of the chord `position(t + lookAhead)
   *  - position(t)` to `rotate()`, so a negative value faces BACK down
   *  the curve. Exactly 0 (the default, matching AE's unchecked box)
   *  leaves orientation alone. At the end of an OPEN curve, where the
   *  forward chord collapses, the last good chord is held rather than
   *  yielding a NaN angle. */
  float lookAhead = 0.0f;
};

/** Text whose BASELINE is a path (`Element::onPath`).
 *
 *  The run is shaped once — real kerning, real ligatures, real advances —
 *  and then every glyph is placed by arc length along the resolved path
 *  and rotated to its tangent, through the same batched RSXform draw
 *  kinetic text uses (one draw per font+colour, never one per glyph).
 *
 *  The alternative, placing curved lettering by hand, costs one Element
 *  and one layout PER GLYPH and loses kerning, because each glyph is laid
 *  out alone. Ring labels, dial faces, seals, compass roses, mottoes and
 *  map lettering all want this instead. */
struct TextPath {
  /** The baseline, resolved against the node's laid-out box — any
   *  `shapes::` generator, or your own. EVERY contour is walked, in order,
   *  as one continuous arc-length coordinate, so a trajectory that the
   *  frame cut into several contours still carries its whole run.
   *
   *  "The node's box" means the TEXT NODE'S OWN box, not a parent's. The
   *  tempting `disc(c, R).child(text(...).onPath(...))` resolves the ring
   *  against the text's intrinsic size and silently collapses every label
   *  into a blob. Give the TEXT node the disc's width and height instead
   *  — the text leaf is the disc. */
  Shape path;
  /** WHERE ALONG the path the run sits, as a fraction of its length. With
   *  Align::Center this is the run's midpoint.
   *
   *  One float, so every `bind()`/`animate()` verb applies — and on a
   *  CLOSED baseline the fraction WRAPS, which is the infinite marquee: a
   *  phase output running 0→1 forever walks the whole run round the loop
   *  and back to where it started, with no seam and no relayout. On an
   *  open one the run simply slides, and glyphs pushed off either end are
   *  dropped rather than piled on the last point.
   *
   *  Moving it is PAINT-ONLY. The run is shaped and broken across the
   *  path's contours once; the phase re-places the glyphs it already
   *  placed, so a marquee costs a repaint and never a reflow. It is
   *  content volatility all the same — the glyphs move inside the node's
   *  own box — so the node's recording is refused while the phase runs and
   *  taken again once it provably holds still. */
  Animatable<float> at = 0.0f;
  enum class Align { Start, Center, End };
  Align align = Align::Start;
  /** Perpendicular offset in px, positive to the LEFT of travel — which on
   *  a clockwise circle is outward. The path is the baseline, so this is
   *  how far off it the type rides. */
  float offset = 0.0f;
  /** Flip glyphs that would come out upside down, so lettering on the
   *  lower half of a ring reads right way up.
   *
   *  Default OFF, which is the engraver's convention: glyph-up points
   *  radially outward everywhere, so the bottom of a ring genuinely reads
   *  upside down. Modern signage flips; historical plates do not. */
  bool autoFlip = false;
  /** Which way a glyph faces.
   *
   *  `Tangent` is running lettering: the baseline lies ALONG the path,
   *  which is what a ring inscription or a motto wants. Note this already
   *  gives you "up points outward" on a circle — that is why a clock
   *  face's 6 comes out upside down, and why `autoFlip` exists.
   *
   *  `Radial` runs the baseline along the RADIUS instead, so the type
   *  radiates like a spoke — which is how an astrolabe limb, a compass
   *  rose and a radial axis label their divisions: you turn the
   *  instrument to read them. Without it each numeral costs one rotated
   *  Element, which is the same per-element cost onPath exists to avoid.
   *
   *  `Upright` leaves every glyph level regardless of where it sits —
   *  the convention a calendar ring or a modern gauge uses, and the one
   *  case neither of the others can reach.
   *
   *  The centre `Radial` radiates from is the resolved baseline's
   *  BOUNDING-BOX centre. That is the true centre for a full ring and
   *  silently wrong for an arc that does not span one — a quarter-arc's
   *  bbox centre is not its circle's centre — so give a partial arc a
   *  full-circle baseline and place the run on it with `at`. */
  enum class Orient { Tangent, Radial, Upright } orient = Orient::Tangent;
  /** Turn every glyph to its EXACT tangent instead of snapping the angle.
   *
   *  Snapping is the default because each distinct rotation is a distinct
   *  glyph-atlas strike: a curve whose glyphs turn continuously would
   *  re-rasterize every letter on every frame. The steps are far under a
   *  pixel of lean at label sizes on a ring whose letters sit further apart
   *  than that. Set it for STATIC artwork set large, where the steps show
   *  and nothing is paying per frame. */
  bool exactTangent = false;
};

/** Anything with paint(canvas, PaintContext) — decorations, effect
 *  bodies. An optional `bool isAnimated() const` declares per-frame
 *  volatility; see AnimatedDecoration below. */
template <typename D>
concept DecorationScheme =
    requires(const D& d, SkCanvas& canvas, const PaintContext& ctx) {
      { d.paint(canvas, ctx) };
    };

/** THE VOLATILITY DECLARATION, and the author obligation behind every
 *  automatic cache in this library.
 *
 *  A scheme that repaints differently from one frame to the next — a
 *  bound dash phase, a live material, a walk keyed to elapsed time — must
 *  say so with `bool isAnimated() const`. The library then stops caching
 *  its node's picture. Say nothing and the node is treated as static: its
 *  first frame is recorded and replayed forever, and the mark freezes with
 *  no error and no warning.
 *
 *  Nothing introspects on your behalf. By the time the composer holds it a
 *  Decoration is a type-erased value with one paint() entry point; there
 *  is no way to look inside a lambda and see that it read the clock. The
 *  value declares, or the value freezes.
 *
 *  `isAnimated()` is the one word for this question across the whole
 *  library — Material, Effect and Decoration all spell it the same way,
 *  and it is always a query derived from how the value was constructed,
 *  never a setter. */
template <typename D>
concept AnimatedDecoration = requires(const D& d) {
  { d.isAnimated() } -> std::convertible_to<bool>;
};

/** Optional on a DecorationScheme: how far it paints BEYOND the node's
 *  bounds (soft shadows, glows). The recording cull grows by the node's
 *  max bleed so cached pictures never truncate overflowing chrome. */
template <typename D>
concept BleedingDecoration = requires(const D& d) {
  { d.bleed() } -> std::convertible_to<float>;
};

/** Optional on a DecorationScheme: the FULL width of the MARK it paints,
 *  across the outline it dresses.
 *
 *  A DIFFERENT NUMBER FROM `bleed()`, and confusing them truncates
 *  pictures silently. `bleed()` is the CULL's number — how far paint
 *  escapes the node's box. This is how wide the mark is. An
 *  `Align::Inner` stroke bleeds ZERO, because it never leaves the shape,
 *  while painting a mark `width` px across; anything sized from its bleed
 *  would be far too small. Consumers that need to know where a mark IS,
 *  rather than how far it escapes, ask this one. Over-reporting either
 *  number is safe; under-reporting either one silently clips cached
 *  output. */
template <typename D>
concept ReachingDecoration = requires(const D& d) {
  { d.reach() } -> std::convertible_to<float>;
};

/** Optional on a DecorationScheme: element keys whose resolved PATHS this
 *  decoration needs (a weave's `strand::from(key)`). The element collects
 *  them at build time and the derive pass answers them into
 *  `PaintContext::borrowed`, on the same flat walk that resolves
 *  flowAround and connector/rail borrows.
 *
 *  Declared rather than introspected, for the same reason isAnimated() is:
 *  the element cannot look inside a type-erased value. A composite
 *  decoration must forward its children's keys, or their borrows resolve
 *  to nothing and they draw nothing. */
template <typename D>
concept BorrowingDecoration = requires(const D& d) {
  { d.borrows() } -> std::convertible_to<std::vector<std::string>>;
};

/** Type-erased decoration: the kernel seam the extension primitives
 *  (PathFormat, Slice, ContourWalk — see Decorations.h) plug into. A bare
 *  PaintProgram works too, at the cost of comparability — see
 *  operator== below. */
class Decoration {
 public:
  template <DecorationScheme D>
  Decoration(D scheme)  // NOLINT: implicit by design
      : m_animated([&] {
          if constexpr (AnimatedDecoration<D>)
            return scheme.isAnimated();
          else
            return false;
        }()),
        m_bleed([&] {
          if constexpr (BleedingDecoration<D>)
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()),
        m_reach([&] {
          if constexpr (ReachingDecoration<D>)
            return (float)scheme.reach();
          else if constexpr (BleedingDecoration<D>)
            return (float)scheme.bleed();  // the best available answer
          else
            return 0.0f;
        }()),
        m_borrows([&]() -> std::vector<std::string> {
          if constexpr (BorrowingDecoration<D>)
            return scheme.borrows();
          else
            return {};
        }()) {
    // Value-comparable schemes (PathFormat, Slice, Shadow…) retain a
    // comparator so the reconciler can prune a static decorated node with no
    // memo (see propsEqual). A non-comparable scheme — or a bare
    // PaintProgram — keeps none and stays conservatively unequal.
    if constexpr (std::equality_comparable<D>) {
      m_scheme = scheme;  // retained copy, compared structurally
      m_equals = [](const std::any& a, const std::any& b) {
        return std::any_cast<const D&>(a) == std::any_cast<const D&>(b);
      };
    }
    m_paint = [s = std::move(scheme)](SkCanvas& c, const PaintContext& ctx) {
      s.paint(c, ctx);
    };
  }
  Decoration(PaintProgram program)  // NOLINT: implicit by design
      : m_paint(std::move(program)) {}

  void paint(SkCanvas& canvas, const PaintContext& ctx) const {
    if (m_paint) m_paint(canvas, ctx);
  }
  /** Declared volatility, read off whichever word the scheme spelled. */
  bool isAnimated() const { return m_animated; }
  float bleed() const { return m_bleed; }
  /** FULL width of the mark this decoration paints, across the outline it
   *  dresses (see ReachingDecoration). Falls back to bleed(), then to 0. */
  float reach() const { return m_reach; }
  /** Keyed elements whose resolved paths this decoration reads (see
   *  BorrowingDecoration). Empty for everything that borrows nothing. */
  const std::vector<std::string>& borrows() const { return m_borrows; }

  /** Structural equality, which is what lets a decorated node prune with
   *  no memo around it: true only when both wrap the same value-comparable
   *  scheme type and those values compare equal.
   *
   *  A bare PaintProgram, or a scheme without operator==, ALWAYS compares
   *  unequal. That is conservative and correct — the library cannot prove
   *  a callable is the same drawing — but it has a cost: such a node is
   *  re-patched and re-recorded on every describe, forever. Prefer a value
   *  scheme for static chrome, or wrap the node in memo(). */
  bool operator==(const Decoration& o) const {
    return m_equals && o.m_equals && m_scheme.type() == o.m_scheme.type() &&
           m_equals(m_scheme, o.m_scheme);
  }

 private:
  bool m_animated = false;
  float m_bleed = 0.0f;
  float m_reach = 0.0f;
  std::vector<std::string> m_borrows;
  PaintProgram m_paint;
  std::any m_scheme;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

/** A named bundle of decorations applied together — the Photoshop "layer
 *  style" as a value. Presets (styles::aquaGel(), styles::y2kChrome())
 *  return one; Element::style() splices it in: `under` layers paint below
 *  the fill/content (drop shadows, body ramps), `over` layers above
 *  (gloss lenses, bevels, keylines). One call dresses the node. */
struct LayerStyle {
  std::vector<Decoration> under;
  std::vector<Decoration> over;
};

// ---------------------------------------------------------------------------
// The stroke grammar — WHERE a stroke goes
//
// The words: SHAPE is the region an element occupies (Element::shape);
// LINE is an element whose shape is an open path; BAND is a derived shape
// around a spine (band(), below); STROKE is the slot that dresses a
// boundary, and BRUSH is what paints. "Frame" and "border" are not
// concepts — they are strokes of a boundary; "bounding box" is
// query-side vocabulary (Composer::bounds), never a shape.

/** One claimed run of a boundary, as fractions of its TOTAL arc length —
 *  every contour end to end. This is `SkTrimPathEffect`'s coordinate, and
 *  the one every span, reveal and motion path in the library speaks. */
struct Span {
  float begin = 0.0f, end = 1.0f;
  bool operator==(const Span&) const = default;
};

/** What a Spans value is resolved against. `fitRects` are the derive
 *  pass's answers for spans::fit(), keyed; `values` holds the resolved
 *  animatable endpoints in declaration order — THREE per term, `begin`,
 *  `end`, `offset` — which is how a reveal can be a transition or a
 *  binding without Spans knowing about either. Short arrays are
 *  tolerated: a missing slot reads its default, so a caller that only
 *  cares about endpoints may pass two. */
struct SpanInput {
  const SkPath* outline = nullptr;
  const std::vector<std::pair<std::string, SkRect>>* fitRects = nullptr;
  const std::vector<float>* values = nullptr;
};

/** WHERE a stroke pass goes: a comparable value built by the `spans::`
 *  factories and combined with `|` (union).
 *
 *  **FRACTION 0 IS THE BOTTOM-LEFT CORNER** on a box, and the boundary
 *  runs UP the left edge from there. That is `SkPath::addRRect`'s own
 *  convention (start index 3, clockwise in Skia's y-down space), inherited
 *  unchanged. Anything reasoning about WHERE a fraction lands needs it:
 *  `upTo(0.25)` on a square claims the LEFT edge, not the top one. A
 *  custom `shape()` seams wherever its own path starts.
 *
 *  Deliberately a CLOSED vocabulary rather than an open seam. The seam
 *  convention — one named required member on a comparable value — governs
 *  shapers, profiles and crossing rules, whose whole point is that a user
 *  writes new ones. A span is an interval set instead: richer values such
 *  as `kit::spans::brackets` are COMPOSITIONS of these terms, not new
 *  kinds, so the value stays trivially comparable and prunable. */
class Spans {
 public:
  enum class Rule : uint8_t {
    Range,    ///< [begin, end] outright — and upTo(t) is range(0, t)
    Wrap,     ///< [begin, end] on the boundary read as a CYCLE (spans::wrap)
    Corners,  ///< a window of `arm` px either side of every tangent break
    Edges,    ///< everything EXCEPT within `arm` px of a break
    Every,    ///< `count` equal slots, each claiming its leading `duty`
    At,       ///< one slot (`index`) of `count`
    Fit,      ///< the run the keyed element covers, grown by `margin` px
    Rest,     ///< the complement (see Element::stroke)
  };
  /** One term of the union. Only the members its Rule reads are
   *  meaningful; the rest keep their defaults so the value compares. */
  struct Term {
    Rule rule = Rule::Range;
    Animatable<float> begin = 0.0f, end = 1.0f;
    /** Added to BOTH endpoints before the interval is read. Read by Range
     *  and Wrap only; other rules ignore it.
     *
     *  It exists for the one case endpoint arithmetic cannot spell: a
     *  window whose ENDS are driven by one Output and whose POSITION is
     *  driven by another. A bound endpoint holds exactly one source
     *  pointer, and summing two live values into one number needs two. */
    Animatable<float> offset = 0.0f;
    float arm = 0.0f;          ///< Corners/Edges: px of arc length
    float angleDeg = 30.0f;    ///< Corners/Edges: the tangent break that counts
    float duty = 1.0f;         ///< Every: fraction of each slot claimed
    float margin = 0.0f;       ///< Fit: px grown around the keyed content
    int count = 1, index = 0;  ///< Every/At
    std::string key;           ///< Fit: the content key; Rest: the pass name
  };
  std::vector<Term> terms;

  /** SLIDE THE WHOLE CLAIM: `by` is added to both endpoints of every
   *  Range/Wrap term. Same value kind as the endpoints themselves, so it
   *  may be a constant, an `animate(...)` or a bound Output.
   *
   *      .stroke(spans::wrap(&start, &end).offset(&drift), ants)
   *
   *  Endpoint arithmetic (`bind(&o).offset(w)`) covers every case where
   *  ONE Output drives the window; this covers the case where the ends
   *  and the position are driven independently.
   *
   *  Three things about the call, all of them easy to assume wrongly:
   *  - it MUTATES and returns `*this` by reference, so it chains off a
   *    temporary safely only while that temporary lives — bind the result
   *    to a `Spans` value (or pass it straight to `stroke()`, which takes
   *    one by value) rather than to `auto &`;
   *  - it applies to the terms PRESENT AT CALL TIME, so
   *    `range(a,b).offset(o) | corners(8)` offsets only the range, while
   *    `(range(a,b) | corners(8)).offset(o)` writes both (the corner term
   *    ignores it);
   *  - on an empty value it does nothing, silently — there is no term to
   *    carry the offset and nothing to warn about. */
  Spans& offset(Animatable<float> by);

  /** Structural equality. Declared here and defined beside the
   *  reconciler's own property comparator, so an animated endpoint
   *  compares the way every other animated property does — by binding
   *  identity when live, by value when described. */
  bool operator==(const Spans& other) const;

  /** Resolve to intervals. Rest terms return nothing — the complement
   *  needs the element's OTHER passes and is computed by the painter. */
  std::vector<Span> resolve(const SpanInput& in) const;
  /** How many floats `SpanInput::values` must carry: begin, end and
   *  offset, in that order, for every term. */
  size_t valueCount() const { return terms.size() * 3; }
  bool hasRest() const {
    for (const Term& t : terms)
      if (t.rule == Rule::Rest) return true;
    return false;
  }
};

/** Union. `spans::corners(18) | spans::at(0, 4)` is one pass. */
inline Spans operator|(Spans a, const Spans& b) {
  a.terms.insert(a.terms.end(), b.terms.begin(), b.terms.end());
  return a;
}

/** The span factories — the WHERE half of `.stroke(where, what)`. */
namespace spans {
/** `[begin, end]` of the boundary's arc length. Both ends take the full
 *  Animatable treatment (constant, `animate(...)`, or a bound Output). */
Spans range(Animatable<float> begin, Animatable<float> end);
/** THE SEAM-CROSSING RANGE: the boundary read as a CYCLE, so a window
 *  whose `begin` is past its `end` claims [begin,1] AND [0,end] — the
 *  marching-ants and orbiting-comet idiom.
 *
 *      .stroke(spans::wrap(bind(&phase), bind(&phase).offset(0.25f)), ants)
 *
 *  Both ends take the full Animatable treatment, so the window marches by
 *  driving them; two shaped bindings on ONE Output are how a fixed-length
 *  window is spelled.
 *
 *  A DEDICATED TERM rather than `range()` learning to wrap, for two
 *  reasons. `range(0.9, 0.1)` is legal and means the empty/reversed
 *  window that endpoint normalisation swaps, so teaching it to wrap would
 *  silently change what existing descriptions draw. And the no-overlap
 *  law reads over RESOLVED runs, where this is the only term that yields
 *  two runs from one pair of endpoints — a reader tracking down a claim
 *  conflict needs the call site to say that the term is cyclic.
 *
 *  DEGENERATE ENDS are read from the RAW endpoints, before the fractional
 *  wrap: `end - begin <= 0` claims nothing and `>= 1` claims the whole
 *  boundary, which is why a window driven past 1.0 keeps its length. The
 *  seam itself (fraction 0) is the outline's own start point, and a
 *  seam-crossing claim is stitched into ONE contour so caps and additive
 *  brushes never double-hit there. */
Spans wrap(Animatable<float> begin, Animatable<float> end);
/** THE REVEAL: `range(0, end)`. `spans::upTo(animate(from(0.f).to(1.f),
 *  {600ms}))` is a stroke that DRAWS ON, and a bound Output scrubs it.
 *  Works the same way under every brush, because it claims a run of the
 *  boundary rather than modifying the mark. */
Spans upTo(Animatable<float> end);
/** A window of `arm` px of arc length either side of every tangent break
 *  — the four corner L's, and the reticle bracket vocabulary. Follows any
 *  silhouette: chamfer the shape and the marks move to the chamfers.
 *  `angleDeg` is what counts as a break. A regular n-gon turns 360/n at
 *  each vertex, so at the 30° default nothing above 12 sides registers a
 *  corner at all — lower the angle for rounder silhouettes. The scan
 *  warns rather than adapting the threshold for you. */
Spans corners(float arm, float angleDeg = 30.0f);
/** The complement of corners(): the runs BETWEEN the breaks, stopping
 *  `arm` px short of each — the rule with open corners. */
Spans edges(float arm, float angleDeg = 30.0f);
/** `count` equal slots around the boundary, each claiming the leading
 *  `duty` of its own slot. `duty == 1` tiles the boundary completely. */
Spans every(int count, float duty = 1.0f);
/** One slot of `count` — `every()`'s singular. Positional, so it moves
 *  when the geometry changes; use it on settled compositions. */
Spans at(int index, int count);
/** The run the KEYED element covers, grown by `margin` px: a gap sized
 *  from content, resolved in the derive phase against that element's
 *  resolved box (the flowAround pattern, applied to a boundary). */
Spans fit(std::string_view key, float margin = 0.0f);
/** Everything this element's other CLAIMING passes left over. */
Spans rest();
/** The complement of ONE named pass — and, unlike bare rest(), allowed
 *  to overlay other passes on purpose. */
Spans rest(std::string_view passName);
}  // namespace spans

// ---------------------------------------------------------------------------
// THE MASKING FAMILY — `mask(by::…)` and `mask(parts::…, by::…)`
//
// Appearance-gating is a relation between a SELECTION and a GATE, and the
// two questions are independent:
//
//   parts::  says WHICH of this node's paint outputs the mask applies to
//   by::     says HOW that paint arrives — the rule by which it is cut
//
// Keeping them independent is what makes the family small: any selection
// combines with any gate, so a region cut, an arc-length reveal, a
// coverage matte and a per-mark cut are one mechanism rather than four.
//
// TWO LAWS, and they are the whole semantics:
//
//  - **A gate is a SHOW set.** `by::edge(90, 0.3)` shows 30%;
//    `spans::upTo(t)` shows [0,t]. The complement is a separate TERM,
//    never a mode flag — `by::outside(r)` is the word for the outside of
//    a region — so which way round a mask runs is readable at the call
//    site without chasing an argument.
//  - **Stacked masks INTERSECT where their selections overlap.** Both must
//    pass. Nesting already means this everywhere else (clip inside clip,
//    a span claim under a whole-node cut), and UNION is spelled inside one
//    gate value (combining spans with `|`), never across gates. Each mask keeps
//    its OWN animation slots, so three masks on one node may run at three
//    different rates and the intersection is exact per frame.

/** A COMPARABLE region in the node's own local space — the shape gate's
 *  value, and deliberately a closed vocabulary rather than an
 *  `std::function<SkPath(SkSize)>`.
 *
 *  A callable would defeat the point of the gate. A gate is read live,
 *  every frame; an incomparable generator never participates in reconciler
 *  equality, so a masked node could never prune and would re-record
 *  forever. A Region is a value: it compares, it prunes, and a node masked
 *  by one still qualifies for the memo that lets an animated-scalar node
 *  hold its recording between ticks.
 *
 *  `own()` is the node's own shape — the region `clip()` uses, and the
 *  reason clip() survives as sugar over this. */
class Region {
 public:
  enum class Kind : uint8_t {
    Own,   ///< the node's own shape() / corners box
    Rect,  ///< a rectangle in local coordinates
    Oval,  ///< the oval inscribed in a local rectangle
    Path,  ///< an explicit local path (SkPath is a comparable value)
  };

  /** The node's own silhouette — clip()'s region, as a value. */
  static Region own();
  static Region rect(const SkRect& r);
  static Region oval(const SkRect& bounds);
  /** An explicit path in the node's LOCAL space. Comparable (SkPath has
   *  structural equality), so this is the general escape hatch that still
   *  prunes — unlike a generator. */
  static Region path(SkPath p);

  Kind kind() const { return m_kind; }
  bool operator==(const Region& other) const;

  /** The path this region covers, given the node's own silhouette. */
  SkPath resolve(const SkPath& ownShape) const;

 private:
  Kind m_kind = Kind::Own;
  SkRect m_rect = SkRect::MakeEmpty();
  SkPath m_path;

  /** FIELD PIN (see ComposeInternal.h's FIELD PINS block). A Region rides
   *  inside a mask gate, which is read LIVE every frame — a region that
   *  compares equal when it isn't leaves a pruned node revealing to its
   *  first frame forever. */
  static void fieldPin(Region& v) {
    auto& [kind, rect, path] = v;
    static_assert(
        std::tuple_size_v<decltype(std::tie(kind, rect, path))> == 3,
        "Region gained or lost a member — rule on it in Region::operator== "
        "(Compose.cpp, in the arm of the Kind that reads it), then bump this "
        "count.");
  }
};

/** WHICH of a node's paint outputs a mask applies to — a small comparable
 *  value, combined with `|`.
 *
 *  The outputs, in paint order (`Paint.cpp`'s own list):
 *
 *      backgrounds · background passes │ fill · echo │ overlays │
 *      content │ children │ foregrounds · foreground passes
 *
 *  which collapse into four classes an author can name: the SURFACE (the
 *  fill and its echoes), the MARKS (every decoration in every slot and
 *  every span pass, across BOTH z-halves — a boundary does not have two of
 *  itself), the CONTENT leaf, and the CHILDREN.
 *
 *  `named()` addresses ONE mark by the local label its slot call gave it.
 *  Those are the same LOCAL names `stroke(Spans, what, name)` carries:
 *  one element's labels for its own marks, for inspection and
 *  intra-element reference. They are NOT query keys — `Composer::bounds`
 *  and `hitTest` do not see them — because a second identity system beside
 *  `key()` is exactly what the query side refuses.
 *
 *  A name that matches nothing selects nothing, silently, exactly as
 *  `spans::rest("unknown")` and `spans::fit("unknown")` do. */
class Parts {
 public:
  enum Bits : uint8_t {
    kSurface = 1u << 0,
    kMarks = 1u << 1,
    kContent = 1u << 2,
    kChildren = 1u << 3,
    kAll = kSurface | kMarks | kContent | kChildren,
  };
  uint8_t bits = 0;
  /** parts::named(): local mark labels, in declaration order. */
  std::vector<std::string> names;

  bool operator==(const Parts&) const = default;
  bool selects(Bits what) const { return (bits & what) != 0; }
  /** Does this selection reach a mark carrying `label` (possibly empty)? */
  bool selectsMark(std::string_view label) const {
    if (bits & kMarks) return true;
    if (label.empty()) return false;
    for (const std::string& n : names)
      if (n == label) return true;
    return false;
  }
  /** Everything this node paints — the one-argument mask()'s selection. */
  bool isEverything() const { return (bits & kAll) == kAll; }
};

/** Union: `parts::content() | parts::surface()`. */
inline Parts operator|(Parts a, const Parts& b) {
  a.bits = (uint8_t)(a.bits | b.bits);
  a.names.insert(a.names.end(), b.names.begin(), b.names.end());
  return a;
}

/** The selection factories — the WHICH half of `mask(what, with)`. */
namespace parts {
/** Every output, children included. The one-argument `mask(by::…)` means
 *  this, and it is what the docs lead with. */
Parts all();
/** Every decoration in every slot and every span pass, BOTH z-halves —
 *  backgrounds, overlays, foregrounds, background passes, stroke passes. */
Parts marks();
/** The fill surface (and its echo re-stamps). */
Parts surface();
/** The text / image / custom leaf. */
Parts content();
Parts children();
/** ONE mark, by the local name its slot call gave it. */
Parts named(std::string_view name);
}  // namespace parts

class Gate;

/** The gate factories — the HOW half of `mask(what, with)`. Named `by::`
 *  because the call site reads as English: mask by edge, mask by spans,
 *  mask by shape. */
namespace by {
/** ARC LENGTH along the node's boundary — the same `Spans` value the
 *  stroke slot uses, here answering "how much of this exists yet" rather
 *  than "where does this pass go".
 *
 *      .mask(by::spans(spans::upTo(animate(from(0.f).to(1.f), {600ms}))))
 *
 *  A boundary is a 1-D coordinate, so this gate addresses only the paint
 *  that TRACES the boundary — the surface and the marks. Selecting content
 *  or children with it means nothing and DOES NOTHING, silently. */
Gate spans(Spans where);
/** A MOVING STRAIGHT EDGE at `angleDeg` across the node's laid-out box,
 *  showing the fraction lying before it (0 = left-to-right, 90 = top to
 *  bottom, 180 = right-to-left, 270 = from the bottom). This is the gate
 *  that reveals a filled surface by EXTENDING it — an arc-length window
 *  walks the perimeter instead, and scaleX/scaleY squash rather than
 *  reveal. */
Gate edge(float angleDeg, Animatable<float> fraction);
/** A REGION of the node's local space, kept. `by::shape(Region::own())` is
 *  what `clip()` does. */
Gate shape(Region r);
/** …and its complement: everything OUTSIDE the region. Two masks
 *  intersect, so a set difference is `by::shape(a)` and `by::outside(b)`
 *  on one node. */
Gate outside(Region r);
/** A COVERAGE SOURCE: the selected paint keeps the Material's ALPHA — the
 *  soft-edged mask (a gradient fade, a noise dissolve, a stencil sprite).
 *
 *  Costs a `saveLayer` per masked group, so it is the expensive member of
 *  the family; `spans`, `edge` and `shape` ride path effects and clips. */
Gate alpha(Material coverage);
/** …and its complement, a term of its own exactly as `outside` is: the
 *  selected paint keeps what the Material does NOT cover. After Effects'
 *  Alpha Inverted Matte. Costs nothing beyond `alpha` — the coverage layer
 *  composites with `kDstOut` instead of `kDstIn`, which is `1 - a` exactly
 *  and needs no shader. */
Gate alphaOut(Material coverage);
/** The other coverage source: the selected paint keeps the Material's
 *  LUMA. After Effects' Luma Matte — paint a matte in greys (or in
 *  anything) and its brightness is the coverage.
 *
 *  **The luma law**: `Y' = 0.299 R' + 0.587 G' + 0.114 B'` — Rec. 601
 *  coefficients on the ENCODED values, taken on the PREMULTIPLIED colour.
 *  Compose paints into surfaces with no colour space attached, so a
 *  shader's channels are the display-encoded numbers the author wrote and
 *  there is no linear stage to weight. Rec. 601's luma coefficients are
 *  the set defined on gamma-encoded R'G'B'; Rec. 709's 0.2126/0.7152/
 *  0.0722 are LUMINANCE coefficients defined on linear light and do not
 *  belong here. Premultiplied means a TRANSPARENT matte reads as black
 *  and hides, as AE's does — a half-transparent white and an opaque 50%
 *  grey are the same matte.
 *
 *  Same cost as `alpha` plus one SkSL pass over the coverage layer, and
 *  none at all when the Material resolves to a colour, where the
 *  weighting is one dot product in C++. */
Gate luma(Material coverage);
/** …and ITS complement: the selected paint keeps what the Material's luma
 *  leaves DARK. After Effects' Luma Inverted Matte. */
Gate lumaOut(Material coverage);
}  // namespace by

/** HOW paint arrives past a mask — a comparable value built by the `by::`
 *  factories. Only the members its Kind reads are meaningful; the rest
 *  keep their defaults so the value compares. */
class Gate {
 public:
  enum class Kind : uint8_t { Spans, Edge, Shape, Coverage };
  /** Coverage: WHICH channel of the Material becomes coverage. The two
   *  members are one mechanism — the same `saveLayer` and the same
   *  compositing pass — so they are a field of one Kind and not two Kinds.
   *  See `by::alpha` / `by::luma` for the law each names. */
  enum class Channel : uint8_t { Alpha, Luma };
  Kind kind = Kind::Spans;
  Spans where;                        ///< Spans
  float angleDeg = 0.0f;              ///< Edge
  Animatable<float> fraction = 1.0f;  ///< Edge
  Region region;                      ///< Shape
  /** Shape AND Coverage: keep the COMPLEMENT of what this gate names —
   *  `by::outside`, `by::alphaOut`, `by::lumaOut`. One field because it is
   *  one question ("which side of the show set?"), asked of two kinds. */
  bool outside = false;
  Channel channel = Channel::Alpha;  ///< Coverage
  /** Coverage. Held out of line because Material is declared in its own
   *  header, which includes this one. */
  std::shared_ptr<const Material> coverage;

  /** Structural equality. Declared here and defined beside the
   *  reconciler's own property comparator, so an animated fraction
   *  compares the way every other animated property does. */
  bool operator==(const Gate& other) const;
  /** How many animatable floats this gate contributes, in the order
   *  `Instance::maskAnims` indexes them: three per Spans term (begin, end,
   *  offset), one for an Edge fraction, none for Shape or Coverage (a
   *  Region is static and a Material animates itself). */
  size_t valueCount() const;
};

/** One mask: a selection and a gate. */
struct Mask {
  Parts what;
  Gate with;
  bool operator==(const Mask& other) const {
    return what == other.what && with == other.with;
  }
};

// ---------------------------------------------------------------------------
// The profile seam — how far a mark sits ACROSS its spine

/** A profile value: `float across(float along) const`, `float max()
 *  const`, and EQUALITY. Both extra members are required, and both are
 *  load-bearing.
 *
 *  `max()` is what every cull and bleed calculation is sized from. A
 *  varying width whose reach cannot be asked for can only be clipped, and
 *  clipping in a cached picture is silent.
 *
 *  Equality is required because a profile is read LIVE, every frame.
 *  Anything an author hands the library must participate in reconciler
 *  equality, or a node that prunes goes on reading the value it was
 *  described with and never sees the new one. An incomparable callable is
 *  therefore not a profile; write a struct with `operator==`.
 *
 *  A PROFILE THAT RETURNS A NON-FINITE WIDTH DELETES THE WHOLE BAND. One
 *  NaN vertex makes the built path non-finite and Skia draws none of it,
 *  with no error. The seam does not guard this — clamp inside your own
 *  law. Trigonometric laws are the usual source: `sqrt(sin(pi*along))` is
 *  NaN at `along == 1` because the float pi rounds up.
 *
 *  `along` is a fraction of the spine's arc length; `across` is px on its
 *  normal, positive to the LEFT of travel — see bandPointAt for the one
 *  statement of that convention. */
template <typename P>
concept ProfileScheme =
    std::equality_comparable<P> && requires(const P& p, float along) {
      { p.across(along) } -> std::convertible_to<float>;
      { p.max() } -> std::convertible_to<float>;
    };

/** THE PX KEY — optional, one line.
 *
 *  A scheme that declares `static constexpr bool alongIsPx = true` is
 *  keyed in PX OF ARC LENGTH from the spine's start rather than in a
 *  fraction of it. Consumers that have measured their spine
 *  (`profileOffset`, the band's rails) hand it `along * lengthPx` through
 *  `Profile::acrossAt`. Nothing else about the seam changes, and a scheme
 *  that says nothing stays fraction-keyed.
 *
 *  WHY IT EXISTS. A decoration under a reveal (`spans::upTo`, a span
 *  gate) is handed the REVEALED contour, so a fraction is a fraction of
 *  what has been drawn SO FAR: a law keyed to it SLIDES along the mark as
 *  the reveal grows. That looks identical in a still frame and wrong in
 *  motion. Absolute distance from the start does not move, which is what
 *  a calligraphic pressure law or a flow-width law actually means.
 *
 *  The conversion cannot live in the author's value, because it needs the
 *  length of the contour ACTUALLY being painted and only the paint-time
 *  consumer knows that. So the seam converts, once, for every consumer. */
template <typename P>
concept PxKeyedProfileScheme = ProfileScheme<P> && requires {
  { P::alongIsPx } -> std::convertible_to<bool>;
};

/** Type-erased comparable profile — Decoration's pattern applied to the
 *  width seam. One shared vocabulary: a band's taper, a weave strand's
 *  offset and a ribbon's width are all this same value. */
class Profile {
 public:
  template <ProfileScheme P>
  Profile(P scheme)  // NOLINT: implicit by design (across(myTaper))
      : m_max((float)scheme.max()) {
    if constexpr (PxKeyedProfileScheme<P>) m_alongIsPx = P::alongIsPx;
    // The concept requires equality, so every profile keeps a comparator —
    // there is no conservatively-unequal fallback here, unlike Decoration.
    m_held = scheme;
    m_equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
    };
    m_across = [s = std::move(scheme)](float along) { return s.across(along); };
  }
  Profile() = default;

  /** The law at `along`, IN THE PROFILE'S OWN KEY — a fraction of the
   *  spine normally, px of arc length when `keyedInPx()`. A consumer that
   *  has measured its spine should call `acrossAt` instead and never think
   *  about which. */
  float across(float along) const { return m_across ? m_across(along) : 0.0f; }
  /** The law at `along`, ALWAYS a fraction of the spine, given the spine's
   *  measured length in px. The one call `profileOffset` and the band's
   *  rails make: it is the bridge that lets a px-keyed law stay put under
   *  a reveal (see PxKeyedProfileScheme). */
  float acrossAt(float along, float lengthPx) const {
    return across(m_alongIsPx ? along * lengthPx : along);
  }
  /** Is this profile's law keyed in px of arc length rather than in
   *  fraction? Part of the value's TYPE, so it never differs between two
   *  profiles that compare equal. */
  bool keyedInPx() const { return m_alongIsPx; }
  /** The widest this profile ever reaches — what bleed and cull are
   *  computed from, so nothing it draws is silently truncated. */
  float max() const { return m_max; }
  bool operator==(const Profile& o) const {
    // Reflexive on the DEFAULT-CONSTRUCTED value too: two empty profiles
    // are the same nothing, and a value that does not compare equal to
    // itself makes every containing description patch forever.
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  float m_max = 0.0f;
  bool m_alongIsPx = false;
  std::function<float(float)> m_across;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

/** The core profile presets: the two every other profile is defined
 *  against. Richer families — the oscillating `wave`, and `braid` built on
 *  it — live in kit, since the kernel only needs to hold the seam. */
namespace strand {
/** across ≡ 0: the boundary itself. */
struct Self {
  float across(float) const { return 0.0f; }
  float max() const { return 0.0f; }
  bool operator==(const Self&) const = default;
};
/** across ≡ px: a parallel. Parallels are rails — they never cross.
 *
 *  **Positive is LEFT of travel**, which is outside a clockwise path.
 *  `kit::brush::shapers::offset`, `lines::offsetAcross`,
 *  `lines::Rail::across`, `Profile::across` and `TextPath::offset` all
 *  mean this same side; see bandPointAt. */
struct Offset {
  float px = 0.0f;
  float across(float) const { return px; }
  float max() const { return std::abs(px); }
  bool operator==(const Offset&) const = default;
};
inline Profile self() { return Profile(Self{}); }
inline Profile offset(float px) { return Profile(Offset{px}); }
}  // namespace strand

/** The band's width, named at the call site: `band(spine, across(22))`.
 *  Takes a constant or any Profile (a taper, a kit oscillation). */
struct Across {
  Profile profile;
  bool operator==(const Across&) const = default;
};
inline Across across(float px) { return Across{strand::offset(px)}; }
inline Across across(Profile p) { return Across{std::move(p)}; }

/** Which side of the spine the band occupies. Explicit because the
 *  offset-path lineage has no defensible default beyond "both". */
enum class Formation : uint8_t { Centered, Outward, Inward };

/** A band spine borrowed from another element's resolved shape, through
 *  the derive phase: `band(around("dial"), across(14))`. */
struct Around {
  std::string key;
  bool operator==(const Around&) const = default;
};
inline Around around(std::string_view key) { return Around{std::string(key)}; }

// ---------------------------------------------------------------------------
// The shaper seam — the ONE way geometry deviates

/** A shaper value: `SkPath shape(const SkPath &) const`, plus equality.
 *
 *  It bends ONE CONTINUOUS MARK — a wave, a zigzag, a jitter, an offset —
 *  and that is the whole of the geometry-deviation vocabulary. Building a
 *  mark out of repeated CELLS instead is a pattern, which is a brush kind
 *  rather than a shaper; the two are named apart because they compose
 *  differently.
 *
 *  SkPath in, SkPath out: dash and width are path operations, so nothing
 *  richer is needed. `bleed()` is optional and declares how far the
 *  deviation reaches (a wave's amplitude), so the paint cull can grow by
 *  it and a cached picture is not truncated.
 *
 *  There are deliberately no sugar methods over this seam. Stock shapers
 *  are ordinary kit values (`kit::brush::shapers::`), peers of anything
 *  you write — which is what a seam is for. */
template <typename S>
concept ShaperScheme =
    std::equality_comparable<S> && requires(const S& s, const SkPath& p) {
      { s.shape(p) } -> std::convertible_to<SkPath>;
    };

/** Type-erased comparable shaper. */
class Shaper {
 public:
  template <ShaperScheme S>
  Shaper(S scheme)  // NOLINT: implicit by design (.shaped(myWave))
      : m_bleed([&] {
          if constexpr (requires {
                          { scheme.bleed() } -> std::convertible_to<float>;
                        })
            return (float)scheme.bleed();
          else
            return 0.0f;
        }()) {
    m_held = scheme;
    m_equals = [](const std::any& a, const std::any& b) {
      return std::any_cast<const S&>(a) == std::any_cast<const S&>(b);
    };
    m_shape = [s = std::move(scheme)](const SkPath& p) { return s.shape(p); };
  }
  Shaper() = default;

  SkPath shape(const SkPath& p) const { return m_shape ? m_shape(p) : p; }
  float bleed() const { return m_bleed; }
  bool operator==(const Shaper& o) const {
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  float m_bleed = 0.0f;
  std::function<SkPath(const SkPath&)> m_shape;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

// ---------------------------------------------------------------------------
// Strands — WHERE a composite's marks run

/** Where one strand of a composite runs. Two families:
 *
 *  RELATIVE — a displacement of the stroked boundary in its (along,
 *  across) frame, **the same frame a band owns** (across positive to the
 *  LEFT of travel; see bandPointAt). Any Profile is one: `strand::self()`
 *  rides the boundary, `strand::offset(px)` runs parallel, and a custom
 *  profile value is accepted here directly.
 *
 *  ABSOLUTE — `strand::from(key)` borrows a keyed element's resolved path
 *  through the derive phase, and `strand::path(p)` is authored geometry
 *  (SkPath is comparable, so it prunes). **With only absolute strands the
 *  boundary is an unpainted host** — nothing runs on it, which is a real
 *  and useful shape of composite.
 *
 *  Paths are DATA: a path participates as an element's shape, as borrowed
 *  geometry, or as pure guide data in no tree. This is the third case. */
class StrandPath {
 public:
  enum class Source : uint8_t { Relative, Borrowed, Authored };

  StrandPath() = default;
  StrandPath(Profile p)  // NOLINT: implicit by design (.path = strand::self())
      : m_source(Source::Relative), m_profile(std::move(p)) {}
  static StrandPath borrowed(std::string key) {
    StrandPath s;
    s.m_source = Source::Borrowed;
    s.m_key = std::move(key);
    return s;
  }
  static StrandPath authored(SkPath path) {
    StrandPath s;
    s.m_source = Source::Authored;
    s.m_path = std::move(path);
    return s;
  }

  Source source() const { return m_source; }
  const Profile& profile() const { return m_profile; }
  const std::string& key() const { return m_key; }
  const SkPath& path() const { return m_path; }
  /** How far off the boundary this strand can run — 0 for the absolute
   *  family, whose geometry is its own. */
  float reach() const {
    return m_source == Source::Relative ? m_profile.max() : 0.0f;
  }
  bool operator==(const StrandPath& o) const {
    return m_source == o.m_source && m_profile == o.m_profile &&
           m_key == o.m_key && m_path == o.m_path;
  }

 private:
  Source m_source = Source::Relative;
  Profile m_profile;
  std::string m_key;
  SkPath m_path;
};

namespace strand {
/** Borrow a keyed element's resolved path (derive phase, cycle-guarded
 *  like every other borrow). */
inline StrandPath from(std::string_view key) {
  return StrandPath::borrowed(std::string(key));
}
/** Authored geometry, in the host element's local space. */
inline StrandPath path(SkPath p) { return StrandPath::authored(std::move(p)); }
}  // namespace strand

/** Displace a path in its own (along, across) frame — the primitive
 *  behind a relative strand, and exactly the band's frame: `along` is a
 *  fraction of total arc length, positive `across` is LEFT of travel
 *  (outside a clockwise path). A constant profile delegates to
 *  `lines::offsetAcross`, which means the same side. */
SkPath profileOffset(const SkPath& spine, const Profile& profile);

/** THE REGION a band occupies: the spine walked at both profile rails,
 *  per contour, through `profileOffset` — so corners get
 *  `lines::offsetAcross`'s real-vertex repair (arc outside a turn, miter
 *  inside) instead of the sample-and-displace spur a naive walk leaves on
 *  the inside of every rectangle.
 *
 *  Public because a varying-width MARK along a spine IS this region: a
 *  milled groove, or a ribbon, is this band filled. Sharing one geometry
 *  keeps the corner repair from being reimplemented per consumer. */
SkPath bandRegion(const SkPath& spine, const Across& width,
                  Formation formation = Formation::Centered);

// ---------------------------------------------------------------------------
// Crossings — WHICH mark is on top where two strands meet

/** Who is on top. Read against a Crossing's `a`, which is always the
 *  LOWER strand index, so the question is well-posed: Over means strand
 *  `a` passes over strand `b`. */
enum class Order : uint8_t { Over, Under };

/** One discovered crossing. **Crossings are never authored** — they are
 *  found by path intersection and numbered along the boundary. */
struct Crossing {
  /** ORDINAL in the discovered list, 0-based: the crossings are sorted by
   *  `alongA` (position on the lower-indexed strand) and then numbered.
   *  It is NOT a coordinate in any parameterisation and NOT stable under
   *  a change of geometry — add a strand or move one and the same knot may
   *  take a different ordinal. This is the number `CrossingRule::except()`
   *  pins, which is exactly why pins are documented as positional. */
  size_t index = 0;
  /** Strand indices, always `a < b` — `b` is the one list order paints
   *  later, i.e. on top when nothing says otherwise. */
  size_t a = 0, b = 0;
  SkPoint at{0, 0};
  /** Where the crossing falls along each strand, as fractions of that
   *  strand's arc length. */
  float alongA = 0.0f, alongB = 0.0f;
  bool operator==(const Crossing&) const = default;
};

/** A crossing rule value: `Order decide(const Crossing &) const`, plus
 *  equality — one named required member on a comparable value, like every
 *  other seam here. Never a bare lambda: a rule is read live every frame,
 *  so it has to participate in reconciler equality or the node holding it
 *  can never prune. */
template <typename D>
concept CrossingScheme =
    std::equality_comparable<D> && requires(const D& d, const Crossing& c) {
      { d.decide(c) } -> std::convertible_to<Order>;
    };

/** The rule ladder, as ONE comparable value. Climb only as far as the
 *  composition needs:
 *
 *      crossing::alternate()                    // == sequence({Over, Under})
 *      crossing::sequence({Over, Over, Under})  // any repeating pattern
 *      crossing::pairs({{0,1},{1,2},{2,0}})     // dominance, cyclic allowed
 *      MyRule{}                                 // your own decide() value
 *
 *  and pin exceptions onto whatever you chose with `.except(i, order)`.
 *
 *  The default is LIST ORDER: later strands pass over earlier ones. That
 *  is what makes `layers` and `weave` formally one machine. */
class CrossingRule {
 public:
  CrossingRule() = default;
  template <CrossingScheme D>
  CrossingRule(D scheme)  // NOLINT: implicit by design (.crossing = MyRule{})
      : m_kind(Kind::Custom) {
    m_held = scheme;
    m_equals = [](const std::any& x, const std::any& y) {
      return std::any_cast<const D&>(x) == std::any_cast<const D&>(y);
    };
    m_decide = [s = std::move(scheme)](const Crossing& c) {
      return s.decide(c);
    };
  }

  static CrossingRule sequence(std::vector<Order> pattern) {
    CrossingRule r;
    r.m_kind = Kind::Sequence;
    r.m_pattern = std::move(pattern);
    return r;
  }
  static CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
    CrossingRule r;
    r.m_kind = Kind::Pairs;
    r.m_dominance = std::move(dominance);
    return r;
  }

  /** Pin ONE crossing, layered over whatever rule this already is.
   *
   *  **Pins are POSITIONAL**: the index is a position in the discovered
   *  order, so a stable RULE survives a geometry change and a pin does
   *  not — move a strand and pin 3 lands on a different meeting. Use
   *  rules while a composition is still moving, and pins only once it is
   *  settled and you are correcting one knot by eye.
   *
   *  Pins compose onto the base rule and never stack as separate
   *  entries: there is one `.crossing` field, and this is how it takes
   *  exceptions. */
  CrossingRule& except(size_t index, Order order) {
    for (auto& pin : m_pins)
      if (pin.first == index) {
        pin.second = order;
        return *this;
      }
    m_pins.emplace_back(index, order);
    return *this;
  }

  Order decide(const Crossing& c) const {
    for (const auto& pin : m_pins)
      if (pin.first == c.index) return pin.second;
    switch (m_kind) {
      case Kind::Sequence:
        if (!m_pattern.empty()) return m_pattern[c.index % m_pattern.size()];
        break;
      case Kind::Pairs:
        for (const auto& [over, under] : m_dominance) {
          if (over == (int)c.a && under == (int)c.b) return Order::Over;
          if (over == (int)c.b && under == (int)c.a) return Order::Under;
        }
        break;
      case Kind::Custom:
        if (m_decide) return m_decide(c);
        break;
      case Kind::ListOrder:
        break;
    }
    // List order: `b` is later in the list, so `a` is underneath.
    return Order::Under;
  }

  bool operator==(const CrossingRule& o) const {
    if (m_kind != o.m_kind || m_pattern != o.m_pattern ||
        m_dominance != o.m_dominance || m_pins != o.m_pins)
      return false;
    if (m_kind != Kind::Custom) return true;
    if (!m_equals || !o.m_equals) return !m_equals && !o.m_equals;
    return m_held.type() == o.m_held.type() && m_equals(m_held, o.m_held);
  }

 private:
  enum class Kind : uint8_t { ListOrder, Sequence, Pairs, Custom };
  Kind m_kind = Kind::ListOrder;
  std::vector<Order> m_pattern;
  std::vector<std::pair<int, int>> m_dominance;
  std::vector<std::pair<size_t, Order>> m_pins;
  std::function<Order(const Crossing&)> m_decide;
  std::any m_held;
  std::function<bool(const std::any&, const std::any&)> m_equals;
};

namespace crossing {
/** Over, under, over, under — the plain-weave rule, and formally just
 *  `sequence({Over, Under})`. Both spellings exist because they name two
 *  author intents over one machine. */
inline CrossingRule alternate() {
  return CrossingRule::sequence({Order::Over, Order::Under});
}
inline CrossingRule sequence(std::vector<Order> pattern) {
  return CrossingRule::sequence(std::move(pattern));
}
/** Strand DOMINANCE: `{{over, under}, …}`. Cycles are legal and are the
 *  point — `{{0,1},{1,2},{2,0}}` is the impossible-braid rule Penrose
 *  tilings and heraldic knots are full of. */
inline CrossingRule pairs(std::vector<std::pair<int, int>> dominance) {
  return CrossingRule::pairs(std::move(dominance));
}
}  // namespace crossing

/** Every crossing among a set of strand paths, numbered along the
 *  boundary (ascending by position on the lowest-indexed strand
 *  involved). Only PROPER crossings count: coincident strands and
 *  endpoint touches, such as a shared polygon vertex, are meetings rather
 *  than crossings, and reporting them would put a knot at every corner. */
std::vector<Crossing> discoverCrossings(const std::vector<SkPath>& strands);

/** The region where two strands' MARKS actually overlap at one crossing:
 *  the intersection of the two paths stroked to their own reach, reduced to
 *  the component containing `at` and bounded by @p maxRadius px around it.
 *
 *  Exact at any angle, which a disc is not — two marks meeting at 12° overlap
 *  in a long lens whose extent along each strand goes as reach/sin(theta),
 *  and a disc sized for the perpendicular case leaves the under-strand
 *  showing straight across the over-strand's mark.
 *
 *  `maxRadius` is not a safety margin, it is REQUIRED for correctness on any
 *  ordinary braid. Once reach/sin(theta) approaches the spacing between
 *  knots, neighbouring lenses touch and path ops merge them into ONE
 *  contour — at which point the first crossing's patch owns the whole run
 *  and the weave degenerates to "one strand on top" for half its knots.
 *  Pass half the arc distance to the adjacent crossing, so each knot can
 *  only ever claim its own half.
 *
 *  Falls back to a disc when the intersection is empty (degenerate or
 *  non-overlapping input). */
SkPath crossingPatch(const SkPath& a, float reachA, const SkPath& b,
                     float reachB, SkPoint at, float maxRadius);

// ---------------------------------------------------------------------------
// Layout values (Yoga semantics, 1:1)

struct Dim {
  enum class Unit : uint8_t { Px, Pct, Auto };
  Unit unit = Unit::Auto;
  float value = 0.0f;

  constexpr Dim() = default;
  constexpr Dim(float px)  // NOLINT: implicit by design
      : unit(Unit::Px), value(px) {}
  bool operator==(const Dim&) const = default;
};
constexpr Dim pct(float v) {
  Dim d;
  d.unit = Dim::Unit::Pct;
  d.value = v;
  return d;
}
constexpr Dim autoDim() { return {}; }

/** `width(50_pct)`, `basis(120_px)` — for the Dim-valued setters;
 *  exposed by `using namespace sigil::compose` (or `using namespace
 *  sigil::compose::literals`). */
inline namespace literals {
constexpr Dim operator""_px(long double v) { return Dim((float)v); }
constexpr Dim operator""_px(unsigned long long v) { return Dim((float)v); }
constexpr Dim operator""_pct(long double v) { return pct((float)v); }
constexpr Dim operator""_pct(unsigned long long v) { return pct((float)v); }
}  // namespace literals

enum class Align : uint8_t { Auto, Start, Center, End, Stretch, Baseline };
enum class Justify : uint8_t {
  Start,
  Center,
  End,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly
};

/** One misprint pass: the node's own fill shape and text re-stamped at
 *  `offset` in a flat color, UNDER the real content. Repeated echoes stack
 *  in declaration order, bottom first. This is the registration-error
 *  look — offset ink under-copies, hard-edged sticker stacks — as one call
 *  rather than duplicate sibling nodes. */
struct Echo {
  SkVector offset = {3, 3};
  SkColor4f color = {0, 0, 0, 1};
  bool operator==(const Echo&) const = default;
};

/** Cache override.
 *
 *  - **Auto** (the default) records provably-static subtrees as pictures.
 *  - **Picture** records, and never lets the library promote the node to
 *    a pixel bake.
 *  - **Texture** rasterizes the subtree once into an image. Best for dense
 *    or effect-heavy content; wasteful for sparse regions, where the blit
 *    of a mostly-empty image costs more than the few draws it replaced.
 *  - **Group** is Texture for a subtree whose children ANIMATE — see
 *    below.
 *  - **None** opts a node out entirely. A per-frame paint program that
 *    reads the clock MUST declare this: nothing can see that a
 *    `PaintProgram` sampled `elapsedSeconds`, so an undeclared one is
 *    recorded on its first frame and replayed frozen thereafter.
 *
 *  **Group** is for "many small rotated or blended pieces forming one
 *  assembly that is currently still". `Cache::Texture` bakes a node's OWN
 *  paint and refuses the moment anything below it is volatile, so a
 *  fill-less container of animated strips gets no bake at all and every
 *  strip replays its shaders every frame. `Cache::Group` bakes the
 *  container AND its children into one unrotated device-space layer, so
 *  the children's rotations, bevels and mutual compositing resolve INSIDE
 *  the bake at full precision. That is why it is pixel-safe where putting
 *  `Cache::Texture` on each child is not: per-child bakes isolate the
 *  pieces and change how they composite with each other.
 *
 *  It is held by a SUBTREE VALUE MEMO rather than by a volatility verdict.
 *  The bake is taken only while every bound transform, opacity and content
 *  scalar below the node still holds the value it held last frame, and is
 *  dropped on the frame any of them ticks. So an entrance animation plays
 *  live and the settled assembly costs one blit.
 *
 *  IT REFUSES, permanently and with one line to stderr, any subtree
 *  carrying volatility a float comparison cannot see: a live material
 *  (`uTime` or a bound uniform), an animated decoration, an animated
 *  image, a bound `fill()`, a variable-font drive, a `Cache::None`
 *  descendant, or a non-srcOver blend or backdrop filter below the root
 *  (which would resolve against the bake's transparent black). It also
 *  declines per frame while its own transform animates or its device rect
 *  is moving, because a device-pinned bake remade every frame costs more
 *  than the paint it replaces. A Group node that never reports itself as
 *  held has one of the above in it. */
enum class Cache : uint8_t { Auto, Picture, Texture, Group, None };

// ---------------------------------------------------------------------------
// Custom layout (the SwiftUI Layout-protocol shape, C++20-ified)

/** What a custom layout sees: the container's resolved size, each child's
 *  measured size (text children measured by SigilWeave), and each child's
 *  first-baseline offset from its own top (NaN for children without one) —
 *  what baseline-rhythm schemes (layouts::BaselineGrid) snap by. */
struct LayoutInput {
  SkSize container = SkSize::MakeEmpty();
  std::vector<SkSize> childSizes;
  std::vector<float> childBaselines;  // NaN = no baseline (non-text)
};

/** A custom layout places children: one rect per child (position and
 *  size, container-relative). Runs as a bounded second layout pass. */
template <typename L>
concept LayoutScheme = requires(const L& l, const LayoutInput& in) {
  { l.place(in) } -> std::convertible_to<std::vector<SkRect>>;
};

// ---------------------------------------------------------------------------
// Concepts (readable errors at the generic entry points)

template <typename P>
concept ComponentProps = std::equality_comparable<P> && std::copyable<P>;

class Element;

template <typename F, typename P>
concept ComponentFn =
    std::invocable<F, const P&> &&
    std::convertible_to<std::invoke_result_t<F, const P&>, Element>;

// ---------------------------------------------------------------------------
// Element — a cheap value description

class Element {
 public:
  Element();  // empty box

  // ---- layout ----
  Element& row();
  Element& column();
  /** Flex-wrap: children flow onto new lines/columns when they
   *  overflow the main axis. */
  Element& wrapLines(bool on = true);
  Element& gap(float px);
  Element& padding(float all);
  Element& padding(float horizontal, float vertical);
  Element& padding(float left, float top, float right, float bottom);
  Element& margin(float all);
  Element& margin(float horizontal, float vertical);
  Element& margin(float left, float top, float right, float bottom);
  /** The flex BASIS, not a guarantee. `shrink` defaults to 1, faithful to
   *  Yoga and CSS, so a `width(150)` child of a row that overflows is
   *  150 px wide only until the row runs out of room — then it gives some
   *  back, and the result is silently narrower content rather than an
   *  error. Pair with `.shrink(0)` when `width(150)` means "this IS 150".
   *  The same holds for `height()` in a column. */
  Element& width(Dim d);
  Element& height(Dim d);
  Element& minWidth(Dim d);
  Element& maxWidth(Dim d);
  Element& minHeight(Dim d);
  Element& maxHeight(Dim d);
  Element& aspect(float ratio);
  Element& grow(float factor = 1.0f);
  Element& shrink(float factor);
  Element& basis(Dim d);
  Element& alignItems(Align a);
  Element& alignSelf(Align a);
  Element& justify(Justify j);
  Element& absolute();
  Element& inset(float all);
  Element& inset(float left, float top, float right, float bottom);
  /** Dim-valued insets: px, pct(), or autoDim() per side — autoDim()
   *  leaves that side unpinned (the CSS `auto`), so width/height (or the
   *  opposite inset) size the node instead of stretching it. */
  Element& inset(Dim left, Dim top, Dim right, Dim bottom);
  /** Pin ONE edge of an absolute node (implies absolute()): the
   *  corner-badge idiom — `.top(12).right(12)` pins a date block to the
   *  top-right without stretching it across the box. Unpinned sides stay
   *  auto. */
  Element& left(Dim d);
  Element& top(Dim d);
  Element& right(Dim d);
  Element& bottom(Dim d);
  /** Center this absolute node ON a parent-space point — the dominant
   *  placement in node-graph scenes (sockets on orbit positions, badges
   *  on markers). Resolved after measurement, so intrinsic-size nodes
   *  center correctly; implies absolute(). */
  Element& centerAt(SkPoint p);
  /** Place an absolute node on a parent-space RECT — the peer of
   *  centerAt(), for when you already know the box.
   *
   *  Exactly `left(r.fLeft).top(r.fTop).width(r.width()).height(r.height())`
   *  — it calls those four setters, so it writes the same four layout
   *  fields, prunes identically, and cannot drift from the longhand. Right
   *  and bottom stay unpinned.
   *
   *  **A primitive for placing content whose coordinates you already
   *  have**, typically because they were measured off a reference. When a
   *  position is a *relationship* instead — "inside its parent", "next to
   *  that one", "as wide as the column" — flex and inset() express it and
   *  this does not.
   *
   *      g.child(box().rect(panelBox).fill(…));
   *      g.child(text(u8"…", st).at({panelBox.fLeft + 16, panelBox.fTop}));
   *
   *  Does not cover right()/bottom() pinning, percentage insets, or
   *  autoDim() sides — those are different intents and keep the longhand.
   *  `util::centred()` (Util.h) builds the rect for the centre-and-size
   *  case. */
  Element& rect(const SkRect& r);
  /** Pin an absolute node's top-left to a parent-space POINT, leaving the
   *  node to size itself from its content — `left(p.fX).top(p.fY)`. The
   *  half of the placement longhand that carries no box; same
   *  qualification as rect() above. */
  Element& at(SkPoint topLeft);

  // ---- shape (defines PaintContext::outline and clipping) ----
  Element& corners(Corners c);
  /** THE NODE'S SHAPE: a path generator over its laid-out size, in local
   *  coordinates. Overrides corners() — the fill surface, clip(), every
   *  stroke pass and every outline-following decoration (PathFormat,
   *  ContourWalk) trace it. Spiky dialogs, scalloped frames, any
   *  non-rectangular chrome.
   *
   *  A shape is a REGION; a stroke is a mark on its boundary. Filling this
   *  is `fill()`, drawing its edge is `stroke()`.
   *
   *  Takes a `Shape`. Every `shapes::` generator is a comparable value, so
   *  a shaped node prunes exactly like an unshaped one. A raw callable is
   *  accepted as the escape hatch, but it never compares equal, so the
   *  node re-patches and re-records on every describe — memo() such a
   *  node, or hold the Shape value stable, to get pruning back. */
  Element& shape(Shape path);
  /** BAND FORMATION: which side of the spine the band occupies.
   *  `.centered()` is the default and straddles it; `.outward()` and
   *  `.inward()` take one side (the offset-path lineage). No effect on a
   *  node that is not a band(). */
  Element& centered();
  Element& outward();
  Element& inward();
  /** Clip fill, content, and children to the node's shape. Decorations
   *  are NOT clipped — they dress the outline (outer strokes, shadows,
   *  glows keep their reach); hit-testing still bounds the subtree.
   *
   *  SUGAR, and exactly equivalent, so the two spellings are one machine:
   *
   *      .clip()  ==  .mask(parts::surface() | parts::content() |
   *                         parts::children(), by::shape(Region::own()))
   *
   *  Kept as its own word because it is also the cheap path: a rounded box
   *  clips with `clipRRect`, where the general shape gate has to build a
   *  path and clip against that. */
  Element& clip(bool on = true);

  // ---- mask (the appearance-gating family) ----
  /** THE FAMILY VERB, taught form: gate everything this node paints.
   *
   *      .mask(by::spans(spans::upTo(animate(from(0.f).to(1.f), {600ms}))))
   *      .mask(by::edge(90.f, bind(&sweep)))
   *      .mask(by::shape(Region::path(seal)))
   *      .mask(by::alpha(Material::linear({0,0}, {0,h}, fadeStops)))
   *
   *  Sugar for `mask(parts::all(), with)`, and the form to reach for
   *  first. A gate addresses only the paint it CAN address: an arc-length
   *  window means something to the surface and the marks and nothing to
   *  the children, so `parts::all()` with `by::spans()` gates the boundary
   *  tracers and leaves the children alone.
   *
   *  Paint-only and bindable, like the transforms: animating a mask never
   *  relayouts, and hit-testing keeps the UNMASKED shape — a mask is a
   *  paint-phase reveal, not a layout change. */
  Element& mask(Gate with);
  /** …and the granular form: gate SOME of what this node paints.
   *
   *      panel.overlay(hazardStripes, "hazard")
   *           .foreground(bevelKeyline)
   *           .mask(parts::named("hazard"), by::edge(0.f, &armTime));
   *
   *  Repeated calls APPEND, as every decoration slot does, and masks whose
   *  selections OVERLAP INTERSECT on the overlap — both gates must pass.
   *  Each mask carries its own animation slots, so masks at three
   *  different rates on one node is a picture, not a race: the
   *  intersection is recomputed exactly, per frame.
   *
   *  Union is spelled INSIDE a gate value (combining spans with `|`), never
   *  across masks — two masks are two conditions, and stacking them can
   *  only ever show less.
   *
   *  The one thing this cannot express that `stroke(where, what)` can: a
   *  span pass CLAIMS its run and joins the overlap check, and a mask does
   *  not. That check is deliberately read against the UNMASKED boundary,
   *  so an overlapping claim is a description-level mistake reported once,
   *  never one that blinks in and out partway through a transition. */
  Element& mask(Parts what, Gate with);

  // ---- paint ----
  /** A colour, a shader, a transition between colours, or a LIVE binding.
   *
   *  The binding form is `fill(&output)` where the Output holds a `Fill`,
   *  and it is the answer to "this widget's colour IS its value" — a
   *  level meter whose hue is the level, a temperature readout, a health
   *  bar that reddens. Write the Fill Output from the same steppable that
   *  computes the number:
   *
   *      ch::Output<float> level; ch::Output<Fill> bar;
   *      ticker.add([&](double){ level = v; bar = Fill::color(ramp(v)); … });
   *      box().scaleX(bind(&level)).fill(&bar)
   *
   *  What does NOT exist is deriving one from the other at the binding
   *  site: `fill(bind(&level).map(ramp))` does not compile, because the
   *  shaping chain maps floats to floats. Compute the Fill in the
   *  steppable, as above. */
  Element& fill(Animatable<Fill> f);
  /** Fill with a Material (gradient ramp, blend stack, sprite, SkSL) — the
   *  richer authoring value. A static Material collapses to a Fill, so it
   *  caches and prunes on the same path. See <sigilcompose/Material.h>. */
  Element& fill(Material m);
  /** Solid-color sugar: fill({r,g,b,a}) without the Fill:: ceremony. */
  Element& fill(SkColor4f color) {
    return fill(Animatable<Fill>{Fill::color(color)});
  }
  /** How an image() leaf samples its source. Defaults to linear, which is
   *  right for photographs and wrong for every pixel grid: art, tilemaps,
   *  fonts baked as sprites, simulation buffers.
   *
   *      image(tileset).sampling(SkSamplingOptions(SkFilterMode::kNearest))
   *
   *  `Material::image()` takes the same options for a sprite fill. No
   *  effect on non-image leaves, silently. */
  Element& sampling(SkSamplingOptions options);
  // ---- decoration layers ----
  // Backgrounds paint below content/children (in declaration order),
  // foregrounds above; fill() is the transitionable first background,
  // custom() a box with one background program.
  // Repeated calls APPEND (the Photoshop stacked-strokes model — two
  // stroke() calls are two rings).
  // Decorations dress the OUTLINE: clip() does not clip them (it bounds
  // fill/content/children only), so outer strokes and shadows survive on
  // clipped nodes.
  /** Takes this node OUT of hit testing — CSS `pointer-events: none`.
   *
   *  READ THIS BEFORE KEYING A CONTAINER. `hitTest` returns any keyed node
   *  whose box contains the point, whether or not that node paints
   *  anything. So a keyed, full-bleed layout SHELL with no fill swallows
   *  every hit in the frame, and every query comes back naming it. There
   *  is no visual symptom and no diagnostic — the shell is invisible and
   *  the answers are simply wrong. This is the opt-out.
   *
   *  Children are still tested: this excludes the node's own box, not its
   *  subtree. */
  Element& hitTestable(bool enabled);
  /** A decoration painted OVER the fill and UNDER the content and
   *  children.
   *
   *  THE STACKING ORDER IS A CONTRACT, not a hint, and picking the wrong
   *  slot is the commonest way to draw nothing visible. `background()`
   *  sits beneath the FILL, so an opaque fill covers it completely — a
   *  bevel put there renders as a flat slab. `foreground()` paints above
   *  the children, so a texture put there greys out the node's own label.
   *  This middle slot is what hazard stripes over a surface but under the
   *  digit, scanlines over a panel but under its readout, and bevelled
   *  chrome all want. The alternative is a sibling stack, which costs a
   *  node and loses the shared outline.
   *
   *  `name` is optional and LOCAL: it labels this mark so
   *  `mask(parts::named(name), by::…)` can address it and nothing else.
   *  Same names, same law as `stroke(Spans, what, name)` — inspection and
   *  intra-element reference, never a query key. */
  Element& overlay(Decoration d, std::string name = {});
  /** A decoration painted BENEATH the fill (the CSS box-shadow
   *  ordering) — shadows, ground textures, anything the surface sits on
   *  top of. If you want it over the surface but under the children, that
   *  is `overlay()` above. `name` labels the mark for `parts::named()`. */
  Element& background(Decoration d, std::string name = {});
  /** THE BACKGROUND SLOT, span-qualified — `.stroke(where, what)`'s twin
   *  in the other z-half.
   *
   *      .background(spans::edges(14), stroke(3, shadowInk))  // under the fill
   *      .stroke(spans::corners(18), stroke(2, ink))          // over the kids
   *
   *  Identical in every respect to `stroke(Spans, ...)` except WHERE the
   *  mark lands: it paints with the backgrounds, beneath the fill and
   *  therefore beneath the content and the children. Everything else is
   *  shared, deliberately — the passes append into ONE list in declaration
   *  order, one claim record covers both z-halves, the no-overlap rule
   *  reads across both, and `rest()` complements both. A boundary does not
   *  have two of itself, so a background pass and a stroke pass claiming
   *  the same run is the same conflict as two stroke passes doing it, and
   *  `rest("name")` can name a pass in either half. */
  Element& background(Spans where, Decoration what, std::string name = {});
  /** A decoration painted OVER the children. `name` labels the mark for
   *  `parts::named()`. */
  Element& foreground(Decoration d, std::string name = {});
  /** fill's peer: dress the node's whole BOUNDARY with a brush — a
   *  PathFormat, a layered brush stack, any decoration that strokes.
   *
   *  This form does not CLAIM: it overlays the whole boundary, so repeated
   *  calls stack (two strokes are two rings) and never collide. Naming a
   *  `where` (below) is what turns a pass into a claim on part of the
   *  boundary; naming a `name` (here) is what lets a mask address this
   *  mark alone. */
  Element& stroke(Decoration brush, std::string name = {});
  /** THE STROKE SLOT: `where` on the boundary, painted by `what`.
   *
   *      .stroke(spans::corners(18), stroke(2, ink))          // reticle
   *      .stroke(spans::edges(14), stroke(1, ink))            // open corners
   *      .stroke(spans::upTo(animate(from(0.f).to(1.f), {600ms})), wire)
   *
   *  Repeated calls APPEND, in declaration order.
   *
   *  ORDERING, precisely, because CALL ORDER DOES NOT DECIDE IT: the
   *  unqualified strokes paint FIRST — they are foregrounds and share that
   *  list — then the span passes in their own declaration order. Within
   *  each group declaration order holds; between the groups the
   *  unqualified ones are always underneath. Interleaving the two by call
   *  order is not expressible, and writing them interleaved does not make
   *  it so. If a span pass must sit UNDER a whole-boundary one, make the
   *  whole-boundary one a span pass too (`spans::every(1)`) so both are in
   *  the same list.
   *
   *  Span-qualified passes CLAIM the runs they resolve to, and two claims
   *  that overlap are reported out loud, naming both passes and the
   *  overlapping run: one boundary, one mark. Layering two marks on one
   *  run is a composite BRUSH rather than two passes —
   *  `Brush{}.layer(a).layer(b)`, or a LayeredBrush.
   *
   *  Two exceptions, both deliberate: bare `spans::rest()` claims whatever
   *  the other passes left over, so a rule and its bracket corners are two
   *  calls and no arithmetic; and `spans::rest("name")` is the complement
   *  of ONE named pass and may overlay the others.
   *
   *  `name` is LOCAL to this element — for inspection, for the
   *  `rest("name")` reference, and for `mask(parts::named(name), …)`. It
   *  is not a query key; `Composer::bounds` and `hitTest` see only
   *  `key()`.
   *
   *  EXACTLY EQUIVALENT to the mask spelling, so the two are one machine:
   *
   *      .stroke(where, what, name)
   *          ==  .stroke(what, name).mask(parts::named(name),
   *                                       by::spans(where))
   *
   *  Identical pixels, and the same value under the same intersection
   *  rule — a further `mask(parts::marks(), by::spans(upTo(t)))` cuts this
   *  pass to `where ∩ upTo(t)`, which is how reticle brackets light up as
   *  a sweep reaches them. The ONE thing the pass form does that the mask
   *  spelling does not: it CLAIMS its run and joins the overlap check. */
  Element& stroke(Spans where, Decoration what, std::string name = {});
  /** Apply a whole LayerStyle (preset or hand-built): its `under` layers
   *  append as backgrounds, `over` as foregrounds — one call dresses the
   *  node in aqua gel / y2k chrome / any bundled treatment. Composable
   *  with fill() and further background()/foreground() calls. */
  Element& style(LayerStyle s);
  /** Append a misprint echo (see Echo): the node's fill shape and text
   *  re-stamped offset+flat-colored beneath the real pass. Not applied to
   *  text carrying `fx()` tracks (a moving letter draws its own batched
   *  buckets) or to image/custom content. */
  Element& echo(SkVector offset, SkColor4f color);
  /** Post-processes this node's rendered layer (forces a stacking
   *  context). Baked once under Cache::Texture. */
  Element& effect(Effect e);
  /** Filters what is already painted beneath this node's bounds before
   *  the node paints (CSS backdrop-filter). Incompatible with
   *  Cache::Texture (the backdrop depends on the live destination);
   *  such nodes fall back to picture caching. */
  Element& backdrop(Effect e);
  Element& opacity(Animatable<float> o);
  Element& blend(SkBlendMode mode);
  Element& translateX(Animatable<float> v);
  Element& translateY(Animatable<float> v);
  /** Ride a CURVE instead of two lanes — the motion path (see MotionPath
   *  for the six rules). Paint-only like the lanes it outranks; the
   *  node's transform origin is the point that lands on the curve, and
   *  the curve is resolved against the PARENT's box.
   *
   *      .travel({.path = shapes::circle(),
   *               .t = bind(&phase).target(0, 1),
   *               .lookAhead = 0.02f})   // auto-orient along the tangent
   */
  Element& travel(MotionPath along);
  Element& rotate(Animatable<float> degrees);
  Element& scale(Animatable<float> factor);
  /** Per-axis scale about the transform origin, multiplied INTO scale().
   *  Paint-only like scale(): animating one never relayouts, and the
   *  content picture replays under the new transform.
   *
   *  Bars, wipes, meters, cooldown sweeps, drain rings and "slide this
   *  piece into its slot" are the most common animated primitive a UI
   *  has, and not one of them is uniform. Without these the idiom was a
   *  full-width fill inside a clip translated by -(1 - fraction) * width,
   *  which only survives while the fill happens to be a gradient along
   *  the OTHER axis. Set transformOrigin() to pin the growing edge —
   *  `transformOrigin(0, 0.5f).scaleX(&fraction)` grows a bar rightward
   *  from its left edge. */
  Element& scaleX(Animatable<float> factor);
  Element& scaleY(Animatable<float> factor);
  /** Shear, in degrees, about the transform origin. Paint-only like
   *  rotate/scale: animating a skew never relayouts, and content pictures
   *  replay under the new transform.
   *
   *  skewX slants verticals, skewY slants horizontals. The sense is
   *  screen-space, y down: a POSITIVE skewX shifts points further down the
   *  node further right, so the shape's top leans LEFT — the italic
   *  forward lean is a NEGATIVE skewX. */
  Element& skewX(Animatable<float> degrees);
  Element& skewY(Animatable<float> degrees);
  // Integer-literal sugar (rotate(-8) etc. — int doesn't convert into the
  // Animatable variant on its own, and the resulting error is unreadable).
  // std::integral-constrained so FLOAT calls can never land here (a plain
  // int overload would capture them via the standard float→int conversion
  // and recurse); Animatable is constructed explicitly for the same reason.
  template <std::integral T>
  Element& opacity(T v) {
    return opacity(Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateX(T v) {
    return translateX(Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& translateY(T v) {
    return translateY(Animatable<float>((float)v));
  }
  template <std::integral T>
  Element& rotate(T deg) {
    return rotate(Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& scale(T f) {
    return scale(Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleX(T f) {
    return scaleX(Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& scaleY(T f) {
    return scaleY(Animatable<float>((float)f));
  }
  template <std::integral T>
  Element& skewX(T deg) {
    return skewX(Animatable<float>((float)deg));
  }
  template <std::integral T>
  Element& skewY(T deg) {
    return skewY(Animatable<float>((float)deg));
  }
  Element& transformOrigin(float fx, float fy);
  /** Pixel-valued transform origin (node-local px) — for pivots that
   *  aren't a fraction of THIS node's box, e.g. zooming a window that
   *  lives inside a full-canvas overlay around its own center. */
  Element& transformOriginPx(SkPoint p);
  Element& zIndex(int z);

  // ---- derive phase (inputs are resolved geometry) ----
  /** Text leaves only: flow this paragraph around the keyed node, with
   *  @p margin px of standoff.
   *
   *  A target that declares a SILHOUETTE — a `shape()`, or a routed
   *  connector or rail — is subtracted by that outline, concavities and
   *  holes included, so text runs into the notch of a star and through the
   *  ring of an annulus. A target that declares none is subtracted by its
   *  BOX, which is the whole of what it occupies. The margin is the same
   *  standoff from whichever edge is being subtracted; corner radii round
   *  the fill rather than the outline and do not count as a silhouette.
   *
   *  Resolved as a bounded second layout pass, so a target that moves
   *  re-cuts the lines under it; a reference to self or a descendant is
   *  ignored (cycle guard). Call repeatedly to weave around several
   *  elements. */
  Element& flowAround(std::string_view key, float margin = 0.0f);

  // ---- content ----
  /** Image leaves only: draw this sub-rect of the asset (atlas / sprite
   *  regions, in source pixels) instead of the whole image. Strictly
   *  constrained — neighboring atlas cells never bleed in. */
  Element& region(SkRect sourceRect);

  /** APPENDS a text-fx track to a text() element (see Track): which
   *  glyphs, what deviation from rest, how the beats spread, and the
   *  master progress that drives it.
   *
   *  Call it once per track. Several tracks compose per glyph — dx/dy and
   *  rotation ADD, scale and alpha MULTIPLY — in the order they were
   *  declared, and each keeps its own transition slot, so retargeting one
   *  track's progress leaves the others running. */
  Element& fx(Track track);
  /** VariationDrive (text leaves): drive a variable-font axis from a
   *  bound Output at DRAW time — paint-only volatility, no reshape, no
   *  relayout. The paint phase probes the node's fonts once per axis:
   *  an advance-variant axis (wght on most fonts) is REFUSED with a debug
   *  warning and the text draws at its shaped coordinates — drive GRAD
   *  (the advance-invariant weight) or re-render discretely instead.
   *
   *  SUGAR over `fx()`: it appends a whole-text track whose deviation is
   *  `GlyphMod::axis`, so a driven axis composes with entrances, loops and
   *  every other track instead of being a second text path they would hide.
   *  Being a track, it also draws through the batched glyph path, which
   *  paints glyphs and not a span's underline or strikethrough. */
  Element& variationDrive(const char (&tag)[5],
                          const choreograph::Output<float>* value);

  /** A SIBLING ANCHORED TO A UNIT OF THE TEXT: a caret, a callout, a tick,
   *  a rule standing at a word's edge. @p what becomes a child of this text
   *  node whose PARENT BOX is the rect @p where resolves to, so it is
   *  written in exactly the placement longhand a `positioned()` child takes
   *  — px or pct `left`/`top`/`right`/`bottom`/`width`/`height`, measured
   *  inside that rect, and free to sit outside it:
   *
   *      text(line, style)
   *          .mark(sel::word(3), box().left(0).top(pct(100))
   *                                   .width(pct(100)).height(2)
   *                                   .fill(Fill::color(ink)))
   *
   *  With no dims at all the mark simply IS the rect. A mark carrying no
   *  key is given one from its declaration order, so it prunes; a mark that
   *  carries one keeps it, and that key is what `Composer::bounds` and
   *  `hitTest` answer for.
   *
   *  A MARK IS NOT A `rich().slot()`. A slot reserves space INSIDE the flow
   *  — the line breaks around it, it moves the line's height, and the type
   *  after it starts further along. A mark reserves nothing: the text is
   *  laid out as though the mark were not there and the mark is placed on
   *  the result, so it may overlap the letters, straddle several, or hang
   *  outside the node's box entirely. Reserve a box for content that is
   *  part of the sentence; mark the type that is already there.
   *
   *  A SELECTOR RESOLVING SEVERAL UNITS GIVES ONE RECT, the union of every
   *  glyph it addressed — `sel::each(unit::Word)` therefore anchors a mark
   *  to the whole paragraph, which is a rect and rarely the intent. One
   *  mark is one element with one identity and one box; to mark each of
   *  several units, write one mark per unit. A selector resolving NOTHING —
   *  including a name no run carries and a pattern that does not compile —
   *  places nothing and warns once, on the silent-no-op family's terms.
   *
   *  THE RECT IS THE REST RECT: where the LAYOUT put those glyphs, not
   *  where an `fx()` track has thrown them this frame. The mark therefore
   *  follows a reflow, a restyle and a resize exactly as the letters do,
   *  and stands still while a cascade deviates them. That is deliberate on
   *  both counts — a deviation is per glyph and per track and several
   *  compose, so there is no one place a moving unit "is", and a mark
   *  re-placed at paint would make the layout depend on the frame. For a
   *  mark that must RIDE the motion, read `Composer::beatsOf` and drive the
   *  mark's own transform from it.
   *
   *  IT NEEDS NO REACH. A track declares one because the glyphs it throws
   *  are painted by the text node itself; a mark is a CHILD, and the
   *  recording cull already grows by the union of its children, so a mark
   *  hanging above the line is not truncated.
   *
   *  ON A PATH RUN (`onPath`) marks are refused, and say so once: the curve
   *  is resolved at paint and a mark is placed during layout, so the only
   *  answer available here would be the straight baseline the run does not
   *  use. `beatsOf` reports the curve, and rides it. */
  Element& mark(Selector where, Element what);

  /** Text leaves only: how lines sit inside the node's width (SigilWeave
   *  TextAlignment — kStart/kCenter/kEnd/kJustify). Meaningful when the
   *  node is WIDER than its text (explicit width, grow, stack stretch);
   *  intrinsic-width text has nothing to align within. */
  Element& textAlign(sigil::weave::TextAlignment a);

  /** Text leaves only: lay this passage out in VERTICAL-RL CJK columns
   *  (`sigil::weave::WritingMode::kVerticalRL`) instead of horizontal
   *  lines. Characters run top to bottom, columns advance RIGHT TO LEFT
   *  from the node's right edge, and the node's width is the measure the
   *  columns wrap within.
   *
   *  Per character the mode is UTR#50's: ideographs stand upright and take
   *  their `vert` forms, Latin lies on its side. A run that wants
   *  otherwise says so in its own style — `TextStyle::shaping.verticalForm`
   *  is `kUpright`, `kRotated` or `kTateChuYoko` — on a `rich()` run or
   *  through `spanStyle`.
   *
   *  A vertical leaf MEASURES ON THE OTHER AXIS: its main extent is its
   *  HEIGHT, and its intrinsic width is one column pitch per column. It has
   *  no baseline — the reading axis is y — so for `Align::Baseline` it
   *  reports its first character's baseline, which lines a column's opening
   *  character up with a horizontal neighbour's first line.
   *
   *  `onPath` IGNORES IT: a path run's baseline is its own geometry and has
   *  no columns to advance. Setting both warns once and the path wins. */
  Element& writingMode(sigil::weave::WritingMode mode);

  // ---- span restyling: the type treatment, addressed by selector -------
  //
  // The same `sel::` vocabulary the fx() tracks address glyphs with, used
  // to say what a range LOOKS LIKE rather than how it moves. Each verb
  // takes an ordered list — call any of them as many times as the passage
  // needs — and a LATER DECLARATION WINS wherever two overlap, so a broad
  // rule followed by a narrow exception reads in the order it is written.
  //
  // They apply to every content form alike: plain `text(utf8, style)`,
  // `rich()` spans, and the `shared_ptr<Paragraph>` overload, because all
  // three are one materialized paragraph by the time a restyle runs.
  //
  // The three are ordered by WHAT THEY ARE ALLOWED TO DISTURB. `spanPaint`
  // repaints and nothing else. `spanAxis` also changes the outline, but
  // only along an advance-invariant axis, so the pen positions stand.
  // `spanStyle` may change anything and re-shapes to do it.
  //
  // The two that run on the PARAGRAPH — `spanPaint` and `spanStyle` —
  // resolve their selection as TEXT RANGES, not glyphs: `sel::text` and
  // `sel::regex` go through weave's query layer, `sel::word`, `sel::words`,
  // `sel::sentence` and `sel::range` through the paragraph's own structure,
  // and `sel::line` through the layout. `Selector::take` and
  // `Selector::drop` slice GLYPHS inside a unit, which a text range cannot
  // express — an `sel::each` selector restyles its whole units here, and
  // the slice is ignored with a warning.
  //
  // A `sel::line` restyle addresses THE LAYOUT OF THE TEXT BEFORE THE
  // RESTYLE, and costs a second layout pass. It does not chase its own
  // result: a `spanStyle` on a line that moves the line breaks leaves the
  // selection where the first breaking put it.
  //
  // `spanAxis` runs on the GLYPHS instead, being a track, so it takes the
  // selector vocabulary whole: a slice addresses the glyphs it names rather
  // than widening to their units.

  /** Text leaves only: repaint the range this selector finds — a colour, a
   *  shader, an underline, an added glow pass. PAINT ONLY, so it NEVER
   *  re-shapes and never relayouts: the glyphs are exactly the glyphs the
   *  unrestyled text shaped, drawn differently. */
  Element& spanPaint(Selector where, sigil::weave::PaintStyle paint);
  /** Text leaves only: restyle the range this selector finds with a
   *  complete TextStyle — a different face, size, weight or tracking as
   *  well as paint. Re-shapes, and only the words the range covers: the
   *  shaping cache is content-addressed, so the rest of the paragraph is
   *  reused as it stands. */
  Element& spanStyle(Selector where, sigil::weave::TextStyle style);
  /** Text leaves only: hold a VARIABLE-FONT AXIS at one coordinate over the
   *  range this selector finds, WITHOUT reshaping it.
   *
   *  The advance-invariant middle the other two verbs leave out. `spanPaint`
   *  cannot carry a face or an axis at all; `spanStyle` can, and re-shapes
   *  the words it touches to do it. A grade (GRAD) is advance-invariant BY
   *  CONSTRUCTION — it thickens a letter without moving the letter after it
   *  — so it is exactly the restyle that can keep the layout the paragraph
   *  already has, and this is the verb that says so.
   *
   *  GATED like every draw-time axis: the runtime probes the range's faces
   *  once per axis and REFUSES one that moves advances, drawing at the
   *  shaped coordinates and warning once. An axis that moves advances is a
   *  reshape, which is what `spanStyle` is for.
   *
   *  SUGAR over `fx()`, and it inherits what that means. The coordinate is a
   *  `GlyphMod::axis` on a track addressing @p where, so it goes through the
   *  same size-scaled snapping ladder a driven axis does; it composes with
   *  entrances and loops rather than being hidden by them; two declarations
   *  overlapping resolve LATER-WINS, as the other two span verbs do, because
   *  an axis coordinate is a substitution and substitutions are
   *  last-one-wins; and the leaf draws through the batched glyph path, which
   *  paints glyphs and not a span style's underline or strikethrough. */
  Element& spanAxis(Selector where, const char (&tag)[5], float value);

  // ---- layout options, fluently ----------------------------------------
  //
  // The general knobs of `weave::ParagraphLayoutOptions`, as setters that
  // work on every content form. The rest of that struct — justification
  // elasticity, Knuth-Plass tolerance, tab stops, line-metric overrides —
  // stays behind the `shared_ptr<Paragraph>` overload, which takes the
  // whole options value.
  //
  // ON THE PARAGRAPH OVERLOAD THESE OVERRIDE FIELD BY FIELD, and only the
  // fields actually set: options passed to `text(paragraph, options)` stand
  // for everything a setter did not name. Setting none of them leaves the
  // passed options untouched.

  /** Text leaves only: greedy (the fast default) or Knuth-Plass optimal
   *  line breaking. */
  Element& lineBreak(sigil::weave::LineBreakStrategy strategy);
  /** Text leaves only: whether soft-hyphen break opportunities are taken,
   *  and what Knuth-Plass charges for taking them. */
  Element& hyphenation(sigil::weave::HyphenationOptions options);
  /** Text leaves only: the marker appended to the last line when the text
   *  overflows its geometry. Empty disables it. */
  Element& ellipsis(std::u8string_view marker);
  /** Text leaves only: use at most this many lines (CSS line-clamp); the
   *  rest reports as overflow and `ellipsis()`, when set, lands on the
   *  clamped line. 0 is unclamped. */
  Element& maxLines(int lines);
  /** Text leaves only: how a paragraph-final or hard-break-final line sits
   *  under `TextAlignment::kJustify` — its own alignment, or `justify` to
   *  stretch it to the full measure like every other line. Inert under the
   *  other alignments, which have no special last line. */
  Element& lastLine(sigil::weave::TextAlignment alignment,
                    bool justify = false);

  /** Text leaves only: paint the GLYPHS with this material, mapped to
   *  TEXT-METRIC space — the material's unit square lands with x across
   *  the widest line and y from the first line's CAP TOP (real cap height
   *  from the face's metrics) to the last line's baseline. That is what
   *  makes chrome type work at any size: author the ramp once in [0,1] and
   *  its horizon crosses the capitals whatever the font size, with no
   *  hand-positioned gradients.
   *
   *  Supersedes the style's foreground paint. A live material re-resolves
   *  per frame. COMBINES with `fx()`: a letter in flight is painted with
   *  the metric material exactly as a resting one is, so a chrome
   *  wordmark can also be a staggered entrance. */
  Element& textFill(Material m);

  /** Strokes the GLYPHS, under the fill — engraved display type, an
   *  outlined label, a caption that has to survive over an image.
   *
   *  NOT `Element::stroke()`, which dresses the node's BOX outline and is
   *  a different thing entirely. This one thickens the letterforms.
   *
   *  Composes with `textFill()` — the stroke is a pass beneath whatever
   *  fills the letterforms — with the style's own underlays and overlays,
   *  which it joins rather than replaces, and with `fx()`, which carries
   *  every pass along as the glyph moves. */
  Element& textStroke(float width, Fill fill);
  /** Text leaves only: lay the run out along a PATH instead of a line.
   *  See TextPath. Single-line runs; the node's own box still sizes the
   *  path, so give it the box the curve should be inscribed in
   *  (`util::disc`-style: width(2r).height(2r).centerAt(centre)).
   *
   *  Interacts with the rest of the text surface the way you would hope:
   *  the style's underlays, overlays and decorations all still draw, and
   *  `fx()` wins if both are set (a track draws its own batched buckets
   *  along the flow, not along the curve). */
  Element& onPath(TextPath spec);

  // ---- identity, caching, transitions ----
  /** The author-owned identity: what the reconciler matches a child by
   *  across describes, and what `connector`/`rail`/`spans::fit` borrow
   *  geometry by.
   *
   *  ON A `slot()` IT RENAMES THE MOUNT. A slot's name IS its key — there
   *  is no second field — so `slot("hud").key("panel")` produces a slot
   *  called "panel", and `renderSlot("hud")` then finds nothing and does
   *  nothing. It warns once, in Release too, because the visible symptom
   *  is an empty region rather than an error. */
  Element& key(std::string_view k);
  Element& cache(Cache c);
  /** Texture-bake resolution multiplier (Cache::Texture only; 0.1–1).
   *  The bake rasterizes at `factor` times the device scale and the blit
   *  scales it back up with linear sampling.
   *
   *  ALMOST ALWAYS THE WRONG LEVER. It cheapens the BAKE, which happens
   *  once, and taxes every BLIT with an upscaling resample, which happens
   *  forever — backwards for the bake-once/blit-every-frame node
   *  Cache::Texture exists for. Reach for it only when something forces
   *  FREQUENT re-bakes (a live material stepping at its own rate, a
   *  resizing node) AND the content is soft enough to survive the
   *  resample. Sharp text and 1 px hairlines never belong under a reduced
   *  bake. */
  Element& bakeScale(float factor);
  Element& transition(Transition t);  // node default for plain constants
  /** Container stagger: child i's subtree enters with an EXTRA
   *  order·each delay on all its animate() mount transitions, compounding
   *  through nested staggered containers. `from` picks the origin — Start
   *  (declaration order), End (last child first, a bottom-up cascade
   *  without reordering paint), Center (ripple outward). One call, no
   *  per-child delay arithmetic:
   *  `column().staggerChildren(33ms, Stagger::From::End).children(rows)`. */
  Element& staggerChildren(std::chrono::milliseconds each,
                           Stagger::From from = Stagger::From::Start);

  // ---- composition ----
  Element& child(Element e);
  template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, Element>
  Element& children(R&& range) {
    for (auto&& e : range) child(std::move(e));
    return *this;
  }

  /** @private reconciler access */
  const std::shared_ptr<detail::ElementNode>& node() const {
    return m_node.value;
  }
  explicit Element(std::shared_ptr<detail::ElementNode> n)
      : m_node(std::move(n)) {}

 private:
  /** Register a decoration's declared derive borrows (see
   *  BorrowingDecoration). Every slot that takes a Decoration must call
   *  this: a borrow honoured in some slots and not others resolves to
   *  nothing in the others, and draws nothing, with no diagnostic. */
  void claimBorrows(const Decoration& d);

  /** The shared body of stroke(Spans,…) and background(Spans,…). `half` is
   *  a detail::StrokePass::Half, passed as an int so the exported header
   *  does not have to name an internal enum. */
  Element& addSpanPass(Spans where, Decoration what, std::string name,
                       int half);

  /** Bind the optional LOCAL label an unqualified mark slot took to the
   *  mark it just appended, for `parts::named()`. `slot` is a
   *  detail::MarkSlot as an int, for the same reason addSpanPass takes
   *  its half that way. */
  void labelMark(int slot, size_t index, std::string name);

  /** Copy-on-write handle: Element stays a cheap value, but fluent mutation
   *  can never alter another copy or a description retained by Composer. */
  struct NodeHandle {
    explicit NodeHandle(std::shared_ptr<detail::ElementNode> node)
        : value(std::move(node)) {}

    detail::ElementNode* operator->();
    const detail::ElementNode* operator->() const;

    std::shared_ptr<detail::ElementNode> value;
  };

  NodeHandle m_node;
};

// ---- factories -----------------------------------------------------------

Element box();
/** Overlap container: children share the box, painted in (zIndex,
 *  declaration order). EVERY child is absolute — the container sets it
 *  after the child's own layout props, so a child cannot rejoin the flex
 *  flow from inside a stack (it keeps its insets, which is what absolute
 *  is for: `.top(12).right(12)` pins a corner). Mixed flow wants a box
 *  with a stack inside it. */
Element stack();
/** A container whose children carry their OWN rects and skip Yoga
 *  entirely — no flex nodes anywhere below it. Generated geometry
 *  (tilings, lattices, node graphs, fields drawn as real elements) never
 *  wants layout, and this is how to say so.
 *
 *  The child spelling is the ordinary placement longhand:
 *  `.left(x).top(y).width(w).height(h)` — px, or pct() against the
 *  parent's rect; an open width/height with an opposing `.right()`/
 *  `.bottom()` pins the far edge instead; a text leaf with an open
 *  extent measures against its resolved (or the parent's) width.
 *  Rects nest: a child's children position inside ITS rect, the whole
 *  subtree Yoga-free. Everything else about the children is ordinary —
 *  decorations, strokes, masks, transitions, stagger, zIndex, hitTest,
 *  bounds() — because instances still exist; only their layout engine is
 *  gone. A large generated field therefore costs one Yoga node rather
 *  than one per element.
 *
 *  The container ITSELF is an ordinary box in its parent's flow: size it
 *  with dims or insets, because it does NOT auto-size from its children.
 *  NOT SUPPORTED INSIDE, and ignored silently when written: flex props,
 *  centerAt, layout() schemes, flowAround text. Those need the flex
 *  world. */
Element positioned();
Element text(std::u8string utf8, sigil::weave::TextStyle style);
/** Mixed-style text as a COMPARABLE VALUE — see RichText. A re-described
 *  identical value prunes, which is the whole difference between this and
 *  the pointer overload below. */
Element text(RichText spans);
/** Full-control text: a prebuilt Paragraph (spans, mixed styles) plus
 *  ParagraphLayoutOptions (justification, hyphenation, Knuth–Plass,
 *  overflow…). The paragraph is shared by reference: reuse one
 *  shared_ptr across renders to keep shaping caches warm; a fresh
 *  pointer means "content changed" and re-shapes. */
Element text(std::shared_ptr<sigil::weave::Paragraph> paragraph,
             sigil::weave::ParagraphLayoutOptions options = {});
Element image(std::shared_ptr<const sigil::image::ImageAsset> asset);
/** A box whose content is one paint program (≡ box().background(p)).
 *
 *  TWO COSTS AN AUTHOR MUST KNOW. First, it is cached like any static
 *  subtree, so a program that reads the clock — or changes for any other
 *  reason without a re-describe — MUST declare `.cache(Cache::None)`, or
 *  its first frame is recorded and replayed frozen. Second, the program is
 *  an incomparable callable, so the structural prune cannot prove a
 *  custom() node unchanged and it re-records on every render(). Wrap it in
 *  memo(), keep its Element value stable across renders, or use the keyed
 *  overload below. Value decorations (PathFormat, Slice, Shadow) prune
 *  automatically — prefer them for static chrome.
 *
 *  IT SIZES LIKE AN EMPTY BOX, and the failure is silent. Being literally
 *  `box().background(p)`, a custom() leaf has no intrinsic size: dropped
 *  into an `absolute().inset(0)` parent it stretches on the cross axis and
 *  measures ZERO on the main one, so the program runs against a
 *  zero-height context and draws nothing at all. Give it dims, or make it
 *  `absolute().inset(0)` itself — which is exactly what
 *  `instancing::instances()` returns, for exactly this reason. */
Element custom(PaintProgram program);
/** The PRUNABLE spelling: the key is the program's IDENTITY, on the same
 *  author contract as `shapes::parametric(key, …)`. One key must always
 *  name one drawing at one parameterisation — fold anything that varies
 *  into the key, or two different pictures compare equal and the stale one
 *  replays. Two describes with equal keys compare EQUAL and the node
 *  prunes; the unkeyed form above re-records every render(). */
Element custom(std::string_view key, PaintProgram program);

/** A container whose children are placed by @p scheme instead of
 *  flexbox (nests freely inside flex and vice versa). The container
 *  itself is sized by its own dims/flex; children are measured by
 *  Yoga/SigilWeave, then positioned and sized by scheme.place() in a
 *  bounded second layout pass. */
template <LayoutScheme L>
Element layout(L scheme);

namespace detail {
Element makeLayout(
    std::function<std::vector<SkRect>(const LayoutInput&)> place);
}  // namespace detail

template <LayoutScheme L>
Element layout(L scheme) {
  return detail::makeLayout(
      [s = std::move(scheme)](const LayoutInput& in) { return s.place(in); });
}

/** A named mount point whose content is supplied independently via
 *  `Composer::renderSlot()`. The surrounding tree is not re-described when
 *  the slot updates, so its caches stay valid — this is how two data
 *  domains that change at different rates share one tree.
 *
 *  THE NAME IS STORED AS THE ELEMENT'S `key`, the same field `.key()`
 *  writes: `slot("hud").key("panel")` is a slot named "panel", and
 *  `renderSlot("hud")` then finds nothing and does nothing. Name the slot
 *  here and only here; `.key()` warns once if called on one anyway. */
Element slot(std::string_view name);

/** A relationship as a first-class element: a path routed between two
 *  keyed nodes' resolved bounds, stroked by the connector's foreground
 *  decorations (attach a PathFormat — the routed path arrives as
 *  PaintContext::outline). Straight line by default; supply a router
 *  for anything else. Position it absolute().inset(0) over the nodes
 *  it connects.
 *
 *  `gap` is `Anchor::gap` under another name: it pulls each END of the
 *  routed path back along itself by that many px, clamped so short routes
 *  keep a visible run. Routes run centre to centre, and a node's box can
 *  be much larger than its visible shape — under `sdf::` chrome, for
 *  instance — so the gap is how a wire stops at the glow instead of
 *  piercing it. */
using Router = std::function<SkPath(const SkRect& from, const SkRect& to)>;
Element connector(std::string_view fromKey, std::string_view toKey,
                  Router router = {}, float gap = 0.0f);

/** A rail endpoint/waypoint: a NORMALIZED point on a keyed node's resolved
 *  bounds ((0,0)=top-left, (1,1)=bottom-right — the binding form tldraw and
 *  Excalidraw both converged on; never absolute coordinates, so rails
 *  survive layout, drag, and reflow). `gap` pulls a TERMINAL anchor back
 *  along its segment (breathing room at the ends; ignored on waypoints). */
struct Anchor {
  std::string nodeKey;
  SkPoint norm = {0.5f, 0.5f};
  float gap = 0.0f;
  bool operator==(const Anchor&) const = default;
};

/** Routes an ordered run of resolved anchor points into the rail's path —
 *  stock ones in <sigilcompose/Routers.h> (polyline, octilinear); write your
 *  own for anything else. Straight polyline when omitted. */
using RailRouter = std::function<SkPath(std::span<const SkPoint>)>;

/** The component that IS a line: a path threaded through an ordered span of
 *  anchors (a transit line through its stations, a wire through ports),
 *  resolved in the derive phase and re-routed whenever an anchored node
 *  moves. The routed path becomes PaintContext::outline, so PathFormat
 *  strokes, ContourWalk stamps and span masks all dress it — a rail with
 *  `.mask(by::spans(spans::upTo(with(1.0f, {800ms}))))` DRAWS ITSELF.
 *  Position it absolute().inset(0) over the nodes it threads, as with
 *  connector(). */
Element rail(std::vector<Anchor> anchors, RailRouter router = {});

/** A BAND: the shape a spine sweeps out at a given width across it.
 *
 *      band(shapes::circle(), across(22)).inward().fill(brass)
 *      band(around("dial"), across(14)).stroke(spans::edges(6), rule)
 *
 *  It is an ordinary element in every way that matters — it lays out,
 *  hosts children, fills, clips and takes stroke passes like any other
 *  shape. What it adds is that its shape is DERIVED: it owns an
 *  (along, across) space over its spine, `along` a fraction of arc length
 *  and `across` px on the normal (see bandPointAt for the sign), and
 *  `across(...)` takes a Profile, so a taper is the same value a strand
 *  or a ribbon width uses.
 *
 *  IT DOES NOT HIT-TEST AS ITS SHAPE. Hit testing consults the node's own
 *  shape value, and a band's silhouette is derived rather than set there,
 *  so a band hits as its LAYOUT BOX — a wider region than the mark you can
 *  see.
 *
 *  An authored spine is a `Shape`, exactly like shape()'s value: a
 *  comparable generator (any `shapes::` value) prunes; a raw callable is
 *  the escape hatch that never compares equal — memo() such a node, or
 *  hold the Shape value stable, to prune it. A borrowed spine
 *  (`around(key)`) is a comparable value and prunes on its own.
 *
 *  Formation is explicit: `.centered()` (the default) straddles the
 *  spine, `.outward()` and `.inward()` take one side. The spine is guide
 *  DATA, never an element — a path participates as an element's shape, as
 *  borrowed geometry (`around(key)`, resolved in the derive phase), or as
 *  pure guide data in no tree, and this is the third case.
 *
 *  The profile's `max()` is what the paint cull grows by, so a band whose
 *  width varies is never silently clipped. */
Element band(Shape spine, Across width);
Element band(Around spine, Across width);

/** The band's own (along, across) space, addressable: `along` is a
 *  fraction of the spine's total arc length, `across` is px on the normal.
 *
 *  **Positive `across` is to the LEFT of travel**, which in screen space
 *  (y down) is OUTSIDE a clockwise path — SkPath's own direction for rects
 *  and circles, so `.outward()` exits the shape.
 *
 *  THIS IS THE ONE STATEMENT OF THAT CONVENTION for the whole library.
 *  `Profile::across`, `strand::offset`, `lines::offsetAcross`,
 *  `lines::Rail::across`, `kit::brush::shapers::offset` and
 *  `TextPath::offset` all mean this same side. Anything placing content on
 *  a band reads it here, so the placement and the band's own geometry
 *  cannot disagree. */
SkPoint bandPointAt(const SkPath& spine, float along, float acrossPx);

// ---------------------------------------------------------------------------
// The DERIVE family, gathered under one word

/** Everything that asks "where did that keyed node land, and give me more
 *  content because of it" — the DERIVE phase.
 *
 *  Its members are `flowAround`, `connector` and `rail` (with `routers::`
 *  as their pluggable seam), `band(around(key))`, `spans::fit(key)` and a
 *  decoration's `strand::from(key)`. Six spellings, one mechanism; the
 *  aliases below exist so it can be found under one name.
 *
 *  THE RULES THEY SHARE — one flat edge store, walked once per render:
 *
 *   1. **AN UNKNOWN KEY IS SILENT, across the whole family.**
 *      `flowAround("typo")`, `spans::fit("typo")`, `around("typo")`, a
 *      connector naming a node that is not in the tree — each resolves to
 *      nothing and draws nothing, with no diagnostic. A misspelled key
 *      looks exactly like a feature you did not write.
 *   2. **ONE SECOND PASS, cycle-guarded.** Backward influence inside a
 *      frame is this declared exception and nothing else: derive answers
 *      are computed from the FIRST layout and fed to at most one more
 *      pass. A borrow that would close a cycle is dropped, not chased.
 *   3. **The answer can lag by a frame** when the borrowed node's own
 *      geometry only settles during that layout, so a borrow taken on the
 *      very first frame may resolve against a not-yet-final rect.
 *   4. **Flat, not recursive.** Routed nodes and flowing text are flat
 *      lists in tree order plus a back-index from anchor key to routes, so
 *      a tree with no derived content pays nothing and `routesAt(key)`
 *      answers in time proportional to the routes at that node.
 */
namespace derive {
/** A relationship as an element — see connector() above. */
using sigil::compose::connector;
/** A path threaded through anchors — see rail() above. */
using sigil::compose::rail;
/** A spine borrowed from a keyed element — `band(derive::around("dial"),
 *  across(14))`. */
using sigil::compose::around;
/** The family's text member as a free verb: `derive::flowAround(el,
 *  "fig", 8)` == `el.flowAround("fig", 8)`. The method is the ergonomic
 *  form, since it chains; this exists so the whole family can be found
 *  under one name. */
inline Element flowAround(Element el, std::string_view key,
                          float margin = 0.0f) {
  el.flowAround(key, margin);
  return el;
}
}  // namespace derive

/** One-shot element render: reconciles, lays out, and records the
 *  paint into a picture. With an empty @p maxSize the tree takes its
 *  intrinsic (content) size; a non-empty one bounds it (root max
 *  dims). Bindings are sampled at their current values; transitions
 *  don't run — there is no live timeline. This is the bake primitive
 *  behind ContourWalk element stamps, and generally "an element tree
 *  as a brush".
 *
 *  THE INTRINSIC SIZE COMES FROM THE ROOT'S CHILDREN, not from the root's
 *  own dims, and this catches people out: `snapshot(box().width(32).
 *  fill(…))` bakes at CONTENT size and quietly ignores the 32. Wrap the
 *  sized tree in a plain `box().child(...)` and the dims are honoured,
 *  because they now belong to a child. */
sk_sp<SkPicture> snapshot(Element root, sigil::weave::FontContext& fonts,
                          SkSize maxSize = SkSize::MakeEmpty());

/** Slicing ONE baked picture into a run of tiles.
 *
 *  A strip far longer than any texture — a marquee, a scrolling ribbon, a
 *  hanging scroll — is authored as a single element tree and baked with
 *  `snapshot()`, which has no size limit because a picture is vector. The
 *  consumer then wants it as N tile-sized rasters. That slice is a clip
 *  and a translate and nothing else: **there is no windowed bake, and
 *  there is no need for one.** Replaying the whole picture per tile,
 *  behind `sliceable()` below, is as cheap as extracting each tile's ops
 *  in advance would be.
 *
 *  What DOES go wrong is the transform, and that is what these two verbs
 *  exist to own.
 *
 *  **Author the strip in the tiles' own orientation.** If the tiles are
 *  tall, the tree is a `column()`; if they are wide, a `row()`. The
 *  temptation is to author across and transpose on the way out, and a
 *  transpose has determinant -1 — it composes with whatever mirroring the
 *  consumer's own sampling already applies, and the mirror bookkeeping
 *  stops being local to either side. `Flow` therefore offers only the two
 *  non-transposing slices, on purpose.
 *
 *  **`Facing` is a statement about the CONSUMER, not the picture.** A
 *  texture sampled onto a surface whose u runs backwards — a ribbon wall
 *  mirrors its own u — shows glyphs reversed unless the tile was baked
 *  reversed to match. `Facing::Mirrored` pre-flips ACROSS the strip, on
 *  the axis perpendicular to `flow`, so that such a consumer reads it the
 *  right way round. Get this wrong and the art is legible in an offline
 *  PNG of the tile and mirrored on the surface, so it will not show up
 *  until the texture is in place. */
namespace tiles {

/** Which way the run of tiles marches through the picture. */
enum class Flow {
  Down,   ///< a column strip: tile k is the k-th slice down
  Across  ///< a row strip: tile k is the k-th slice rightward
};

/** Whether the tile is pre-flipped for a consumer that samples mirrored. */
enum class Facing {
  Forward,  ///< the tile reads like the picture
  Mirrored  ///< flipped across the strip, for mirrored sampling
};

/** The canvas transform that brings tile @p index of a @p tile -sized run
 *  into view. Concat it, then draw the picture:
 *
 *  ```
 *  SkAutoCanvasRestore restore(canvas, true);
 *  canvas->clear(SK_ColorTRANSPARENT);
 *  canvas->concat(tiles::window(size, k, Flow::Down, Facing::Mirrored));
 *  canvas->drawPicture(strip);
 *  ```
 *
 *  The surface's own bounds are the clip, so nothing else is needed —
 *  neighbouring tiles share their boundary texels and the seams vanish. */
SkMatrix window(SkISize tile, int index, Flow flow = Flow::Down,
                Facing facing = Facing::Forward);

/** The same picture, re-recorded behind a bounding-box hierarchy, so each
 *  `window()` replay visits only the ops that meet its tile instead of all
 *  of them.
 *
 *  Worth it past a handful of tiles and not before: building the
 *  hierarchy costs a pass over the picture, which a two-tile run does not
 *  earn back. Slicing WITHOUT it is quadratic, because every tile walks
 *  every tile's ops, so the saving grows with the tile count while the
 *  build cost does not.
 *
 *  It exists as a verb because the obvious one-liner has a trap:
 *  `drawPicture()` into a recorder stores a NESTED reference the
 *  bounding-box hierarchy cannot see into, leaving the tree empty and the
 *  replay cost unchanged. This flattens with `playback()` instead. */
sk_sp<SkPicture> sliceable(const sk_sp<SkPicture>& art);

}  // namespace tiles

/** A face's vertical metrics at a given size, without laying anything out.
 *
 *  A compose text node's top is the LINE BOX top, while type is usually
 *  positioned against its CAP TOP — so aligning text to a coordinate taken
 *  from a design or a reference image needs the slack between the two, and
 *  `measure()` returns only an `SkSize`. `capSlack()` below is that
 *  number.
 *
 *  `capHeight` and `xHeight` are what the face itself reports; both fall
 *  back to a fraction of the ascent when a face reports zero, which some
 *  do. All values are positive distances in px, with `ascent` measured
 *  above the baseline. */
struct TextMetrics {
  float ascent = 0;      ///< baseline to the top of the em box (positive)
  float descent = 0;     ///< baseline to the bottom (positive)
  float leading = 0;     ///< the face's recommended extra line gap
  float capHeight = 0;   ///< baseline to the top of a flat capital
  float xHeight = 0;     ///< baseline to the top of a lowercase x
  float lineHeight = 0;  ///< ascent + descent + leading
  /** How far the line box's top sits above the cap top — the number that
   *  turns "place this at the reference's y" into a coordinate. */
  float capSlack() const { return ascent - capHeight; }
};

TextMetrics metrics(const sigil::weave::TextStyle& style,
                    sigil::weave::FontContext& fonts);

/** Shape ONE RUN without building an Element: per-glyph advances in px, in
 *  visual order, through the same shaping path a text() leaf takes, so
 *  kerning and ligatures are real. The result's length is the GLYPH count,
 *  which is neither the byte nor the code-point count.
 *
 *  Pen positions are the running prefix sums, so hand-placing N glyphs
 *  costs one layout here rather than N text() leaves and N `measure()`
 *  calls. A space between two words is a gap the flow leaves rather than a
 *  glyph, so it rides the advance of the glyph before it and the sums stay
 *  true across a whole sentence; the sums therefore add up to the run's
 *  laid-out extent, not to the ink alone. The pen starts at the FIRST
 *  GLYPH, so leading whitespace is no part of the run.
 *
 *  Single style, no wrapping: the run is laid on one unbounded line. A
 *  '\n' starts a new line and resets the positions after it, so pass a
 *  RUN and not a paragraph.
 *
 *  This is the STATIC answer, for a run that is not in the tree. For a
 *  MOUNTED, animated run — one a `text()` leaf is drawing and an `fx()`
 *  track is cascading — `Composer::beatsOf` is the answer instead: it
 *  reports the rect the layout actually placed each unit in, which follows
 *  a wrap, a mixed-style run and a path baseline that no single-style
 *  measurement can see. */
std::vector<float> measureRun(std::u8string_view utf8,
                              const sigil::weave::TextStyle& style,
                              sigil::weave::FontContext& fonts);

/** WHERE THE LETTERS SIT: `measureRun`'s advances already summed. Entry i
 *  is glyph i's pen x, measured from the first glyph's pen, and there is
 *  ONE PAST-THE-END ENTRY, so `runPens(...).back()` is the run's whole
 *  laid-out width and `pens[i + 1] - pens[i]` is glyph i's advance. `n`
 *  glyphs give `n + 1` entries, and an empty run gives the single entry 0.
 *
 *  THE ONE RULE TO KNOW, which is `measureRun`'s and is stated here because
 *  this is the form that gets read: A SPACE RIDES THE PREVIOUS ADVANCE.
 *  An inter-word space is a gap the flow leaves between positioned runs
 *  rather than a glyph, so it is no entry of its own; whatever the layout
 *  left between one glyph's pen end and the next one's origin is folded
 *  into the advance of the glyph BEFORE it. That is exactly what makes
 *  these sums reproduce the pen positions the layout used, across a whole
 *  sentence and not only inside one word. Two steps are deliberately not
 *  folded: a '\n' restarts the pen, and a BACKWARDS step between two words
 *  is bidi reordering, which visual-order prefix sums cannot express (a
 *  backwards step INSIDE a word is ordinary kerning and does count). The
 *  pen starts at the first glyph, so leading whitespace is no part of the
 *  run and entry 0 is always 0.
 *
 *  Same shaping path, same single style, same unbounded line as
 *  `measureRun` — and the same division of labour: this is the STATIC
 *  answer for an unmounted run, `Composer::beatsOf` is the answer for a
 *  mounted, cascading one. */
std::vector<float> runPens(std::u8string_view utf8,
                           const sigil::weave::TextStyle& style,
                           sigil::weave::FontContext& fonts);

/** One-shot intrinsic measurement: what size would this element take?
 *  Runs the same reconcile+layout as snapshot() and returns the root's
 *  resolved size without painting. The sizing primitive behind
 *  content-fit chrome (marquees, tooltips, badges): measure the content,
 *  then describe the real tree with the answer. Same sampling rules as
 *  snapshot() — bindings at current values, no transitions. */
SkSize measure(Element root, sigil::weave::FontContext& fonts,
               SkSize maxSize = SkSize::MakeEmpty());

// ---------------------------------------------------------------------------
// env — an INHERITED VALUE, read where a component is composed
//
// SwiftUI's Environment and React's context, for a library whose describe
// phase is an ordinary C++ call tree. Passing `const Theme&` is idiomatic
// for your OWN components; what has no answer without this is the
// library's own — a `feed::`, a decoration nested four levels down —
// each of which must otherwise be handed its colours by whoever composes
// it, so a theme change is a mechanical edit at every call site.
//
// THE ONE THING TO UNDERSTAND. Describe here is EAGER and BOTTOM-UP:
// `box().child(panel())` calls `panel()` before the box exists, and every
// component is a plain function whose arguments are evaluated inside the
// enclosing scope. So the describe-time call stack IS the element tree,
// and the C++ answer to "inherit down a call stack" is dynamic scope:
//
//     env::Provide<Palette> theme(dark);      // binds for this scope
//     return box().child(panel());            // panel() reads it
//
//     // …four levels down, in a component that was never handed it:
//     const Palette *p = env::inherited<Palette>();
//
// WHY THIS DOES NOT COST THE PRUNE. An inherited value is read DURING
// DESCRIBE and lands in the reading node's own props, so the reconciler's
// property comparison is already an exact dependency tracker: a node whose
// props came out identical prunes, whether or not it read the environment,
// and a node whose colour actually moved re-patches. The element tree the
// Composer sees is environment-INDEPENDENT — the value is baked in by then
// — so no kernel phase learns a new concept and nothing invalidates a
// subtree wholesale.
//
// The alternative shape, resolving a theme at PAINT through bound Outputs,
// trades the wrong way: it makes every themed node permanently volatile
// and therefore uncacheable, paying per frame forever to save work on the
// rare frame where the theme actually changes.
//
// THE ONE PLACE THE KERNEL HAD TO LEARN IT is `memo`, the only site in
// the library where a component function runs AFTER the author's scope
// has ended. A memo therefore captures the ambient stack at construction,
// compares it alongside its props, and re-establishes it around the
// deferred call — so `memo` stays a pure function of (props, environment)
// and cannot serve a stale theme. Everything else that takes a callable
// (a `ContourWalk` stamp, a `custom()` program) runs at derive or paint
// time with NO scope: capture what such a lambda needs by value at the
// call site, which is where the scope still exists.
//
// REQUIREMENTS ON AN INHERITED TYPE, and what its equality means: it is
// copyable and equality-comparable, and two values are equal exactly when
// describing anything under them yields props that compare equal. The
// comparison is therefore structural and exact — SkColor4f bitwise,
// typefaces by pointer — never perceptual and never epsilon'd, because
// the consumer of the answer is the prune.
//
// MATERIALISE DERIVED VALUES INTO THE TYPE. A theme that carries a
// `std::function` derivation rule instead of the colours it produces is
// incomparable, so it never compares equal to itself and every memo below
// it becomes a permanent miss. Run the function once and store the
// results.
//
// There is deliberately NO library-wide `Theme` type. Bindings are keyed
// by C++ TYPE, and the key a library component uses is its own existing
// props type (`feed::TextOptions`), so this is a transport channel rather
// than a design-token vocabulary.

namespace detail {

/** One ambient binding, type-erased. `type` is a per-T address so no RTTI
 *  is needed and the type test is a pointer compare. */
struct EnvEntry {
  const void* type = nullptr;
  std::shared_ptr<const void> value;
  bool (*equal)(const void*, const void*) = nullptr;
};

/** A captured ambient stack, innermost LAST. Empty is the overwhelmingly
 *  common case and costs one empty vector — the feature is free unused. */
using EnvSnapshot = std::vector<EnvEntry>;

/** The live describe-time stack. Thread-local: compose is a guest and
 *  describes on whatever thread the host calls on. */
EnvSnapshot& envStack();

/** Value equality over two captured stacks: same bindings, in the same
 *  order, each equal by its own `operator==`. Identical holders short-
 *  circuit. This is what makes a memo's environment part of its key. */
bool envEqual(const EnvSnapshot& a, const EnvSnapshot& b);

/** Re-establishes a captured stack around a DEFERRED describe (the memo
 *  invoke). Swaps rather than pushes: a deferred call must see exactly
 *  what its author's scope had, not that stack plus whatever the current
 *  reconcile walk happens to sit inside. */
class EnvRestore {
 public:
  explicit EnvRestore(const EnvSnapshot& snapshot);
  ~EnvRestore();
  EnvRestore(const EnvRestore&) = delete;
  EnvRestore& operator=(const EnvRestore&) = delete;

 private:
  EnvSnapshot m_saved;
};

template <class T>
const void* envTypeTag() {
  static const char tag = 0;
  return &tag;
}

}  // namespace detail

namespace env {

/** Bind `value` for every component described while this object lives.
 *  RAII and LIFO; an inner `Provide<T>` shadows an outer one, and other
 *  types are unaffected. Not copyable or movable — it is a scope. */
template <class T>
class Provide {
 public:
  explicit Provide(T value) {
    static_assert(std::is_copy_constructible_v<T>,
                  "an inherited value is a value");
    auto held = std::make_shared<const T>(std::move(value));
    m_self = held.get();
    detail::envStack().push_back(detail::EnvEntry{
        detail::envTypeTag<T>(), std::shared_ptr<const void>(std::move(held)),
        [](const void* a, const void* b) {
          return *static_cast<const T*>(a) == *static_cast<const T*>(b);
        }});
    m_depth = detail::envStack().size();
  }
  /** Unbinds THIS scope's binding and no other. Destroying providers out
   *  of LIFO order is misuse; when it happens, the destructor locates its
   *  own entry by the held value's identity and removes exactly that one
   *  — an unconditional pop would unbind a SIBLING that is still alive.
   *  The misuse warns; the well-nested path stays a compare and a
   *  pop_back, allocation-free. */
  ~Provide() {
    detail::EnvSnapshot& stack = detail::envStack();
    if (stack.size() == m_depth && stack.back().value.get() == m_self) {
      stack.pop_back();
      return;
    }
    SkDebugf(
        "[compose] env::Provide destroyed out of order — scopes must "
        "nest LIFO; removing only this scope's own binding\n");
    for (size_t i = stack.size(); i-- > 0;)
      if (stack[i].value.get() == m_self) {
        stack.erase(stack.begin() + (std::ptrdiff_t)i);
        return;
      }
  }
  Provide(const Provide&) = delete;
  Provide& operator=(const Provide&) = delete;

 private:
  size_t m_depth = 0;
  const void* m_self = nullptr;  // identity of the entry this scope pushed
};

/** The nearest enclosing binding of `T`, or nullptr when nothing bound
 *  one — which is a component's cue to use its own default, exactly like
 *  a React context's default value. Valid until the binding's scope ends
 *  (i.e. for the rest of the describe call that read it). */
template <class T>
const T* inherited() {
  const detail::EnvSnapshot& stack = detail::envStack();
  const void* tag = detail::envTypeTag<T>();
  for (size_t i = stack.size(); i-- > 0;)
    if (stack[i].type == tag)
      return static_cast<const T*>(stack[i].value.get());
  return nullptr;
}

/** The inherited value, or `fallback` — the one-liner spelling for a
 *  component that has a sensible default of its own. */
template <class T>
T inheritedOr(T fallback) {
  const T* found = inherited<T>();
  return found ? *found : std::move(fallback);
}

/** Is a binding of `T` in scope? For a component that must behave
 *  differently rather than just fall back to a default. */
template <class T>
bool bound() {
  return inherited<T>() != nullptr;
}

}  // namespace env

namespace detail {
Element makeMemo(std::any props,
                 std::function<bool(const std::any&, const std::any&)> equal,
                 std::function<Element(const std::any&)> invoke);
}  // namespace detail

/** Deferred description: `fn` runs only when `props` changed (by
 *  operator==) since the last render on this position/key — AND the
 *  ambient `env::` bindings are unchanged, because a memo is a pure
 *  function of (props, environment) and would otherwise serve the theme
 *  it first described under forever. The captured stack is re-established
 *  around the deferred call, so `env::inherited<T>()` inside `fn` reads
 *  what was bound where the memo was WRITTEN, not where it runs. */
template <ComponentProps P, ComponentFn<P> F>
Element memo(P props, F fn) {
  return detail::makeMemo(
      std::any(std::move(props)),
      [](const std::any& a, const std::any& b) {
        return std::any_cast<const P&>(a) == std::any_cast<const P&>(b);
      },
      [fn = std::move(fn)](const std::any& p) -> Element {
        return fn(std::any_cast<const P&>(p));
      });
}

// ---------------------------------------------------------------------------
// Composer — the retained side; a guest in the host's canvas

class Composer {
 public:
  /** BOTH REFERENCES ARE HELD, not copied, and both must outlive the
   *  composer. @p ticker drives transitions and, through its FrameClock
   *  when one is attached, PaintContext time; @p fontContext measures and
   *  shapes every text leaf. */
  Composer(motion::Ticker& ticker, sigil::weave::FontContext& fontContext);
  ~Composer();

  Composer(const Composer&) = delete;
  Composer& operator=(const Composer&) = delete;

  /** Layout viewport in canvas-space px; percent dims resolve here.
   *  The root element always fills the viewport (its own width/height
   *  are ignored, like the CSS root) — size content via children.
   *  An EMPTY size means INTRINSIC instead: the root sizes to its content
   *  and its own dims ARE respected. That is the rule the
   *  snapshot()/measure() path runs under. */
  void setSize(SkSize size);

  /** Feeds PaintContext::elapsedSeconds (one clock everywhere). Null
   *  freezes paint time at 0 — fine for static content and goldens. */
  void setClock(const motion::FrameClock* clock);

  /** Output view transform (color management): applied to the composer's
   *  whole output as the final stage — one saveLayer while set, zero cost
   *  when cleared (a default Effect{}). The intended source is an OCIO
   *  display/view baked to a 3D LUT (<sigilcompose/Ocio.h>), but any Effect
   *  works. Per-node caches are unaffected (this is post-cache, at
   *  composite). */
  void setView(Effect view);

  /** THE DECLARED INPUT SPACE — a declaration, NOT a conversion.
   *
   *  Compose composites in ENCODED sRGB and has no linear stage: every
   *  surface it paints into is N32Premul with no SkColorSpace, so the
   *  numbers an author writes are the numbers that land in the bytes. That
   *  is not configurable, because every channel weighting in the library —
   *  `by::luma`'s Rec. 601 coefficients first among them — is defined
   *  against it.
   *
   *  What this adds is the ability to SAY what you believe your colour
   *  values are, since "I deliberately author encoded sRGB" and "nobody
   *  thought about colour at all" otherwise produce identical trees.
   *  `EncodedSRGB`, the default, matches reality and is silent. Anything
   *  else is a mismatch the library can see, and it says so once: your
   *  values are still TREATED as encoded sRGB, so under a `LinearSRGB`
   *  declaration every channel computation in the pipeline — blending,
   *  `by::luma`, alpha compositing — runs on numbers it was not defined
   *  for.
   *
   *  NO conversion is performed, ever, and the declaration participates in
   *  nothing else: two renders under different declarations are
   *  byte-identical. */
  enum class InputSpace : uint8_t {
    EncodedSRGB,  ///< display-encoded sRGB — the space compose composites in
    LinearSRGB,   ///< linear-light sRGB — NOT compose's space; declaring warns
    DisplayP3,    ///< display-encoded Display P3 — NOT compose's space; warns
  };
  void declareInputSpace(InputSpace space);
  InputSpace declaredInputSpace() const;

  /** THE DESCRIBE PATH: reconciles @p root against the retained tree,
   *  matching children by key and pruning where a memo hits or the
   *  description compares equal. Call it whenever your data changed —
   *  never per frame just to move a value, which is what a binding is
   *  for. */
  void render(Element root);

  /** Updates only the named slot() mount point. Layout and stacking still
   *  integrate normally and ancestors re-record their caches, but the rest
   *  of the tree is untouched — which is the point: two data domains
   *  changing at different rates do not invalidate each other.
   *
   *  A name that matches no slot does nothing, silently. */
  void renderSlot(std::string_view name, Element content);

  /** Content or layout changed since the last draw(). Redraw when
   *  dirty() || ticker.active(). */
  bool dirty() const;

  /** Lays out if needed and paints at the canvas's current matrix/clip.
   *  Provably-static subtrees replay their auto-recorded pictures. */
  void draw(SkCanvas& canvas);

  /** Drops every per-node cache (auto pictures, Cache::Texture bakes,
   *  held live-material shaders) and marks the tree for a full repaint.
   *  GPU hosts call this on device loss or a backend switch: cached
   *  images minted by a dead context must not replay onto the next
   *  canvas. The retained tree, layout, and animations are untouched. */
  void purgeCaches();

  // ---- queries (resolved side only) ----
  /** Layout rect of a keyed node, in the composer's coordinate space.
   *  Valid after a draw() (or any other call that runs layout). */
  std::optional<SkRect> bounds(std::string_view key) const;
  /** Live SigilWeave layout of a keyed text node (valid until the next
   *  layout; for glyph choreography and queries). */
  const sigil::weave::ParagraphLayout* paragraphLayout(
      std::string_view key) const;
  /** THE SCHEDULE ONE fx() TRACK IS RUNNING: a `Beat` per beat of track
   *  @p trackIndex on the keyed text node, in draw order. Valid after a
   *  draw() (or any other call that runs layout), and computed on demand —
   *  nothing pays for it until it is asked for.
   *
   *  This is the read-back that keeps a non-glyph mark honest. Position a
   *  ball, a playhead or an underline from `rect` and `localT` here and it
   *  agrees with the glyphs by construction, whatever the cascade turns out
   *  to be — flat, nested, cue-driven, numbered over the selection or over
   *  the paragraph.
   *
   *  THIS IS THE ANSWER FOR A MOUNTED, ANIMATED RUN, where `measureRun` and
   *  `runPens` are the answer for a static one that is not in the tree: a
   *  beat's rect is read off the placement the layout produced, so it knows
   *  about wrapping, mixed styles and a path baseline, none of which a
   *  single-style measurement of one run can see.
   *
   *  An unknown key, a node that is not text, a track index past the node's
   *  list, and a track carrying no effect all resolve to an EMPTY vector,
   *  silently, exactly as an unknown key resolves everywhere else in the
   *  query family. Check your key first. */
  [[nodiscard]] std::vector<Beat> beatsOf(std::string_view key,
                                          size_t trackIndex) const;
  /** Topmost key at a canvas-space point. Valid after a draw().
   *
   *  Paint-order aware (zIndex, then declaration order, topmost first),
   *  transform-aware (rotated, scaled and translated nodes hit in their
   *  visual place), and shape-aware (custom outlines and corner radii
   *  bound the hit region, so the gap between a star's arms misses). A
   *  keyless node's hit resolves to its nearest keyed ancestor, and
   *  clipped subtrees do not hit outside their clip.
   *
   *  It answers for any keyed node whose region contains the point,
   *  whether or not that node painted anything — so a keyed transparent
   *  container will answer for its whole box. See
   *  `Element::hitTestable` for the opt-out. */
  std::optional<std::string> hitTest(SkPoint canvasPoint) const;
  /** The edge store's back-index: keys of route elements (connector()/
   *  rail()) anchored on @p nodeKey, in tree order — the graph query
   *  ("which edges touch this node") for hover highlights and pruned
   *  updates. Keyless routes are anchored but unaddressable, so they are
   *  omitted; give routes keys to see them here. Valid after render(). */
  std::vector<std::string> routesAt(std::string_view nodeKey) const;

  // ---- introspection (cost verification; see the compose_bench target) --
  struct Stats {
    size_t instances = 0;       ///< live retained nodes
    size_t yogaNodes = 0;       ///< instances carrying a Yoga node —
                                ///< positioned() subtrees carry none
    size_t describedNodes = 0;  ///< element nodes visited last render()
    size_t memoHits = 0;        ///< memo props equal → describe skipped
    size_t patchedNodes = 0;    ///< instances whose props changed
    size_t picturesLive = 0;    ///< auto-cached subtree pictures held
    size_t texturesLive = 0;    ///< Cache::Texture images held
    /** CACHE WRITES last draw() — every recording AND every pixel bake.
     *
     *  The name is narrower than the number: `Cache::Texture` bakes and
     *  library-promoted bakes count here too, so this answers "how much
     *  cache work did that frame do". `texturesBaked` breaks out the
     *  pixel-bake subset. */
    size_t picturesRecorded = 0;
    size_t texturesBaked = 0;  ///< of those, bakes rather than recordings
    size_t nodesPainted = 0;   ///< instances painted live last draw()
    // Per-phase wall time, so a slow frame localizes at a glance. The paint
    // number is where per-pixel cost lives (live materials, re-records);
    // reconcile/layout/volatile are the retained machinery.
    double reconcileMs = 0;  ///< render()/renderSlot() since previous draw()
    double layoutMs = 0;     ///< ensureLayout() inside last draw()
    double volatileMs = 0;   ///< computeVolatile() walk inside last draw()
    double paintMs = 0;      ///< paint traversal inside last draw()
  };
  const Stats& stats() const;

  /** PER-NODE PAINT COST.
   *
   *  `stats().paintMs` says how long the frame spent painting and nothing
   *  about WHERE. This localizes it.
   *
   *      composer.setProfiling(true);
   *      composer.draw(canvas);
   *      for (const auto &row : composer.profile())   // worst first
   *        printf("%7.2f ms  %s\n", row.selfMs, row.label.c_str());
   *
   *  `selfMs` EXCLUDES children, so the number lands on the node that
   *  actually costs rather than on its ancestors.
   *
   *  A CACHED NODE CAN STILL BE THE MOST EXPENSIVE THING ON THE SHEET. A
   *  picture records the DRAW CALLS, so replaying it re-runs every shader
   *  over every pixel; only a texture bake replaces that with a blit. Such
   *  a node shows up here as `cached() == true` with a large `selfMs`.
   *
   *  Off by default: the timing calls are cheap but not free. */
  /** How a node produced its pixels this frame. Picture and Texture are
   *  named separately rather than collapsed into "cached", because they
   *  cost radically different amounts — see the note above. */
  enum class CacheState : uint8_t {
    Live,      ///< painted from scratch
    Picture,   ///< replayed a recording — RE-RUNS every shader, every pixel
    Texture,   ///< blitted a raster bake — the author asked for it
    Promoted,  ///< blitted a raster bake the LIBRARY decided to make
    /** Blitted the node's OWN paint and drew its live children over it.
     *  Volatility is declared per node, so a static ground plane carrying
     *  one moving child shares the child's verdict and loses; this state
     *  says the two were separated. */
    SplitOwn,
    /** Blitted a whole-subtree bake held by `Cache::Group`'s value memo:
     *  the node AND its animated children, composited once into one
     *  unrotated device layer while every bound scalar below holds still.
     *  Distinct from Texture because the thing being asserted is
     *  different — a Texture node is provably static, a Group node is
     *  provably NOT CHANGING RIGHT NOW, and the difference is one frame. */
    Group,
  };
  /** WHY a node is, or is not, a pixel bake.
   *
   *  Promotion refusals are individually correct and individually
   *  invisible, so an expensive live-painted node is otherwise a dead end
   *  for an author. Every profiled node carries its reason here. Each
   *  refusal value names a condition under which a bake would produce
   *  DIFFERENT PIXELS, which is the one thing promotion may never do. */
  enum class Promotion : uint8_t {
    Cheap,        ///< under the cost threshold — promoting it would not pay
    Warming,      ///< expensive, counting the consecutive frames before a bake
    Promoted,     ///< baked by the library
    AskedFor,     ///< Cache::Texture — the author's own bake, not a decision
    OptedOut,     ///< Cache::Picture / Cache::None, or promotion switched off
    Volatile,     ///< its content genuinely changes every frame
    Composited,   ///< opacity < 1 or a non-srcOver blend: a bake would round
                  ///< twice
    Transformed,  ///< rotated, skewed or mirrored — a bake would resample
    Filtered,     ///< layer/backdrop effect or clip on the node itself
    /** Something in the subtree composites against what is already on the
     *  canvas — a non-srcOver blend or a backdrop filter, on this node or
     *  any descendant. A bake would resolve it against transparent black.
     *  Separated from Filtered because the remedy is different: a clip is
     *  the author's own node to change, whereas this can be a blend three
     *  levels down that they will not find without being told. */
    ReadsBackdrop,
    TooBig,  ///< beyond the per-bake area cap or the composer's bake budget
    /** The node's OWN paint is baked and its volatile children are painted
     *  live over the blit. Not a refusal — the outcome for a node whose
     *  static self was being re-rasterized every frame to redraw a moving
     *  child on top of it. */
    SplitBaked,
  };
  struct NodeCost {
    std::string label;   ///< key() if set, else kind + size — actionable
    double selfMs = 0;   ///< this node's own paint, EXCLUDING children
    double totalMs = 0;  ///< including children
    int depth = 0;
    CacheState cacheState = CacheState::Live;
    Promotion promotion = Promotion::Cheap;
    /** EVERY condition that refused a bake, not just the first one.
     *
     *  `promotion` is a first-match verdict, so a node that is both
     *  volatile and clipped reports only `Volatile`, and fixing the
     *  volatility then reveals a second refusal that was never mentioned.
     *  This mask carries all of them at once; `promotion` stays the
     *  primary outcome.
     *
     *  The bit index IS the Promotion ordinal, so there is no second table
     *  to drift out of sync with the enum. */
    uint16_t refusals = 0;
    bool refused(Promotion p) const {
      return (refusals & (uint16_t)(1u << (unsigned)p)) != 0;
    }
    bool cached() const { return cacheState != CacheState::Live; }
  };
  /** One short phrase for a Promotion, for printing next to a cost. */
  static const char* promotionReason(Promotion p);
  void setProfiling(bool on);
  bool profiling() const;
  /** Rows from the last draw(), sorted by `selfMs` descending. Empty when
   *  profiling is off. */
  const std::vector<NodeCost>& profile() const;

  /** AUTOMATIC TEXTURE PROMOTION. On by default on CPU raster; OFF by
   *  default on a Graphite/GPU surface, because the cost model driving it
   *  measures op-RECORDING time, which describes raster work and not GPU
   *  work. This setter overrides in both directions.
   *
   *  A provably-static `Cache::Auto` subtree already caches as an
   *  SkPicture — but a picture records the DRAW CALLS, so replaying it
   *  re-runs every shader over every pixel, forever. It saves the describe
   *  and the layout, not the pixels. Promotion is what turns an expensive
   *  static node into an actual blit: the composer watches how long each
   *  static node's paint costs, and once a node has been expensive for
   *  several consecutive frames it bakes that subtree ONCE into a raster
   *  image and blits it thereafter.
   *
   *  THREE KINDS OF NODE ARE ELIGIBLE:
   *
   *  1. A cached subtree whose picture replay is expensive.
   *  2. A LEAF that never records a picture at all. Bare boxes are
   *     deliberately excluded from picture recording, since one drawRect
   *     beats a nested recording — but a full-canvas box carrying one
   *     costly shader is exactly such a leaf, and it is often the single
   *     most expensive object in a frame. Leaves are measured.
   *  3. A node whose only volatility is a LIVE MATERIAL that has not
   *     actually moved since the bake. `Material::quantizeTime(hz)` steps
   *     its uniforms hz times a second, so most frames resolve to the SAME
   *     shader and the previous bake is still exact. A material bound to a
   *     continuous Output resolves to a new shader every frame, never
   *     reaches that stability, and stays live — the library measures
   *     which it is rather than assuming.
   *
   *  Re-baking is not free, so a node holds its promotion only while it is
   *  actually stable: a bake per frame would cost more than the replay it
   *  replaced.
   *
   *  IT MUST NOT CHANGE A PIXEL, and that is enforced structurally rather
   *  than hoped for: promotion is refused unless the node maps to device
   *  space with no rotation, mirroring or skew, and the bake is then taken
   *  in DEVICE space at an integer-snapped rect and blitted back with the
   *  matrix reset and no resampling. An integer device-space translation
   *  cannot alter rasterisation, so the blit is a literal copy of the
   *  pixels the live paint would have produced. Anything outside that
   *  envelope keeps painting as it did.
   *
   *  The refusals that look most like missed wins are the honest ones. A
   *  leaf at `opacity(0.13).blend(kSoftLight)` — the paper-grain idiom,
   *  and often the most expensive node in a tree — cannot be promoted:
   *  compositing a bake applies the alpha to an already-rounded 8-bit
   *  colour, while the direct draw applies it to the shader's float
   *  output, and the two agree only to within 1 LSB. Ask for that one
   *  yourself with `.cache(Cache::Texture)` — an author who types it has
   *  accepted the rounding; the library will not accept it on your behalf.
   *
   *  Why a given node was or was not promoted is reported per node as
   *  NodeCost::promotion.
   *
   *  Opting out: globally here, or per node with `.cache(Cache::Picture)`,
   *  which means "record, and never promote". `Cache::Texture` is the
   *  opposite opt-in and is unaffected. */
  void setAutoTexturePromotion(bool on);
  bool autoTexturePromotion() const;

  /** @private */
  struct Impl;

 private:
  friend struct detail::Instance;
  friend sk_sp<SkPicture> snapshot(Element, sigil::weave::FontContext&, SkSize);
  friend SkSize measure(Element, sigil::weave::FontContext&, SkSize);
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::compose
