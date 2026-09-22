#pragma once

/** @file
 * @ingroup compose-core
 *
 * What the node's own surface is painted with.
 */

#include <sigilcompose/core/Declarations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <concepts>
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
   *  SkSL. A static paint collapses to a Fill, so it caches and prunes
   *  on the same path. */
  Derived& fill(material::skia::Paint m);
  /** A surface value supplied by component properties. Exact-type
   *  deduction keeps ordinary fill and material arguments on their own
   *  overloads. */
  template <typename P>
    requires std::same_as<std::remove_cvref_t<P>, SurfacePaint>
  Derived& fill(P&& paint) {
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
