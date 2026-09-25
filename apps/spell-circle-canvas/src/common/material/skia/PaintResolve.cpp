/** @file
 * WHICH BUILD A DRAW GETS: the frameless snapshot a consumer asks for
 * without a box, and the per-draw shader resolved against one. The two
 * doors route a paint's kind and tier onto the deferred blend fold, the
 * sksl build, the instance's cache, the fit, the pan, or the eagerly
 * resolved shader it has held since it was built — and the executor
 * entrances over them, with the crossings into Skia's words.
 */

#include <include/core/SkShader.h>
#include <sigilmaterial/skia/Color.h>

#include <utility>

#include "PaintInternal.h"

namespace sigil::material::skia {

sk_sp<SkShader> PaintAccess::asShader(const Paint& self) {
  // A BLEND has no sksl recipe of its own — it inherits liveness through
  // its layers — so the live branch below would dereference a null m_live
  // for it. This is reachable by nesting a blend inside another blend's
  // layer list, since blend() calls asShader() on every layer. Guarding the
  // null and falling through to m_shader would not be right either:
  // m_shader is the eager snapshot blend() built, which is precisely the
  // stale answer the live branch exists to avoid. Fold the layers instead,
  // per call.
  if (self.m_recipe && self.m_recipe->kind == Paint::Recipe::Kind::Blend && self.isAnimated())
    return foldBlend(self, nullptr);
  // A bound-offset image material's m_shader snapshot baked the static
  // matrix — rebuild with the pan's current values, the same
  // stale-snapshot rule as the live branch below.
  if (self.hasBoundOffset())
    if (sk_sp<SkShader> panned = pannedImageShader(self)) return panned;
  // A live material's m_shader snapshot predates its binds, so rebuild it
  // fresh and let bound Outputs contribute their CURRENT values — this is
  // what blend() flattens, and returning the snapshot would bake whatever
  // the Outputs happened to hold at construction. The m_live guard is
  // explicit rather than implied by isAnimated(): a bound pan (handled just
  // above) reports animated with no sksl recipe behind it.
  if (self.m_live && self.isAnimated()) return build(*self.m_live, nullptr);
  if (self.m_backed && self.isAnimated()) return buildBacked(self, nullptr);
  if (snapshot(self)) return snapshot(self);
  if (self.m_isSolid) return SkShaders::Color(toSkColor(self.m_solid), nullptr);
  return nullptr;  // none
}

sk_sp<SkShader> PaintAccess::shaderFor(const Paint& self, const PaintFrame& paintFrame) {
  // Deferred blend: when any layer needs the PaintFrame (live uniforms,
  // SDF uResolution), the flatten happens HERE, per resolve, so every layer
  // contributes its correct current form — the eager snapshot from blend()
  // would have baked those layers with a null context (uResolution = 0,0).
  // World-space is layer-local by design: a flagged OUTER blend anchors the
  // whole fold here, while a flagged LAYER already anchored itself on the
  // way through childShader → resolve.
  if (self.m_recipe && self.m_recipe->kind == Paint::Recipe::Kind::Blend &&
      (self.isAnimated() || self.geometryDependent())) {
    sk_sp<SkShader> folded = foldBlend(self, &paintFrame);
    if (self.m_worldSpace) folded = anchorToRoot(std::move(folded), paintFrame);
    return folded;
  }
  // The sksl path — build() digests W and applies the world-space wrap
  // inside its memo. Guarded on m_live because a world-space flag makes
  // gradient-factory materials geometry-dependent too, and those have no
  // sksl recipe; they take the final branch below instead.
  if (self.m_live && (self.isAnimated() || self.geometryDependent()))
    return build(*self.m_live, &paintFrame, self.m_worldSpace);
  // The recipe-backed path — the same rule, through the core's cache.
  if (self.m_backed && (self.isAnimated() || self.geometryDependent()))
    return buildBacked(self, &paintFrame);
  // The fit: the source mapped onto THIS box, which neither the static
  // snapshot below nor the recipe matrix could know. It carries the bound
  // pan itself, so it sits above the pan branch as well.
  if (self.hasFit()) {
    if (sk_sp<SkShader> fitted = fittedImageShader(self, paintFrame)) {
      if (self.m_worldSpace) fitted = anchorToRoot(std::move(fitted), paintFrame);
      return fitted;
    }
  }
  // The bound pan: the recipe matrix translated by the bindings' current
  // values, per draw. It has to sit above the static snapshot below,
  // which baked the UNpanned matrix. The paint layer's scalar memo,
  // which carries the resolved pan among its content scalars, is what keeps
  // the node's recording alive between moves.
  if (self.hasBoundOffset()) {
    if (sk_sp<SkShader> panned = pannedImageShader(self)) {
      if (self.m_worldSpace) panned = anchorToRoot(std::move(panned), paintFrame);
      return panned;
    }
  }
  // World-space anchoring for everything that ISN'T an sksl recipe: the
  // gradient factories, image()/buffer(), raw shader() wraps. A gradient
  // declared in canvas coordinates reaches root space through this line.
  // The wrapper is minted per resolve, which costs nothing here: these are
  // geometry-tier materials, resolved when the node records, so no
  // per-frame pointer stability is at stake.
  sk_sp<SkShader> s = snapshot(self);
  if (self.m_worldSpace && s) s = anchorToRoot(std::move(s), paintFrame);
  return s;
}

sk_sp<SkShader> shader(const Paint& paint) {
  return PaintAccess::asShader(paint);
}

sk_sp<SkShader> shader(const Paint& paint, const FrameData& frame) {
  return PaintAccess::shaderFor(paint, paintFrameOf(frame));
}

sk_sp<SkShader> staticShader(const Paint& paint) {
  return PaintAccess::snapshot(paint);
}

sk_sp<SkShader> resolvePass(const Paint& paint, const PassInputs& in,
                            const FrameData& frame) {
  return PaintAccess::resolvePass(paint, in, paintFrameOf(frame));
}

SkMatrix toSkMatrix(const glm::mat3& m) {
  // glm is column-major: m[column][row].
  return SkMatrix::MakeAll(m[0][0], m[1][0], m[2][0], m[0][1], m[1][1],
                           m[2][1], m[0][2], m[1][2], m[2][2]);
}

glm::mat3 toMatrix(const SkMatrix& m) {
  glm::mat3 out(1.0f);
  out[0][0] = m.getScaleX();
  out[1][0] = m.getSkewX();
  out[2][0] = m.getTranslateX();
  out[0][1] = m.getSkewY();
  out[1][1] = m.getScaleY();
  out[2][1] = m.getTranslateY();
  out[0][2] = m.getPerspX();
  out[1][2] = m.getPerspY();
  out[2][2] = m.get(SkMatrix::kMPersp2);
  return out;
}

PaintFrame paintFrameOf(const FrameData& frame) {
  PaintFrame out;
  out.size = SkSize::Make(frame.resolution.x, frame.resolution.y);
  out.rootSize = SkSize::Make(frame.rootResolution.x, frame.rootResolution.y);
  out.toRoot = toSkMatrix(frame.world);
  out.seconds = frame.seconds;
  out.contentScale = frame.contentScale;
  return out;
}

SkBlendMode toSkBlendMode(BlendMode mode) {
  switch (mode) {
    case BlendMode::Normal: return SkBlendMode::kSrcOver;
    case BlendMode::Multiply: return SkBlendMode::kMultiply;
    case BlendMode::Screen: return SkBlendMode::kScreen;
    case BlendMode::Overlay: return SkBlendMode::kOverlay;
    case BlendMode::Darken: return SkBlendMode::kDarken;
    case BlendMode::Lighten: return SkBlendMode::kLighten;
    case BlendMode::ColorDodge: return SkBlendMode::kColorDodge;
    case BlendMode::ColorBurn: return SkBlendMode::kColorBurn;
    case BlendMode::HardLight: return SkBlendMode::kHardLight;
    case BlendMode::SoftLight: return SkBlendMode::kSoftLight;
    case BlendMode::Difference: return SkBlendMode::kDifference;
    case BlendMode::Exclusion: return SkBlendMode::kExclusion;
    case BlendMode::Hue: return SkBlendMode::kHue;
    case BlendMode::Saturation: return SkBlendMode::kSaturation;
    case BlendMode::Color: return SkBlendMode::kColor;
    case BlendMode::Luminosity: return SkBlendMode::kLuminosity;
    case BlendMode::PlusLighter: return SkBlendMode::kPlus;
    case BlendMode::Modulate: return SkBlendMode::kModulate;
    case BlendMode::Clear: return SkBlendMode::kClear;
    case BlendMode::Source: return SkBlendMode::kSrc;
    case BlendMode::Destination: return SkBlendMode::kDst;
    case BlendMode::SourceIn: return SkBlendMode::kSrcIn;
    case BlendMode::SourceOut: return SkBlendMode::kSrcOut;
    case BlendMode::SourceAtop: return SkBlendMode::kSrcATop;
    case BlendMode::DestinationOver: return SkBlendMode::kDstOver;
    case BlendMode::DestinationIn: return SkBlendMode::kDstIn;
    case BlendMode::DestinationOut: return SkBlendMode::kDstOut;
    case BlendMode::DestinationAtop: return SkBlendMode::kDstATop;
    case BlendMode::Xor: return SkBlendMode::kXor;
  }
  return SkBlendMode::kSrcOver;
}

BlendMode toBlendMode(SkBlendMode mode) {
  switch (mode) {
    case SkBlendMode::kSrcOver: return BlendMode::Normal;
    case SkBlendMode::kMultiply: return BlendMode::Multiply;
    case SkBlendMode::kScreen: return BlendMode::Screen;
    case SkBlendMode::kOverlay: return BlendMode::Overlay;
    case SkBlendMode::kDarken: return BlendMode::Darken;
    case SkBlendMode::kLighten: return BlendMode::Lighten;
    case SkBlendMode::kColorDodge: return BlendMode::ColorDodge;
    case SkBlendMode::kColorBurn: return BlendMode::ColorBurn;
    case SkBlendMode::kHardLight: return BlendMode::HardLight;
    case SkBlendMode::kSoftLight: return BlendMode::SoftLight;
    case SkBlendMode::kDifference: return BlendMode::Difference;
    case SkBlendMode::kExclusion: return BlendMode::Exclusion;
    case SkBlendMode::kHue: return BlendMode::Hue;
    case SkBlendMode::kSaturation: return BlendMode::Saturation;
    case SkBlendMode::kColor: return BlendMode::Color;
    case SkBlendMode::kLuminosity: return BlendMode::Luminosity;
    case SkBlendMode::kPlus: return BlendMode::PlusLighter;
    case SkBlendMode::kModulate: return BlendMode::Modulate;
    case SkBlendMode::kClear: return BlendMode::Clear;
    case SkBlendMode::kSrc: return BlendMode::Source;
    case SkBlendMode::kDst: return BlendMode::Destination;
    case SkBlendMode::kSrcIn: return BlendMode::SourceIn;
    case SkBlendMode::kSrcOut: return BlendMode::SourceOut;
    case SkBlendMode::kSrcATop: return BlendMode::SourceAtop;
    case SkBlendMode::kDstOver: return BlendMode::DestinationOver;
    case SkBlendMode::kDstIn: return BlendMode::DestinationIn;
    case SkBlendMode::kDstOut: return BlendMode::DestinationOut;
    case SkBlendMode::kDstATop: return BlendMode::DestinationAtop;
    case SkBlendMode::kXor: return BlendMode::Xor;
  }
  return BlendMode::Normal;
}

SkTileMode toSkTileMode(Repeat repeat) {
  switch (repeat) {
    case Repeat::Pad:
      return SkTileMode::kClamp;
    case Repeat::Repeat:
      return SkTileMode::kRepeat;
    case Repeat::Mirror:
      return SkTileMode::kMirror;
    case Repeat::None:
      return SkTileMode::kDecal;
  }
  return SkTileMode::kClamp;
}

}  // namespace sigil::material::skia
