/** @file
 * THE PAINT VALUE: what a paint answers about itself once it is built —
 * the recipe properties an author states (the layer strength, the cull
 * reserve, the pan, the fit, the root anchoring, the time quantization),
 * the volatility tier those properties put it in, and the structural
 * equality that decides whether a re-describe prunes.
 */

#include <include/core/SkTypes.h>  // SkDebugf

#include <algorithm>
#include <string>
#include <utility>

#include "PaintInternal.h"

namespace sigil::material::skia {

const sigil::material::Material* Paint::recipeMaterial() const {
  return m_backed ? &m_backed->material : nullptr;
}

void Paint::detachBacked() {
  if (m_backed && m_backed.use_count() > 1)
    m_backed = std::make_shared<Backed>(*m_backed);
}

bool Paint::operator==(const Paint& o) const {
  if (m_amount != o.m_amount)
    return false;  // a layer strength is recipe, like a stop or a mode
  if (m_bleed != o.m_bleed)
    return false;  // a cull reserve is recipe too — a change must re-record
  if (m_boundOffset != o.m_boundOffset)
    return false;  // the pan BINDING is recipe, compared by pointer like any
                   // other binding; the values it resolves to are the paint
                   // layer's scalar memo to track, never the prune's
  if (m_worldSpace != o.m_worldSpace)
    return false;  // the FLAG is recipe — it says which frame the author
                   // meant. W itself is layout-derived and never compares
                   // here; it is invalidated the way uResolution is
  if (m_isSolid != o.m_isSolid) return false;
  if (m_isSolid) return m_solid == o.m_solid;
  // Recipe-backed: SigilMaterial's own equality — recipe identity, bytes,
  // bindings by identity, children by value.
  if ((m_backed != nullptr) != (o.m_backed != nullptr)) return false;
  if (m_backed) return m_backed->material == o.m_backed->material;
  // sksl-backed: static recipes compare structurally (effect pointer +
  // constant values); live ones by identity — conservative, they never prune.
  if ((m_live != nullptr) != (o.m_live != nullptr)) return false;
  if (m_live) {
    if (isAnimated() || o.isAnimated()) return m_live == o.m_live;
    // Children are recipe, recursively: two materials over one effect that
    // sample DIFFERENT palettes are different materials, and a node that
    // pruned across that swap would sample the old one forever.
    return m_live->effect == o.m_live->effect &&
           m_live->constants == o.m_live->constants &&
           m_live->constants2 == o.m_live->constants2 &&
           m_live->constants4 == o.m_live->constants4 &&
           m_live->constantArrays == o.m_live->constantArrays &&
           m_live->children == o.m_live->children;
  }
  if ((m_recipe != nullptr) != (o.m_recipe != nullptr)) return false;
  if (m_recipe) return *m_recipe == *o.m_recipe;
  return m_shader == o.m_shader;  // raw shader wrap / none
}

// uniform() and child() mutations copy-on-write the recipe, because Paint
// is a VALUE: copies of one base material must never alias each other's
// uniforms. Two elements built from a shared sksl base and then bound to
// different Outputs are the ordinary case, and without this the second bind
// would overwrite the first through the shared block and both would be wrong
// with nothing to indicate it.
void Paint::detachLive() {
  if (m_live && m_live.use_count() > 1)
    m_live = std::make_shared<Live>(*m_live);
}

Paint& Paint::amount(float a01) {
  m_amount = std::clamp(a01, 0.0f, 1.0f);
  return *this;
}

Paint& Paint::offset(std::optional<motion::Animatable<float>> x,
                     std::optional<motion::Animatable<float>> y) {
  // Only the image-backed kinds carry a local matrix for the pan to
  // translate. Everything else warns and ignores, following the same rule
  // uniform() uses: never abort (one typo must not kill a live-coding host),
  // and never silently no-op (an ignored pan on a gradient looks like a
  // broken animation).
  const bool pannable = m_recipe && (m_recipe->kind == Recipe::Kind::Image ||
                                     m_recipe->kind == Recipe::Kind::Buffer);
  if (!pannable) {
    SkDebugf(
        "skia::Paint::offset(&x, &y): ignored — only image()/buffer() "
        "materials carry a local matrix to pan (Pattern's backend)\n");
    return *this;
  }
  m_boundOffset = {std::move(x), std::move(y)};
  return *this;
}

SkPoint Paint::boundOffsetValue() const {
  return {m_boundOffset[0] ? motion::resolveFloatAt(nullptr, *m_boundOffset[0])
                           : 0.0f,
          m_boundOffset[1] ? motion::resolveFloatAt(nullptr, *m_boundOffset[1])
                           : 0.0f};
}

bool Paint::hasFit() const { return m_recipe && m_recipe->fit != Fit::Native; }

Paint& Paint::fit(Fit how) {
  if (!m_recipe || (m_recipe->kind != Recipe::Kind::Image &&
                    m_recipe->kind != Recipe::Kind::Buffer)) {
    SkDebugf(
        "skia::Paint::fit(): ignored — only image()/buffer() materials have "
        "a source with a size of its own to fit\n");
    return *this;
  }
  // The recipe is held const and shared by every copy of the material, so
  // a fit is stated by replacing it — the same posture the factories take.
  auto rec = std::make_shared<Recipe>(*m_recipe);
  rec->fit = how;
  m_recipe = std::move(rec);
  return *this;
}

Paint& Paint::worldSpace(bool on) {
  m_worldSpace = on;  // recipe, like amount()/bleed(): joins operator==
  return *this;
}

bool Paint::usesWorldSpace() const {
  if (m_worldSpace) return true;
  // The FLAG is layer-local (never inherited), but the reconcile walk
  // needs to see a flagged layer anywhere below: a blend whose second
  // layer anchors still needs its node W-invalidated.
  if (m_live)
    for (const auto& [name, child] : m_live->children)
      if (child.usesWorldSpace()) return true;
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto& layer : m_recipe->layers)
      if (layer.first.usesWorldSpace()) return true;
  return false;
}

