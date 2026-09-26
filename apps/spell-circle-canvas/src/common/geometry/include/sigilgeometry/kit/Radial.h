#pragma once

/** @file
 * @ingroup geometry-kit
 *
 * THE TWO GENERAL SHAPES over a box: `radial`, N vertices dealt round the
 * centre and joined or marked, and `ellipse`, the one conic inscribed in
 * the box with its sweep, its hole and its exponent. Polygon, star,
 * chords, ticks and dial segments are settings of the first; circle,
 * arc, sector, annulus, ring and squircle are settings of the second —
 * and each keeps its short name as a stock value over them.
 */

#include <cstdint>
#include <glm/vec2.hpp>
#include <utility>
#include <vector>

#include "sigilgeometry/kit/Corners.h"
#include "sigilgeometry/path/Outline.h"
#include "sigilgeometry/path/Radial.h"

namespace sigil::geometry::shapes {

/** The modifiers every general shape carries, over the free forms in
 *  `Corners.h`: its corners treated, its outline bent by a shaper, and
 *  the shape drawn about a centre at a radius. */
template <typename Self>
struct Modifiers {
  /** Every corner rounded, or cut, by @p radius. */
  auto cornered(float radius, CornerOptions options = {}) const {
    return Cornered<Self>{self(), radius, options};
  }
  /** The outline bent by @p shaper, once, where it is asked for. */
  template <path::ShaperScheme S>
  auto distorted(S shaper) const {
    return Shaped<Self, S>{self(), std::move(shaper)};
  }
  /** The shape drawn in the square of side `2 · radius` about
   *  @p centre. */
  path::Outline at(glm::vec2 centre, float radius) const {
    return shapes::at(self(), centre, radius);
  }

 private:
  const Self& self() const { return static_cast<const Self&>(*this); }
};

/** N VERTICES DEALT ROUND THE BOX'S CENTRE, joined into a loop, joined
 *  chord by chord, or marked — the options in `path::RadialOptions` say
 *  which. A polygon is `radial(6)`, a star `radial(10, {.radii = {1,
 *  0.42f}})`, a tick ladder `radial(60, {.connect = Connect::None})`, a
 *  ring of studs `radial(12, {.connect = Connect::None, .marks =
 *  {Mark::shape(circle().outline({8, 8}))}})` — one outline, and one
 *  node where a loop of elements would be twelve. */
struct Radial : Modifiers<Radial> {
  int count = 3;
  path::RadialOptions options{};

  bool operator==(const Radial& other) const {
    return count == other.count && options == other.options;
  }
  path::Outline outline(glm::vec2 size) const {
    return path::radialOutline(count, options, size);
  }
  /** The vertices themselves, in the same box. */
  std::vector<glm::vec2> points(glm::vec2 size) const;
};

/** @p count vertices dealt by @p options. */
inline Radial radial(int count, path::RadialOptions options = {}) {
  Radial shape;
  shape.count = count;
  shape.options = std::move(options);
  return shape;
}

/** How an ellipse that does not sweep the whole turn is closed — p5's
 *  arc modes. */
enum class Close : uint8_t {
  /** Not at all: the open arc, which strokes and cannot fill. */
  Open,
  /** Straight across from end to start. */
  Chord,
  /** Through the centre: the wedge, or with a hole the annular slice. */
  Pie,
};

/** THE ELLIPSE INSCRIBED IN THE BOX — a circle on a square box — and
 *  everything cut from it. Angles follow the screen: 0° is +x, sweeping
 *  clockwise. */
struct EllipseOptions {
  /** Where the sweep starts. */
  float fromDegrees = 0;
  /** How far it runs; a whole turn is the closed figure and ignores
   *  `close`. */
  float sweepDegrees = 360;
  Close close = Close::Pie;
  /** A concentric hole, as a fraction of the radius — the annulus, and
   *  with a partial sweep the donut slice. */
  float inner = 0;
  /** The same hole said as the ring's own width in px, which is what a
   *  ring keeps when its box does not; it wins over `inner`. */
  float thickness = 0;
  /** A concentric disc of this px radius at the centre, one outline with
   *  the ring so the pair fills and animates together. */
  float dot = 0;
  /** The superellipse |x|^e + |y|^e = 1: 2 the ellipse, 4–5 the app-icon
   *  squircle, larger toward the rectangle. A value other than 2 draws
   *  the whole figure. */
  float exponent = 2;
  /** Px pulled in from the box on every side. */
  float inset = 0;
  /** The largest true circle that fits, on a box that is not square. */
  bool uniform = false;
  /** Which way the closed figure is drawn — which decides which way
   *  glyphs on it face.
   *  @trap The winding and `start` together decide where an arc-length
   *  fraction is measured from; both move every label placed by
   *  fraction. */
  path::Winding winding = path::Winding::OutersClockwise;
  /** Which of the four extreme points the closed figure starts at, from
   *  the top clockwise: 1, the default, is due east. */
  unsigned start = 1;

  bool operator==(const EllipseOptions&) const = default;
};

/** The ellipse and its cuts as a comparable value. */
struct Ellipse : Modifiers<Ellipse> {
  EllipseOptions options{};
  bool operator==(const Ellipse& other) const {
    return options == other.options;
  }
  path::Outline outline(glm::vec2 size) const;
};

/** The ellipse @p options describe. */
inline Ellipse ellipse(EllipseOptions options = {}) {
  Ellipse shape;
  shape.options = options;
  return shape;
}

/** ANY OUTLINE SCALED INTO THE BOX: its bounds mapped onto the box —
 *  stretched, or with `preserveAspect` fitted and centred. */
struct FitOptions {
  bool preserveAspect = false;
  bool operator==(const FitOptions&) const = default;
};

/** An outline fitted to whatever box it is asked for. */
struct Fitted : Modifiers<Fitted> {
  path::Outline source{};
  FitOptions options{};
  bool operator==(const Fitted& other) const {
    return source == other.source && options == other.options;
  }
  path::Outline outline(glm::vec2 size) const;
};

/** @p source fitted to the box. */
inline Fitted fitted(path::Outline source, FitOptions options = {}) {
  Fitted shape;
  shape.source = std::move(source);
  shape.options = options;
  return shape;
}

}  // namespace sigil::geometry::shapes
