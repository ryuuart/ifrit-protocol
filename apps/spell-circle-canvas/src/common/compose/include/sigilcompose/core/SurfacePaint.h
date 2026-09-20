#pragma once

/** @file
 * @ingroup compose-core
 *
 * `SurfacePaint` — the one value that stands for anything a node's
 * surface can be painted with, so a component takes ONE property and
 * still accepts a colour, a live binding, a transition, a material
 * recipe or a whole Skia paint.
 */

#include <sigilcompose/core/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <utility>
#include <variant>

namespace sigil::compose {

class Element;

/** A COMPONENT'S SURFACE — WHAT ANY OF THEM MEANS BY "PAINT THIS".
 *
 *  It converts implicitly from every form `Element::fill()` accepts, so a
 *  component declares one `SurfacePaint` property and its caller writes
 *  whichever it already holds:
 *
 *      Fill                        a colour or a shader
 *      motion::Animatable<Fill>    a fill that eases or is driven
 *      const choreograph::Output<Fill>*  a live binding
 *      motion::Transitioned<Fill>  a fill mid-transition
 *      material::skia::Paint       a gradient ramp, blend stack, SkSL
 *      material::Material          a recipe, wrapped as that paint
 *
 *  `apply()` writes it onto an element exactly as the matching `fill()`
 *  overload would; an empty paint leaves that element's fill alone. A
 *  bound output must outlive the component, as it must when it is passed
 *  to `Element::fill()` directly. */
class SurfacePaint {
 public:
  SurfacePaint() = default;
  // NOLINTBEGIN(google-explicit-constructor)
  SurfacePaint(Fill fill)
      : m_value(motion::Animatable<Fill>{std::move(fill)}) {}
  SurfacePaint(motion::Animatable<Fill> fill) : m_value(std::move(fill)) {}
  SurfacePaint(const choreograph::Output<Fill>* fill)
      : m_value(motion::Animatable<Fill>{fill}) {}
  SurfacePaint(motion::Transitioned<Fill> fill)
      : m_value(motion::Animatable<Fill>{std::move(fill)}) {}
  SurfacePaint(material::skia::Paint paint) : m_value(std::move(paint)) {}
  SurfacePaint(material::Material recipe)
      : m_value(material::skia::Paint::recipe(std::move(recipe))) {}
  // NOLINTEND(google-explicit-constructor)

  [[nodiscard]] bool none() const;
  Element& apply(Element& element) const;
  /** A decoration reads live outputs and materials at paint. Node-owned
   *  transitions are applied by Element; decorations read their target. */
  [[nodiscard]] Fill resolve(const PaintContext& context) const;
  [[nodiscard]] bool isAnimated() const;
  bool operator==(const SurfacePaint&) const = default;

 private:
  std::variant<motion::Animatable<Fill>, material::skia::Paint> m_value;
};

}  // namespace sigil::compose
