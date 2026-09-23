#pragma once

/** @file
 * @ingroup compose-core
 *
 * What the node's own surface is painted with.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/PaintBox.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
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
 *  only its decorations and children show. What may be passed: an
 *  `material::Color`, a `Fill`, a `motion::Animatable<Fill>`, a
 *  `material::skia::Paint`, or a `SurfacePaint`, which is the one value
 *  all of those convert into and the type a component declares. */
template <class Derived>
class PaintVerbs {
 public:
  /** Paint the surface with a colour, a shader, a transition between
   *  colours, or a LIVE binding — `fill(&output)` where the output
   *  holds a `Fill`. */
  Derived& fill(motion::Animatable<Fill> f);
  /** Fill with a paint — a gradient ramp, a blend stack, a sprite,
   *  SkSL — its unit square stretched over @p box: the element's own box
   *  by default, `Padding` or `Content` to start it inside the border or
   *  the padding, `Canvas` for one field several boxes show slices of. A
   *  static paint collapses to a Fill, so it caches and prunes on the
   *  same path.
   *  @trap A fill does not inherit, so `Subtree` is `Element`, and a
   *  border here is a stroke dressing the boundary rather than a box
   *  lane, so `Padding` is `Element` too. A text unit is refused, said
   *  once, and read as `Element`. */
  Derived& fill(material::skia::Paint m, PaintBox box = PaintBox::Element);
  /** A surface value supplied by component properties. Exact-type
   *  deduction keeps ordinary fill and material arguments on their own
   *  overloads. A box other than `Element` places a paint's unit square,
   *  so a surface with no paint to place — a colour, the ink in force, a
   *  custom property, a bound fill — is applied whole. */
  template <typename P>
    requires std::same_as<std::remove_cvref_t<P>, SurfacePaint>
  Derived& fill(P&& paint, PaintBox box = PaintBox::Element) {
    if (box != PaintBox::Element && !paint.none())
      if (std::optional<material::skia::Paint> placed = paint.collapsedPaint())
        return fill(std::move(*placed), box);
    paint.apply(self());
    return self();
  }

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
