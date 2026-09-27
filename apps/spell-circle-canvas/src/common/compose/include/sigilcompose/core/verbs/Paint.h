#pragma once

/** @file
 * @ingroup compose-core
 *
 * What the node's own surface is painted with.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/PaintBox.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/paint/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>

namespace sigil::material::pattern {
class Tile;
}

namespace sigil::compose {

class Pattern;

/** THE SURFACE. Unfilled when unstated, so a box paints nothing and
 *  only its decorations and children show. What may be passed: a
 *  `material::Color`, a `Fill`, a `motion::Animatable<Fill>` or a
 *  `material::Material`. A `Fill` is the one value the others but the
 *  animatable convert into, and the type a component declares. */
template <class Derived>
class PaintVerbs {
 public:
  /** Paint the surface with a colour, a shader, a transition between
   *  colours, or a LIVE binding — `fill(&output)` where the output
   *  holds a `Fill`. */
  Derived& fill(motion::Animatable<Fill> f);
  /** FILL WITH A MATERIAL — a gradient, layers, an image, a program —
   *  its unit square stretched over @p box: the element's own box by
   *  default, `Padding` or `Content` to start it inside the border or the
   *  padding, `Canvas` for one field several boxes show slices of. A
   *  static material collapses to a Fill, so it caches and prunes on the
   *  same path. Its effects dress the node's own layer — shadows and glows
   *  beneath the fill, strokes and bevels over it, a hard shadow as an
   *  echo of the fill and the text; passes that read pixels run over the
   *  node and its subtree as `filter` does. The surface is a 3D renderer's
   *  and is not painted here.
   *  @trap A fill does not inherit, so `Subtree` is `Element`, and a
   *  border here is a stroke dressing the boundary rather than a box
   *  lane, so `Padding` is `Element` too. A text unit is refused, said
   *  once, and read as `Element`. */
  Derived& fill(material::Material material, PaintBox box = PaintBox::Element);
  /** A FILL, as a component property hands it on: a material in it is
   *  placed over @p box exactly as `fill(material::Material, box)` places
   *  it, and a colour, the ink in force or a custom property — which have
   *  no unit square to place — is applied whole. A text unit is refused
   *  and said once, whatever the fill. */
  Derived& fill(Fill fill, PaintBox box = PaintBox::Element);

  /** NEITHER A TILE NOR A PATTERN IS A FILL: a pattern's bake is its
   *  identity, and one minted inside a describe has no bake in it and
   *  re-renders its tile every frame. Hold the pattern where assets are
   *  held and fill with what it bakes. Deleted rather than absent so
   *  the error names the rule. */
  Derived& fill(material::pattern::Tile tile) = delete;
  Derived& fill(const Pattern& pattern) = delete;
  /** Solid-colour sugar: `fill({r,g,b,a})` without the `Fill::`
   *  ceremony. */
  Derived& fill(material::Color color) {
    return fill(motion::Animatable<Fill>{Fill::color(color)});
  }

 private:
  Derived& self() { return static_cast<Derived&>(*this); }
  detail::ElementNode* declarations() {
    return detail::NodeAccess::declarations(self());
  }
};

}  // namespace sigil::compose
