#pragma once

#include <sigilcompose/core/Paint.h>
#include <sigilmotion/values/Animatable.h>

#include <optional>
#include <utility>
#include <variant>

namespace sigil::compose {

class Element;

/** A component's surface: a fill, a live fill binding, or a material.
 *  Empty paint leaves the supplied element's fill alone. Bound outputs
 *  must outlive the component, just as when passed to Element directly. */
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
  /** The one comparable Fill a slot that stores a Fill can hold, with no
   *  frame to resolve against: a plain fill answers itself, a static
   *  paint collapses through `toFill`, and a live or geometry-dependent
   *  one answers `Fill::none()` because the colour it would answer is
   *  the frame's and the slot has no frame. */
  [[nodiscard]] Fill collapsedFill() const;
  /** The one paint a slot that stores a paint can hold: a paint answers
   *  itself, a plain colour becomes a solid and a plain shader a shader
   *  leaf. A fill that reads the tree — the ink in force, a custom
   *  property — and a live fill binding answer nothing, because a paint
   *  slot resolves without the tree and without the binding's identity. */
  [[nodiscard]] std::optional<material::skia::Paint> collapsedPaint() const;
  [[nodiscard]] bool isAnimated() const;
  bool operator==(const SurfacePaint&) const = default;

 private:
  std::variant<motion::Animatable<Fill>, material::skia::Paint> m_value;
};

}  // namespace sigil::compose
