#pragma once

/** @file
 * Signed-distance surfaces — shape, border, glow and soft shadow in ONE
 * shader pass over Inigo Quilez's 2D distance operators. One recipe per
 * silhouette kind; every style parameter is a uniform, so a pulsing border
 * or a breathing glow is a bound scalar and however many differently
 * styled instances there are, three programs compile and no more.
 *
 * Distances are computed in aspect-corrected PIXEL space over the node's
 * resolution, never uv, so borders stay even on a stretched box. The
 * anti-alias half-width is 0.75 px in the material's own space.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <memory>

namespace sigil::material::sdf {

/** Which silhouette a Shape stands for. One recipe exists per kind — a
 *  shape does not change kind, it is rebuilt through another factory. */
enum class Kind : uint8_t { RoundBox, Circle, Star };

struct Shape;
struct Style;
inline Shape roundBox(float radius);
inline Shape circle();
Shape star(int points, float pointiness);
Material material(const Shape& shape, const Style& style);

/** A silhouette, sized by the box minus the style's reserved pad. Built
 *  only through the factories: the per-kind parameters are the body's
 *  uP0..uP2, they mean something different in each kind, and they are
 *  valid only as the factory packs them — star's `pointiness` is clamped
 *  into [2, points], so a raw triple is not a shape. */
struct Shape {
 private:
  Kind kind = Kind::RoundBox;
  float p0 = 0, p1 = 0, p2 = 0;

  friend Shape roundBox(float radius);
  friend Shape circle();
  friend Shape star(int points, float pointiness);
  friend Material material(const Shape& shape, const Style& style);

 public:
  Kind kindOf() const { return kind; }
  bool operator==(const Shape&) const = default;
};

/** Rounded box inscribed in the box (radius in px, clamped to half-size). */
inline Shape roundBox(float radius) {
  Shape s;
  s.kind = Kind::RoundBox;
  s.p0 = radius;
  return s;
}

/** Circle inscribed in the box. */
inline Shape circle() {
  Shape s;
  s.kind = Kind::Circle;
  return s;
}

/** N-pointed star. `pointiness` is m in [2, points]: m = points is the
 *  regular polygon, and values toward 2 sharpen the arms. */
Shape star(int points, float pointiness);

/** How the silhouette is dressed. Layer order (back to front): shadow,
 *  glow, fill, border. */
struct Style {
  Color fill = {0, 0, 0, 0};
  float borderWidth = 0;             ///< centred on the edge (uBorderW)
  Color borderColor = {1, 1, 1, 1};  ///< uBorder
  float glowRadius = 0;              ///< exponential falloff, px (uGlowR)
  Color glowColor = {1, 1, 1, 1};    ///< uGlow
  glm::vec2 shadowOffset = {0, 0};   ///< uShadowOffX / uShadowOffY
  float shadowBlur = 0;              ///< uShadowBlur
  Color shadowColor = {0, 0, 0, 0};  ///< alpha 0 = no shadow (uShadow)

  bool operator==(const Style&) const = default;
};

/** The params struct — the recipe's ABI. Every field is a uniform of the
 *  body by this name. */
struct SdfParams {
  float uPad;
  Color uFill;
  float uBorderW;
  Color uBorder;
  float uGlowR;
  Color uGlow;
  float uShadowOffX;
  float uShadowOffY;
  float uShadowBlur;
  Color uShadow;
  float uP0;
  float uP1;
  float uP2;
};

/** The pad the style reserves inside the box: border half-width plus glow
 *  and shadow reach. Public so a caller can size a box by its VISIBLE
 *  silhouette — box = visible diameter + 2·pad(style) — rather than
 *  restating the formula.
 *
 *  PAD IS LAYOUT RESERVE, NOT APPEARANCE. It never changes the rendered
 *  glow's falloff, which is governed by exp(-d/glowRadius); a larger pad
 *  only guarantees the box does not crop it. */
float pad(const Style& style);

/** The box dimension that leaves @p contentPx of VISIBLE interior after
 *  the style's pad on both sides. */
inline float minBoxFor(const Style& style, float contentPx) {
  return contentPx + 2.0f * pad(style);
}

/** The recipe for @p kind, defined once: the SDF prelude, the kind's
 *  distance function, and the one-pass layering. Reads the resolution. */
const std::shared_ptr<const Recipe>& recipe(Kind kind);

/** The material: @p shape dressed by @p style. Bind `uGlowR`, `uBorderW`
 *  and the rest to animate within the reserve the style computed. */
Material material(const Shape& shape, const Style& style);

}  // namespace sigil::material::sdf