Paint& Paint::bleed(float px) {
  m_bleed = px;  // read by the recording cull (max-accumulated, so only a
  return *this;  // positive reserve ever grows anything)
}

Paint& Paint::quantizeTime(float hz) {
  if (m_backed) {
    detachBacked();
    m_backed->material.quantizeTime(hz);
    return *this;
  }
  if (!m_live) {
    SkDebugf(
        "skia::Paint::quantizeTime: ignored — only sksl() materials carry "
        "uTime\n");
    return *this;
  }
  if (!validUniform(m_live->effect, "uTime", sizeof(float))) {
    SkDebugf(
        "skia::Paint::quantizeTime: ignored — the effect does not declare "
        "`uniform float uTime`\n");
    return *this;
  }
  detachLive();
  m_live->timeQuantizeHz = hz > 0 ? hz : 0;
  return *this;
}

bool Paint::isAnimated() const {
  // A bound pan IS animation: the material re-resolves per frame while the
  // pan moves, so it has to occupy the live slot and every consumer — the
  // deferred blend flatten, child-slot liveness, decoration volatility —
  // must see it as such.
  //
  // HOW its node caches is a separate question, split out below: a pan-only
  // material qualifies for the cheaper scalar-comparison lane rather than
  // the live-material memo. See animatedBeyondBoundOffset().
  if (boundOffsetLive()) return true;
  return animatedBeyondBoundOffset();
}

bool Paint::animatedBeyondBoundOffset() const {
  if (m_backed && m_backed->material.isAnimated()) return true;
  // A bound UniformBlock is a bind whose value is a table: the material
  // re-resolves per frame (the resolve memo reads the revision), and its
  // node is declared volatile so a cache cannot freeze the array.
  if (m_live &&
      (!m_live->blocks.empty() || m_live->usesTime || m_live->usesScale))
    return true;
  // A scalar bind counts only while it is LIVE: one holding a plain
  // number is a value written into the uniform, and a node does not
  // repaint forever for a constant.
  if (m_live)
    for (const auto& [name, out] : m_live->binds)
      if (motion::isLive(nullptr, out)) return true;
  // A child slot's volatility is the parent's: the parent samples it, so a
  // live child that did not lift the parent to the live path would be
  // resolved once and frozen into the parent's cache. A NESTED bound
  // offset deliberately counts here (child.isAnimated(), not the
  // subtraction): the node-level scalar lane resolves only the TOP
  // material's own pan, so anything deeper stays conservatively opaque.
  if (m_live)
    for (const auto& [name, child] : m_live->children)
      if (child.isAnimated()) return true;
  // A blend inherits liveness from its layers (deferred fold in resolve()).
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto& layer : m_recipe->layers)
      if (layer.first.isAnimated()) return true;
  return false;
}

bool Paint::geometryDependent() const {
  // W is layout-derived exactly as uResolution is: a world-space material
  // needs the PaintFrame (for its node's toRoot) at resolve time, it
  // resolves when the node records, and it re-records when layout moves the
  // node. This one `true` is what routes EVERY flagged material — including
  // the gradient factories, which have no sksl recipe at all — through the
  // context-carrying paths: Element::fill's live slot, the coverage gate's
  // resolve, childShader's per-frame form, and build()'s memo skip.
  if (m_worldSpace) return true;
  // A fit is a question about the BOX, so it is answered where uResolution
  // is answered: when the node records, and again when layout moves it.
  if (hasFit()) return true;
  if (m_backed && m_backed->material.geometryDependent()) return true;
  if (m_live && m_live->usesGeometry) return true;
  if (m_live)
    for (const auto& [name, child] : m_live->children)
      if (child.geometryDependent()) return true;
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend)
    for (const auto& layer : m_recipe->layers)
      if (layer.first.geometryDependent()) return true;
  return false;
}

}  // namespace sigil::material::skia
