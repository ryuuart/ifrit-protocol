#pragma once

/** @file
 * SigilCompose SDF materials — shape, border, glow and soft shadow in ONE
 * shader pass, over Inigo Quilez's 2D signed-distance operators. An
 * extension over the public Material API: `sdf::material(shape, style)`
 * returns an ordinary Material for `.fill()`, so it caches, prunes and
 * animates by the library's standard rules:
 *
 *  - the effect reads `uResolution`, so the material is GEOMETRY-DEPENDENT.
 *    It resolves when its node records, stays picture-cached between
 *    layouts, and re-records on a size change — static SDF chrome costs
 *    one recording rather than per-frame paint;
 *  - every style parameter is a named uniform, so binding one to a
 *    ch::Output (`.uniform("uBorderW", &width)`, `"uGlowR"`, `"uP0"`…)
 *    makes the material live. A pulsing border or a breathing glow is a
 *    bound scalar, not a repaint loop;
 *  - ONE compiled effect per shape KIND — the parameters are uniforms — so
 *    however many differently-styled nodes there are, this file compiles
 *    three shaders and no more.
 *
 * Distances are computed in aspect-corrected PIXEL space, never UV, so
 * borders stay even on a stretched node. The evaluation is analytic and
 * therefore exact at any zoom, with no glyph atlas; very small sharp star
 * tips are the case where an MSDF atlas would still do better.
 *
 * Known tradeoff: the anti-alias half-width is 0.75 LOCAL px, so a
 * recording replayed under a scaled host softens edges slightly. Declaring
 * a content-scale uniform would track zoom exactly, at the price of making
 * every material LIVE and painting per frame; this header chooses
 * cacheability, and a re-record on the next layout change re-crisps the
 * edges.
 *
 * The SDF paints the node's LOOK only. Hit-testing and clipping still use
 * the node's own geometry, so pair it with `.shape(shapes::star(...))`
 * when hits and clips should match the silhouette.
 */

#include <sigilcompose/core/Material.h>

namespace sigil::compose::sdf {

enum class Kind : uint8_t { RoundBox, Circle, Star };

struct Style;
struct Shape;
inline Shape roundBox(float radius);
inline Shape circle();
Shape star(int points, float pointiness);
Material material(const Shape& shape, const Style& style);

/** A silhouette, sized by the node's box minus the style's reserved pad.
 *  Built only through the factories below: the per-kind parameters are the
 *  shader's uP0..uP2 slots, they mean something different in each kind,
 *  and they are valid only as the factory packs them — star's `pointiness`
 *  is clamped into [2, points], so a raw triple is not a shape. Hence the
 *  private fields and the friend list. */
struct Shape {
 private:
  Kind kind = Kind::RoundBox;

 public:
 private:
  float p0 = 0, p1 = 0, p2 = 0;  // per-kind params (uniforms uP0..uP2)

  friend Shape roundBox(float radius);
  friend Shape circle();
  friend Shape star(int points, float pointiness);
  friend Material material(const Shape& shape, const Style& style);
};

/** Rounded box inscribed in the node (radius in px, clamped to half-size). */
inline Shape roundBox(float radius) {
  Shape s;
  s.kind = Kind::RoundBox;
  s.p0 = radius;
  return s;
}

/** Circle inscribed in the node's box. */
inline Shape circle() {
  Shape s;
  s.kind = Kind::Circle;
  return s;
}

/** N-pointed star. `pointiness` is m ∈ [2, points]: m = points is the
 *  regular polygon, and values toward 2 sharpen the arms. */
Shape star(int points, float pointiness);

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
};

/** The pad the style reserves inside the node's box: border half-width
 *  plus glow and shadow reach. Public so callers can size a node by its
 *  VISIBLE silhouette — box dimension = visible diameter + 2·sdf::pad(
 *  style) — rather than restating the formula at a call site that then
 *  drifts from this one.
 *
 *  PAD IS LAYOUT RESERVE, NOT APPEARANCE. It never changes the rendered
 *  glow's falloff, which is governed by exp(-d/glowRadius); a larger pad
 *  only guarantees the box does not crop it. Shrinking the pad does not
 *  tighten a glow, and growing the box without growing the content shrinks
 *  the visible interior to nothing — see `minBoxFor()`. */
float pad(const Style& style);

/** The box dimension that leaves @p contentPx of VISIBLE interior after
 *  the style's pad on both sides. Size nodes with this: the pad is taken
 *  out of the box, so a box that is not big enough for it has no visible
 *  interior at all and the node renders as nothing but its outer
 *  treatments — a 20 px box with a 5 px glow is already there. */
inline float minBoxFor(const Style& style, float contentPx) {
  return contentPx + 2.0f * pad(style);
}

/** The SDF material: shape plus style, one shader pass. The style's outer
 *  treatments (border half-width, glow falloff, shadow reach) reserve a pad
 *  inside the node's box so nothing clips; bindable uniforms animate within
 *  that reserve, so bind a glow up to the style's glowRadius and not past
 *  it — the pad was computed from the style, and a bound value that
 *  exceeds it clips.
 *
 *  Returns an EMPTY material if the shader failed to compile (the error is
 *  logged once when the effect is first built). An empty material paints
 *  nothing, so a node whose only fill is this one disappears. */
Material material(const Shape& shape, const Style& style);

}  // namespace sigil::compose::sdf
