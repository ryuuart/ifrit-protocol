/** @file
 * WHICH BUILD A DRAW GETS: the frameless snapshot a consumer asks for
 * without a box, and the per-draw shader resolved against one. The two
 * doors route a paint's kind and tier onto the deferred blend fold, the
 * sksl build, the instance's cache, the fit, the pan, or the eagerly
 * resolved shader it has held since it was built.
 */

#include <include/core/SkShader.h>

#include <utility>

#include "PaintInternal.h"

namespace sigil::material::skia {

sk_sp<SkShader> Paint::asShader() const {
  // A BLEND has no sksl recipe of its own — it inherits liveness through
  // its layers — so the live branch below would dereference a null m_live
  // for it. This is reachable by nesting a blend inside another blend's
  // layer list, since blend() calls asShader() on every layer. Guarding the
  // null and falling through to m_shader would not be right either:
  // m_shader is the eager snapshot blend() built, which is precisely the
  // stale answer the live branch exists to avoid. Fold the layers instead,
  // per call.
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend && isAnimated())
    return foldBlend(nullptr);
  // A bound-offset image material's m_shader snapshot baked the static
  // matrix — rebuild with the pan's current values, the same
  // stale-snapshot rule as the live branch below.
  if (hasBoundOffset())
    if (sk_sp<SkShader> panned = pannedImageShader()) return panned;
  // A live material's m_shader snapshot predates its binds, so rebuild it
  // fresh and let bound Outputs contribute their CURRENT values — this is
  // what blend() flattens, and returning the snapshot would bake whatever
  // the Outputs happened to hold at construction. The m_live guard is
  // explicit rather than implied by isAnimated(): a bound pan (handled just
  // above) reports animated with no sksl recipe behind it.
  if (m_live && isAnimated()) return build(*m_live, nullptr);
  if (m_backed && isAnimated()) return buildBacked(nullptr);
  if (m_shader) return m_shader;
  if (m_isSolid) return SkShaders::Color(m_solid, nullptr);
  return nullptr;  // none
}

sk_sp<SkShader> Paint::shaderFor(const PaintFrame& ctx) const {
  // Deferred blend: when any layer needs the PaintFrame (live uniforms,
  // SDF uResolution), the flatten happens HERE, per resolve, so every layer
  // contributes its correct current form — the eager snapshot from blend()
  // would have baked those layers with a null context (uResolution = 0,0).
  // World-space is layer-local by design: a flagged OUTER blend anchors the
  // whole fold here, while a flagged LAYER already anchored itself on the
  // way through childShader → resolve.
  if (m_recipe && m_recipe->kind == Recipe::Kind::Blend &&
      (isAnimated() || geometryDependent())) {
    sk_sp<SkShader> folded = foldBlend(&ctx);
    if (m_worldSpace) folded = anchorToRoot(std::move(folded), ctx);
    return folded;
  }
  // The sksl path — build() digests W and applies the world-space wrap
  // inside its memo. Guarded on m_live because a world-space flag makes
  // gradient-factory materials geometry-dependent too, and those have no
  // sksl recipe; they take the final branch below instead.
  if (m_live && (isAnimated() || geometryDependent()))
    return build(*m_live, &ctx, m_worldSpace);
  // The recipe-backed path — the same rule, through the core's cache.
  if (m_backed && (isAnimated() || geometryDependent()))
    return buildBacked(&ctx);
  // The fit: the source mapped onto THIS box, which neither the static
  // snapshot below nor the recipe matrix could know. It carries the bound
  // pan itself, so it sits above the pan branch as well.
  if (hasFit()) {
    if (sk_sp<SkShader> fitted = fittedImageShader(ctx)) {
      if (m_worldSpace) fitted = anchorToRoot(std::move(fitted), ctx);
      return fitted;
    }
  }
  // The bound pan: the recipe matrix translated by the bindings' current
  // values, per draw. It has to sit above the static snapshot below,
  // which baked the UNpanned matrix. The paint layer's scalar memo,
  // which carries the resolved pan among its content scalars, is what keeps
  // the node's recording alive between moves.
  if (hasBoundOffset()) {
    if (sk_sp<SkShader> panned = pannedImageShader()) {
      if (m_worldSpace) panned = anchorToRoot(std::move(panned), ctx);
      return panned;
    }
  }
  // World-space anchoring for everything that ISN'T an sksl recipe: the
  // gradient factories, image()/buffer(), raw shader() wraps. A gradient
  // declared in canvas coordinates reaches root space through this line.
  // The wrapper is minted per resolve, which costs nothing here: these are
  // geometry-tier materials, resolved when the node records, so no
  // per-frame pointer stability is at stake.
  sk_sp<SkShader> s = m_shader;
  if (m_worldSpace && s) s = anchorToRoot(std::move(s), ctx);
  return s;
}

}  // namespace sigil::material::skia
