#pragma once

/** @file
 * @ingroup compose-brush
 *
 * SigilCompose hatches — the parallel lattice clipped to a silhouette, and
 * the radial and concentric hatches about a centre.
 *
 * THE INK IS A `Fill`: a material laid across the rules — a gradient, a
 * pattern, a program, or a colour through `Fill::color` — or the ink in
 * force.
 */

#include <sigilgeometry/kit/Hatches.h>
#include <sigilgeometry/path/Numeric.h>

#include "sigilcompose/brush/Lines.h"

namespace sigil::compose::lines {

/** Lattice hatching: the rules of `pattern` stroked `width` px wide,
 *  filling the node's OUTLINE — clipped to it, so a concave shape hatches
 *  exactly rather than to its bounds. WHERE the rules lie is
 *  SigilGeometry's hatch (`geometry::shapes::Hatch`: spacing, angle in
 *  radians, taper, origin, inset and the `cross` pass); the ink and the
 *  width are this decoration's. A value decoration: compares, prunes and
 *  caches like any other. */
struct Hatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  geometry::shapes::Hatch pattern{.spacing = 6.0f,
                                  .angle = geometry::path::radians(45.0f)};
  float width = 1.2f;
  /** Live pitch and live angle (radians), on the same terms as
   *  `PathFormat::dashPhaseBinding`: an animatable, so a moiré that
   *  breathes, a tightening engraving or a rotating shade pass is one
   *  `bind()` chain rather than a second live value somebody steps by hand.
   *  Either one live makes `isRunning()` true, which is what declares
   *  the node volatile and keeps it repainting.
   *
   *  A decoration paints with only a `PaintContext` and has no instance
   *  holding a motion, so a value carrying its own TRANSITION has nothing
   *  to run it and reads as its target. */
  std::optional<motion::Animatable<float>> spacingBinding;
  std::optional<motion::Animatable<float>> angleBinding;

  bool isRunning() const {
    return (spacingBinding && spacingBinding->isRunning()) ||
           (angleBinding && angleBinding->isRunning());
  }
  float pitch() const {
    return spacingBinding ? spacingBinding->value() : pattern.spacing;
  }
  float angle() const {
    return angleBinding ? angleBinding->value() : pattern.angle;
  }

  bool operator==(const Hatch& other) const {
    return strokeFill == other.strokeFill && pattern == other.pattern &&
           width == other.width && spacingBinding == other.spacingBinding &&
           angleBinding == other.angleBinding;
  }

  /** The rules are clipped to the outline they fill. */
  bool staysOnOutline() const { return true; }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

/** RADIAL hatching: rules that fan out of a centre, rings concentric with
 *  it, or both, clipped to the node's outline.
 *
 *  `lines::hatch` is a parallel lattice at one fixed angle, which cannot
 *  describe a field engraved out of a point; approximating one from many
 *  rotated wedges costs a node per wedge for a single field.
 *
 *  `spokes` rules every 360/spokes degrees; `rings` draws circles at even
 *  radii. Set either to 0 for the other alone. `centre` is a FRACTION of
 *  the node's box, so it survives a resize. A value decoration: compares,
 *  prunes and caches like the rest. */
struct RadialHatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  int spokes = 48;
  int rings = 0;
  float width = 1.2f;
  /** Skip the innermost `holeFraction` of the reach — a fan out of a
   *  point crowds to solid ink at the centre otherwise. */
  float holeFraction = 0.08f;
  glm::vec2 centre = {0.5f, 0.5f};
  float rotateDegrees = 0.0f;
  /** STATED ring radii, in px from the centre. When non-empty this list
   *  replaces the `rings` spacing entirely — one circle per entry, exactly
   *  where it says. Use it whenever the radii matter: the even spacing
   *  runs out to the bounding box's HALF-DIAGONAL, so on a circular node
   *  the outermost ring lands at R·√2, outside the shape, and is clipped
   *  away entirely. Spokes keep their own reach either way. */
  std::vector<float> radiiPx;

  bool operator==(const RadialHatch& other) const {
    return strokeFill == other.strokeFill && spokes == other.spokes &&
           rings == other.rings && width == other.width &&
           holeFraction == other.holeFraction && centre == other.centre &&
           rotateDegrees == other.rotateDegrees && radiiPx == other.radiiPx;
  }

  /** The rules are clipped to the outline they fill. */
  bool staysOnOutline() const { return true; }

  void paint(draw::Pen& pen, const PaintContext& ctx) const;
};

}  // namespace sigil::compose::lines
