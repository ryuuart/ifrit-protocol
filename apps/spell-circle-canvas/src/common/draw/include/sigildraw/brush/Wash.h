#pragma once

/** @file
 * A wet pigment wash inside a polygon.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <sigildraw/Constants.h>

#include <optional>
#include <span>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

enum class BleedDirection { Out, In };

/** A layered translucent pigment wash. Opacity is the pigment's load and
 *  the colour's own alpha multiplies it. Bleed and texture are unit
 *  values; border is how much pigment gathers at the edge; layers is how
 *  many independently perturbed deposits form the body; a bleed angle, in
 *  radians, pushes the layers that way. */
struct Wash {
  SkColor4f color{0, 0, 0, 1};
  float opacity = 150.0f / 255.0f;
  float bleed = 0.07f;
  float texture = 0.8f;
  float border = 0.5f;
  bool scatter = true;
  BleedDirection bleedDirection = BleedDirection::Out;
  std::optional<float> bleedAngle;
  int layers = 18;
  Constant blend = MULTIPLY;

  bool operator==(const Wash&) const = default;
};

/** Builds translucent, softly displaced layers around and within the
 *  polygon, adds granulation and edge pooling, and composites the whole
 *  wash onto the canvas with its blend as one layer. The pen's state is
 *  restored. */
void wash(Pen& pen, const Wash& pigment, std::span<const SkPoint> polygon);

}  // namespace sigil::draw::brush
