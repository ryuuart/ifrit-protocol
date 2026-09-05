#pragma once

/** @file
 * One procedural drawing tool: plain data saying how each dab lands.
 */

#include <include/core/SkColor.h>
#include <sigildraw/Constants.h>
#include <sigildraw/brush/Dab.h>
#include <sigildraw/brush/Dynamics.h>
#include <sigildraw/brush/Grain.h>
#include <sigildraw/brush/Pressure.h>
#include <sigildraw/brush/Shape.h>

#include <functional>
#include <optional>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** How the tool deposits pigment along the sampled centreline. */
enum class Tip {
  Dust,     ///< dry-media particles dispersed around the centreline
  Fibres,   ///< parallel hairs, intermittently dry
  Nib,      ///< one continuous pressure-width mark
  Scatter,  ///< particles distributed around each sample
  Image,    ///< the tool's shape source stamped at every dab
  Custom,   ///< a caller-provided drawing callback stamped at every dab
};

/** How a shape or callback tip is oriented at each dab. */
enum class Rotation {
  Fixed,
  Natural,  ///< follows the direction of travel
  Random,
  Tilt,  ///< follows the azimuth of a tilted stylus
};

/** The full description of one tool.
 *
 *  Width, spacing and scatter are canvas units. Opacity is the tool's
 *  load, a unit value, and the colour's own alpha multiplies it. Density
 *  is how much a dry tip deposits: the probability a dust dab lands, the
 *  share of fibres and scatter particles that deposit, so above one it
 *  only lets a light pressure keep depositing. Bristles is the fibre or
 *  particle count. Sharpness moves particles toward the centreline; noise
 *  widens the per-stroke opacity roll. Speed responses take effect
 *  against `speedReference` units per second and act on live input only,
 *  since a stored path carries no clock. `markerTip` builds up pigment at
 *  the two ends of a nib, image or custom stroke.
 *
 *  `shape` is what a Tip::Image stamps and what states the stroke's
 *  spacing and scatter against the stamp; `grain` is the texture the
 *  whole mark is laid through, whatever the tip; `dynamics` are the
 *  curves the device drives, each multiplying the scalar responses above
 *  it. All three are what an imported brush arrives as, and all three are
 *  absent from a tool that has none. */
struct Tool {
  Tip tip = Tip::Dust;
  SkColor4f color{0, 0, 0, 1};
  float width = 8.0f;
  float spacing = 1.0f;
  float opacity = 0.45f;
  float scatter = 0.3f;
  float density = 0.8f;
  int bristles = 12;
  Pressure pressure{0.65f, 1.0f, 0.45f};
  Constant blend = BLEND;
  Rotation rotation = Rotation::Natural;
  float angle = 0.0f;
  float aspect = 1.0f;
  float sizeJitter = 0.0f;
  float opacityJitter = 0.0f;
  float spacingJitter = 0.0f;
  float speedSize = 0.0f;
  float speedOpacity = 0.0f;
  float speedReference = 1200.0f;
  float pressureSize = 1.0f;
  float pressureOpacity = 0.1f;
  float tiltSize = 0.0f;
  float tiltOpacity = 0.0f;
  float tiltAspect = 0.0f;
  float tiltOffset = 0.0f;
  float sharpness = 0.5f;
  float noise = 0.0f;
  bool markerTip = true;
  std::optional<Shape> shape;
  std::optional<Grain> grain;
  Dynamics dynamics;
  /** A custom tip draws around the origin in a nominal one-unit square:
   *  the pen arrives translated to the dab, rotated to its angle and
   *  scaled to its size and aspect, with the tool's pigment as fill and
   *  stroke and the pen's default rect and ellipse modes. Style the tip
   *  changes holds until the next dab resets those four; the transform is
   *  restored after every dab. */
  std::function<void(Pen&, const Dab&)> customTip;
};

/** Stock tools. Every field is public on the returned value, so a sketch
 *  tunes one or describes a tool of its own without a catalogue. */
[[nodiscard]] Tool pencil(SkColor4f color, float width = 1.4f);
[[nodiscard]] Tool charcoal(SkColor4f color, float width = 9.0f);
[[nodiscard]] Tool marker(SkColor4f color, float width = 16.0f);
[[nodiscard]] Tool watercolor(SkColor4f color, float width = 22.0f);
[[nodiscard]] Tool spray(SkColor4f color, float width = 18.0f);

/** Rolls the stroke-wide randomness once — the pressure envelope's jitter
 *  or variation and the opacity noise — from the pen's stream. A live
 *  stroke keeps the returned value for its whole lifetime, so input event
 *  boundaries cannot change its opacity or envelope. */
[[nodiscard]] Tool prepareStroke(Pen& pen, const Tool& tool);

/** How far apart @p tool lays its dabs, in canvas units. A tool with a
 *  shape states its spacing against the stamp, so the answer follows the
 *  width; a procedural tool states it in canvas units already. Every
 *  resampling in the library asks this rather than reading `spacing`. */
[[nodiscard]] float spacingOf(const Tool& tool);

}  // namespace sigil::draw::brush
