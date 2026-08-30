#pragma once

/** @file
 * SigilCompose paint values — Fill, Corners, the PaintContext a paint
 * program is handed, the instance-side StampCache, the caller-owned
 * UniformBlock, and Effect, the post-processing seam. These are the
 * comparable values the paint stage reads; the polymorphic Material that
 * supersedes Fill as fill()'s authoring value is declared in
 * <sigilcompose/Material.h>.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>
#include <include/core/SkPicture.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>
#include <include/core/SkSize.h>
#include <include/core/SkTypes.h>
#include <sigilcompose/core/Motion.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

class SkCanvas;
class SkImageFilter;
class SkRuntimeEffect;

namespace sigil::weave {
class FontContext;
}

namespace sigil::compose {

namespace detail {
struct ElementNode;
}  // namespace detail

// The polymorphic paint value (<sigilcompose/Material.h>) — supersedes Fill as
// the authoring value for fill(); a static Material collapses to a Fill.
class Material;

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

// Colour — source palettes arrive as lists of hex integers.

/** `0xRRGGBB` (+ alpha) as an SkColor4f, sRGB byte values divided by 255.
 *
 *  Named `hex` rather than `rgb`, because `rgb(0xRRGGBB)` reads as "three
 *  arguments" when there is only one, and rather than a single letter,
 *  which is unsearchable. constexpr, so palette constants stay constexpr. */
constexpr SkColor4f hex(uint32_t rrggbb, float a = 1.0f) {
  return {(float)((rrggbb >> 16) & 0xffu) / 255.0f,
          (float)((rrggbb >> 8) & 0xffu) / 255.0f,
          (float)(rrggbb & 0xffu) / 255.0f, a};
}

/** The same colour at a different alpha — `{c.fR, c.fG, c.fB, a}`.
 *
 *  Kept separate from mul() deliberately: replacing alpha and scaling the
 *  colour channels are different operations, and folding both into one
 *  signature would leave a defaulted argument deciding which the caller
 *  meant. */
constexpr SkColor4f alpha(SkColor4f c, float a) {
  return {c.fR, c.fG, c.fB, a};
}

/** The brightness ladder: scale RGB by @p k, optionally replacing alpha
 *  (`a < 0` keeps it) — a tone ramp off one sampled base colour.
 *
 *  Deliberately does NOT clamp. SkColor4f is float and a channel above 1
 *  is legal (and meaningful under a wide-gamut or OCIO view); Skia clamps
 *  when it lands in an 8-bit surface. A caller who needs the clamped value
 *  is asking for a different operation and writes it at the call site. */
constexpr SkColor4f mul(SkColor4f c, float k, float a = -1.0f) {
  return {c.fR * k, c.fG * k, c.fB * k, a < 0 ? c.fA : a};
}

/** Linear interpolation between two colours, alpha included. Component-wise
 *  in whatever space the colours are already in — plain arithmetic, not a
 *  colour-managed blend. */
constexpr SkColor4f mix(SkColor4f a, SkColor4f b, float t) {
  return {a.fR + (b.fR - a.fR) * t, a.fG + (b.fG - a.fG) * t,
          a.fB + (b.fB - a.fB) * t, a.fA + (b.fA - a.fA) * t};
}

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
  /** One bake. A consumer stores either a recorded picture or a
   *  rastered image with the logical size it was baked at, and reads
   *  back only the kind it wrote. */
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
   *  hand-written in Effects.cpp and reads these members directly; the state
   *  is private, so the decomposition lives inside the class. */
  static void fieldPin(Effect& v) {
    auto& [filter, effect, uniforms, uniforms2, uniforms4, uniformArrays, bound,
           blocks, dirBlur, paramBlur, children, chainA, chainB] = v;
    static_assert(std::tuple_size_v<decltype(std::tie(
                          filter, effect, uniforms, uniforms2, uniforms4,
                          uniformArrays, bound, blocks, dirBlur, paramBlur,
                          children, chainA, chainB))> == 13,
                  "Effect gained or lost a member — rule on it in "
                  "Effect::operator== (Effects.cpp), then bump this count. "
                  "(m_filter is EXCLUDED on the shader, directionalBlur and "
                  "blur paths because it is derived from m_effect + the "
                  "constant lanes / m_dirBlur / m_paramBlur + m_children; "
                  "m_bound and m_blocks make the effect isAnimated(), which "
                  "operator== already refuses; m_chainA/B only exist on a "
                  "live chain, ditto.)");
  }
};

}  // namespace sigil::compose
