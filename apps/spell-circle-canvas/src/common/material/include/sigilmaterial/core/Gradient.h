#pragma once

/** @file
 * @ingroup material-core
 *
 * WHAT A GRADIENT IS TOLD before any renderer draws it: its colour stops,
 * and the one options struct the three gradients share — which units
 * their points are in, how far a radial reaches, what is painted past
 * the ends, and the parts only a radial or a conic reads.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/color/Ramp.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <initializer_list>
#include <optional>
#include <vector>

namespace sigil::material {

/** WHAT A GRADIENT'S POINTS AND RADII ARE MEASURED IN. `Box` is the
 *  painted box's unit square — (0, 0) its top-left, (1, 1) its
 *  bottom-right, whatever size layout gives it — so one gradient fits
 *  every box it paints. `Pixels` is the painted node's own px, for a
 *  gradient placed against a canvas whose size the sketch fixes. */
enum class GradientUnits : uint8_t { Box, Pixels };

/** WHAT A RADIUS OF 1 REACHES, for a radial gradient in box units.
 *  Both are measured from the BOX's centre in its unit square, so on a
 *  box that is not square the circle is an ellipse. For a gradient
 *  centred on the box they are CSS's ellipse extents of the same names.
 *  @trap Unlike CSS's, they do not follow the gradient's own centre: a
 *  gradient moved off the box's centre keeps its size, and a radius of 1
 *  no longer lands exactly on a corner or a side. */
enum class RadialExtent : uint8_t {
  FarthestCorner,  ///< a radius of 1 reaches the box's corners
  ClosestSide,     ///< a radius of 1 reaches the middle of each side
};

/** WHAT A GRADIENT PAINTS PAST ITS ENDS. */
enum class Repeat : uint8_t {
  Pad,     ///< the end colours carry on
  Repeat,  ///< the stops start over
  Mirror,  ///< the stops run back, then forward again
  None,    ///< nothing is painted past the ends
};

/** HOW A GRADIENT IS PLACED, the one options struct `linearGradient`,
 *  `radialGradient` and `conicGradient` share. Every field has the
 *  default the common case wants; a gradient reads only the fields that
 *  mean something to it. */
struct GradientOptions {
  /** What the points and radii are measured in. */
  GradientUnits units = GradientUnits::Box;
  /** Radial, box units: what a radius of 1 reaches. */
  RadialExtent extent = RadialExtent::FarthestCorner;
  /** What is painted past the first and the last stop. */
  Repeat repeat = Repeat::Pad;
  /** Radial: an offset focus. The stops then run from the circle around
   *  the focus, of @p focusRadius, to the gradient's own circle, which
   *  stays where it is — a highlight displaced off a sphere's centre. */
  std::optional<glm::vec2> focus;
  float focusRadius = 0.0f;
  /** Conic: the window the stops are spread over, in degrees clockwise
   *  from 3 o'clock, so 12 o'clock is -90.
   *  @trap Outside [0, 360] the window CLAMPS rather than wrapping, so
   *  `{.startDegrees = 90, .endDegrees = 450}` paints a flat quarter;
   *  rotate the stops instead. */
  float startDegrees = 0.0f;
  float endDegrees = 360.0f;

  bool operator==(const GradientOptions&) const = default;
};

/** THE STOPS A GRADIENT TAKES: a list of `ColorStop`s, plain colours
 *  spaced evenly from the first offset to the last, or a `Ramp`.
 *
 *  A ramp read straight in sRGB with no easing hands over its own stops;
 *  a ramp walked in another space, or eased, is read at evenly spaced
 *  offsets, so the gradient shows the walk the ramp names rather than a
 *  straight line between its stops.
 *  @trap Two stops at one offset are a hard edge for a list; a ramp that
 *  has to be read at offsets softens that edge to one reading's width. */
class ColorStops {
 public:
  ColorStops() = default;
  // NOLINTBEGIN(google-explicit-constructor): each is a way to write stops
  ColorStops(std::initializer_list<ColorStop> stops) : m_stops(stops) {}
  ColorStops(std::vector<ColorStop> stops) : m_stops(std::move(stops)) {}
  ColorStops(std::initializer_list<Color> colors)
      : ColorStops(std::vector<Color>(colors)) {}
  ColorStops(const std::vector<Color>& colors);
  ColorStops(const Ramp& ramp);
  // NOLINTEND(google-explicit-constructor)

  /** The stops, with every offset written out. */
  [[nodiscard]] const std::vector<ColorStop>& stops() const { return m_stops; }
  /** True when the offsets were not authored but spaced evenly, so a
   *  renderer that spaces them itself may be handed none. */
  [[nodiscard]] bool evenlySpaced() const { return m_evenlySpaced; }
  [[nodiscard]] bool empty() const { return m_stops.empty(); }
  [[nodiscard]] size_t size() const { return m_stops.size(); }

  bool operator==(const ColorStops&) const = default;

 private:
  std::vector<ColorStop> m_stops;
  bool m_evenlySpaced = false;
};

}  // namespace sigil::material
