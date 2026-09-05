#pragma once

/** @file
 * SigilCompose hatches — the parallel lattice clipped to a silhouette, and
 * the radial and concentric hatches about a centre.
 */

#include "sigilcompose/brush/Lines.h"

namespace sigil::compose::lines {

/** Lattice hatching: parallel rules `spacing` px apart at `angleDeg`,
 *  `width` px each, filling the node's OUTLINE — clipped to it, so a
 *  concave shape hatches exactly rather than to its bounds. `cross` adds
 *  the perpendicular pass. A value decoration: compares, prunes and caches
 *  like any other. */
struct Hatch {
  Fill strokeFill = Fill::color({1, 1, 1, 1});
  float spacing = 6.0f;
  float width = 1.2f;
  float angleDeg = 45.0f;
  bool cross = false;
  /** Live pitch and live angle, on the same terms as
   *  `PathFormat::dashPhaseBinding`: an animatable, so a moiré that
   *  breathes, a tightening engraving or a rotating shade pass is one
   *  `bind()` chain rather than a second Output somebody steps by hand.
   *  Either one live makes `isAnimated()` true, which is what declares
   *  the node volatile and keeps it repainting.
   *
   *  A decoration paints with only a `PaintContext` and has no instance
   *  holding a motion, so a value carrying its own TRANSITION has nothing
   *  to run it and reads as its target. */
  std::optional<motion::Animatable<float>> spacingBinding;
  std::optional<motion::Animatable<float>> angleBinding;

  bool isAnimated() const {
    return (spacingBinding && motion::isLive(nullptr, *spacingBinding)) ||
           (angleBinding && motion::isLive(nullptr, *angleBinding));
  }
  float pitch() const {
    return spacingBinding ? motion::resolveFloatAt(nullptr, *spacingBinding)
                          : spacing;
  }
  float angle() const {
    return angleBinding ? motion::resolveFloatAt(nullptr, *angleBinding)
                        : angleDeg;
  }

  bool operator==(const Hatch& o) const {
    return strokeFill == o.strokeFill && spacing == o.spacing &&
           width == o.width && angleDeg == o.angleDeg && cross == o.cross &&
           spacingBinding == o.spacingBinding && angleBinding == o.angleBinding;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
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
  SkPoint centre = {0.5f, 0.5f};
  float rotateDeg = 0.0f;
  /** STATED ring radii, in px from the centre. When non-empty this list
   *  replaces the `rings` spacing entirely — one circle per entry, exactly
   *  where it says. Use it whenever the radii matter: the even spacing
   *  runs out to the bounding box's HALF-DIAGONAL, so on a circular node
   *  the outermost ring lands at R·√2, outside the shape, and is clipped
   *  away entirely. Spokes keep their own reach either way. */
  std::vector<float> radiiPx;

  bool operator==(const RadialHatch& o) const {
    return strokeFill == o.strokeFill && spokes == o.spokes &&
           rings == o.rings && width == o.width &&
           holeFraction == o.holeFraction && centre == o.centre &&
           rotateDeg == o.rotateDeg && radiiPx == o.radiiPx;
  }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

}  // namespace sigil::compose::lines
