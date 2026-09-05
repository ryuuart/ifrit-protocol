#pragma once
/** @file
 * The scanline lattice: parallel lines cut to the inside of a set of
 * rings.
 *
 * A hatch, a fill drawn by a pen plotter, a shaded region and a mass of
 * strokes are all the same construction — lines at an angle, a spacing
 * apart, kept where they are inside and dropped where they are not — and
 * the marks it answers with are CENTRELINES, which is what separates it
 * from clipping a line pattern to an outline: a centreline can be walked,
 * drawn along with a natural-media tool, split, or joined to the next one.
 */
#include <glm/vec2.hpp>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** One mark of a lattice: the piece of one line that lay inside. */
struct LatticeMark {
  glm::vec2 from{0, 0};
  glm::vec2 to{0, 0};
};

/** How the lines of a lattice are laid down. */
struct LatticeOptions {
  /** The distance between one line and the next. */
  float spacing = 1;
  /** Which way the lines run, in radians clockwise from +x in Skia's
   *  y-down space. */
  float angle = 0;
  /** What each gap is multiplied by after the one before it: one is an
   *  even lattice, more spreads the lines as the scan advances and less
   *  crowds them. A gap never falls below an eighth of a unit, so a
   *  taper toward zero crowds rather than stalls. */
  float taper = 1;
  /** The most lines one lattice lays down. A bound rather than a
   *  preference: a spacing far smaller than the rings it fills would
   *  otherwise answer with a vector nobody asked the size of. */
  int maxLines = 10000;
};

/** PARALLEL LINES CUT TO THE EVEN-ODD INTERIOR of `rings`: each line's
 *  crossings with every edge, sorted along the line and paired, so a ring
 *  inside another is a hole and two rings side by side are two islands —
 *  the rule `containsEvenOdd` answers a point with, and the rule a path
 *  filled with `SkPathFillType::kEvenOdd` is drawn by.
 *
 *  The marks come in scan order, each running in the direction the lines
 *  run. Rings of fewer than three points bound no area and are skipped;
 *  the scan covers the rings that are left. Both ends of a ring are
 *  joined whether or not it says it is closed, because an area is what is
 *  being filled. */
std::vector<LatticeMark> lattice(std::span<const Polyline> rings,
                                 const LatticeOptions& options);

}  // namespace sigil::geometry::path
