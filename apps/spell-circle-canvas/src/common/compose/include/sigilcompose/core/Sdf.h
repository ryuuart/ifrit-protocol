#pragma once

/** @file
 * SigilCompose SDF materials — shape, border, glow and soft shadow in ONE
 * shader pass, SigilMaterial's signed-distance surfaces as compose
 * materials: `sdf::material(shape, style)` returns an ordinary Material
 * for `.fill()`, so it caches, prunes and animates by the library's
 * standard rules:
 *
 *  - the recipe reads the resolution, so the material is
 *    GEOMETRY-DEPENDENT. It resolves when its node records, stays
 *    picture-cached between layouts, and re-records on a size change;
 *  - every style parameter is a named uniform, so binding one to a
 *    ch::Output (`.uniform("uBorderW", &width)`, `"uGlowR"`, `"uP0"`…)
 *    makes the material live. A pulsing border or a breathing glow is a
 *    bound scalar, not a repaint loop;
 *  - ONE compiled program per shape KIND — the parameters are uniforms.
 *
 * The SDF paints the node's LOOK only. Hit-testing and clipping still use
 * the node's own geometry, so pair it with `.shape(shapes::star(...))`
 * when hits and clips should match the silhouette.
 */

#include <sigilcompose/core/Material.h>
#include <sigilmaterial/sdf/Sdf.h>

namespace sigil::compose::sdf {

using sigil::material::sdf::circle;
using sigil::material::sdf::Kind;
using sigil::material::sdf::roundBox;
using sigil::material::sdf::Shape;
using sigil::material::sdf::star;

/** How the silhouette is dressed — every field is a shader uniform.
 *  Layer order (back to front): shadow, glow, fill, border. */
struct Style {
  SkColor4f fill = {0, 0, 0, 0};
  float borderWidth = 0;                 // centered on the edge (uBorderW)
  SkColor4f borderColor = {1, 1, 1, 1};  // (uBorder)
  float glowRadius = 0;                  // exp falloff, px (uGlowR)
  SkColor4f glowColor = {1, 1, 1, 1};    // (uGlow)
  SkVector shadowOffset = {0, 0};        // (uShadowOffX/Y)
  float shadowBlur = 0;                  // (uShadowBlur)
  SkColor4f shadowColor = {0, 0, 0, 0};  // alpha 0 = no shadow (uShadow)

  /** The same style in SigilMaterial's words. */
  sigil::material::sdf::Style toMaterial() const {
    const auto c = [](SkColor4f k) {
      return sigil::material::Color{k.fR, k.fG, k.fB, k.fA};
    };
    sigil::material::sdf::Style out;
    out.fill = c(fill);
    out.borderWidth = borderWidth;
    out.borderColor = c(borderColor);
    out.glowRadius = glowRadius;
    out.glowColor = c(glowColor);
    out.shadowOffset = {shadowOffset.fX, shadowOffset.fY};
    out.shadowBlur = shadowBlur;
    out.shadowColor = c(shadowColor);
    return out;
  }
};

/** The pad the style reserves inside the node's box: border half-width
 *  plus glow and shadow reach. Public so callers can size a node by its
 *  VISIBLE silhouette — box dimension = visible diameter + 2·sdf::pad(
 *  style) — rather than restating the formula.
 *
 *  PAD IS LAYOUT RESERVE, NOT APPEARANCE. It never changes the rendered
 *  glow's falloff; a larger pad only guarantees the box does not crop it.
 *  Shrinking the pad does not tighten a glow, and growing the box without
 *  growing the content shrinks the visible interior to nothing — see
 *  `minBoxFor()`. */
inline float pad(const Style& style) {
  return sigil::material::sdf::pad(style.toMaterial());
}

/** The box dimension that leaves @p contentPx of VISIBLE interior after
 *  the style's pad on both sides. Size nodes with this: a box that is not
 *  big enough for the pad has no visible interior at all and the node
 *  renders as nothing but its outer treatments. */
inline float minBoxFor(const Style& style, float contentPx) {
  return contentPx + 2.0f * pad(style);
}

/** The SDF material: shape plus style, one shader pass. The style's outer
 *  treatments reserve a pad inside the node's box so nothing clips;
 *  bindable uniforms animate within that reserve, so bind a glow up to the
 *  style's glowRadius and not past it — a bound value that exceeds the
 *  pad clips. */
inline Material material(const Shape& shape, const Style& style) {
  return Material::recipe(
      sigil::material::sdf::material(shape, style.toMaterial()));
}

}  // namespace sigil::compose::sdf
