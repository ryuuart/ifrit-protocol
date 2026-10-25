#pragma once

/** @file
 * @ingroup compose-core
 *
 * CSS'S BOX SHADOW as a value: a copy of the node's outline moved, grown
 * and softened in one colour, outside the outline or inset within it —
 * what `.boxShadow(…)` lays and what `.background()` takes by hand.
 */

#include <sigilcompose/core/Paint.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmotion/values/Animatable.h>

#include <glm/vec2.hpp>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::draw {
class Pen;
}  // namespace sigil::draw

namespace sigil::compose {

/** CSS'S BOX SHADOW: a soft copy of the node's outline, moved, grown and
 *  softened, in one colour — a value DecorationScheme (so a static
 *  shadowed node prunes without memo). Its fields are CSS's `box-shadow`
 *  words, `offset`, `blur`, `spread`, `color` and `inset`, as on
 *  `TextShadow` and `material::ShadowOptions`. `.boxShadow(…)` lays one
 *  where CSS paints it; attached by hand with `.background()` *before*
 *  the fill, the fill paints over it. */
struct Shadow {
  /** How far the shadow falls from the outline, in local pixels. */
  glm::vec2 offset = {0, 0};
  /** CSS's blur radius in local pixels: a Gaussian of half of it. */
  float blur = 0;
  /** Local pixels the outline grows by before it is blurred; a negative
   *  spread shrinks it. On an inset shadow it widens the band, and a
   *  negative one is read as none. */
  float spread = 0;
  /** What the shadow is painted in — a colour (`Fill::color`), a
   *  material, or the ink in force or a custom property, read where the
   *  shadow is painted — and CSS's `box-shadow` colour: the ink in force
   *  where none is named. */
  Fill color = Fill::currentInk();
  /** Inside the outline rather than outside it: a band hugging the inner
   *  edge, cast along the offset — so (0, 3) shades the TOP inner edge —
   *  and cut to the shape, painted over the fill as CSS's inset shadow
   *  is. */
  bool inset = false;

  /** Bound offsets: when set, the live value's current number REPLACES that
   *  axis of `offset` each paint, and the decoration declares itself
   *  animated (per-frame volatility) — the hover-lift shadow slides
   *  without re-describing. `maxBind` reserves cull reach for the bound
   *  range (bleed() can't read a future value). */
  std::optional<motion::Animatable<float>> bindOffsetX;
  std::optional<motion::Animatable<float>> bindOffsetY;
  float maxBind = 0.0f;

  /** CSS box-shadow semantics: knock the shape's own footprint OUT of the
   *  shadow, so nothing paints under the node. A translucent node over a
   *  knocked-out shadow stays clear instead of sampling its own shadow
   *  through itself. */
  bool knockout = false;

  bool operator==(const Shadow&) const = default;
  bool isRunning() const {
    return (bindOffsetX && bindOffsetX->isRunning()) ||
           (bindOffsetY && bindOffsetY->isRunning());
  }
  /** Paint reach beyond the node's bounds; the recording's cull rect grows
   *  by this. Under-report it and a big soft shadow is clipped at the
   *  node's picture-cache bounds, which is why the bound range has to be
   *  declared through `maxBind` — bleed() cannot read a future value. An
   *  inset shadow is cut to the shape and reaches nothing past it. */
  float bleed() const;
  /** The shadow is the outline it is handed, moved by its offset, grown
   *  by its spread and softened by its blur, all of which `bleed()`
   *  holds. */
  bool staysOnOutline() const { return true; }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** CSS'S `box-shadow` READ FROM ITS TEXT: a comma-separated list, each
 *  shadow two to four lengths — the offset, a blur radius and a spread —
 *  with a colour and `inset` before or after them, the first on top —
 *  `readShadow("0 4px 12px 2px rgba(0 0 0 / .3) inset")`. A length is in
 *  `px` or `pt`, or a bare zero; the colour is any CSS colour `readColor`
 *  reads, or `currentcolor`, the ink in force, which is also what an
 *  unnamed colour is. `none` is the empty list. Nothing where any shadow
 *  does not read, a blur radius is negative, or a length is in a unit
 *  measured against the font or the viewport. */
std::optional<std::vector<Shadow>> readShadow(std::string_view text);

/** A blurred copy of the node's outline cast at @p offset in @p color — a
 *  material converts, and `Fill::currentInk()` or `Fill::var(name)` follow
 *  the node's cascade — attach it as the FIRST background so everything
 *  else paints over it. The advanced spelling beneath `.boxShadow(…)`. */
inline Shadow shadow(Fill color, glm::vec2 offset, float blur) {
  return Shadow{.offset = offset, .blur = blur, .color = std::move(color)};
}
/** The same in a flat colour. */
inline Shadow shadow(material::Color color, glm::vec2 offset, float blur) {
  return shadow(Fill::color(color), offset, blur);
}

}  // namespace sigil::compose
