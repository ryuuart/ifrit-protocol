#pragma once
/** @file
 * The scanline lattice: parallel lines cut to the inside of a set of
 * rings, and the multigrid those lines dualise into a tiling of rhombs.
 *
 * A hatch, a fill drawn by a pen plotter, a shaded region and a mass of
 * strokes are all the same construction — lines at an angle, a spacing
 * apart, kept where they are inside and dropped where they are not — and
 * the marks it answers with are CENTRELINES, which is what separates it
 * from clipping a line pattern to an outline: a centreline can be walked,
 * drawn along with a natural-media tool, split, or joined to the next one.
 */
#include <glm/vec2.hpp>
#include <optional>
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
  /** WHERE THE LADDER IS MEASURED FROM: a point every line's spacing is
   *  counted from, so the same lattice over a moving shape keeps its
   *  lines in the same places and the fill stops crawling as the shape
   *  animates. Unset lays the first line half a gap inside the rings,
   *  which is what fills a shape whose place is not fixed. */
  std::optional<glm::vec2> origin;
  /** The most lines one lattice lays down. A bound rather than a
   *  preference: a spacing far smaller than the rings it fills would
   *  otherwise answer with a vector nobody asked the size of. */
  int maxLines = 10000;
  bool operator==(const LatticeOptions&) const = default;
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

// ---------------------------------------------------------------------------
// The multigrid, and the tiling of rhombs it dualises into.

/** ONE FAMILY OF PARALLEL LINES of a multigrid: which way they face,
 *  how far apart they stand, and where the ladder is phased. */
struct MultigridFamily {
  /** The direction the family's lines are NORMAL to, in radians from +x.
   *  The lines themselves run across it, and this direction is also the
   *  EDGE the family contributes to every rhomb it bounds — which is why
   *  a family is named by its normal and not by the way its lines run. */
  double normal = 0;
  /** The distance between one line of the family and the next. */
  double spacing = 1;
  /** THE PHASE, in whole spacings: line `k` of the family is where the
   *  normal coordinate reaches `(k - offset) * spacing`. The offsets are
   *  what pick one tiling out of the family a set of directions admits,
   *  and their SUM decides the class: a sum that lands on a whole number
   *  puts every family's line through one point, which is the exactly
   *  symmetric member, and any other sum is an unsymmetric one. */
  double offset = 0;
};

/** `count` families evenly spread, all at one spacing and one phase.
 *
 *  The spread is a WHOLE TURN for an odd count and a HALF TURN for an
 *  even one, which is the smallest turn giving `count` distinct line
 *  directions: an even count over a whole turn would land two families
 *  on the same lines, and a pair of parallel families bounds no rhomb.
 *  An odd ring over the whole turn carries the `count`-fold rotation
 *  that permutes its families cyclically, so the tiling it dualises into
 *  is exactly symmetric about the origin.
 *
 *  Five is the Penrose rhombs, four the Ammann-Beenker octagonal tiling,
 *  three the rhombille. */
std::vector<MultigridFamily> multigridRing(int count, double offset,
                                           double spacing = 1);

/** ONE RHOMB of a multigrid's dual tiling: the cell of the plane where
 *  one line of family `families[0]` crosses one line of family
 *  `families[1]`. */
struct MultigridRhomb {
  /** The two crossing families, the lower index first. Their two normals
   *  are the rhomb's two edge directions, so the shape of the rhomb is
   *  the angle between them and nothing else. */
  int families[2]{0, 0};
  /** Which line of each family crosses here. With the two families this
   *  is the rhomb's whole identity — no two rhombs share it. */
  int lines[2]{0, 0};
  /** Indices into `MultigridTiling::vertices`, running round the rhomb
   *  from its low corner: along the first family's normal, then the
   *  second's, then back. */
  int corners[4]{0, 0, 0, 0};
};

/** A MULTIGRID'S DUAL: every rhomb, over one deduplicated set of
 *  corners, so two rhombs meeting at a corner name the same vertex. */
struct MultigridTiling {
  /** Plane positions in the tiling's OWN units, in which every rhomb
   *  edge is one long whatever the lines that made it were spaced. */
  std::vector<glm::dvec2> vertices;
  std::vector<MultigridRhomb> rhombs;
};

/** How much of a multigrid is dualised, and how near two corners stand
 *  before they are one. */
struct MultigridOptions {
  /** How far from the origin the tiling reaches, in the tiling's own
   *  units: a rhomb is kept when any of its corners stands within this.
   *  The line indices that answer it are DERIVED — the dual map carries
   *  a reach in the tiling back to a reach in the grid — so no caller
   *  ever states an index range. */
  double radius = 8;
  /** How near two corners stand before they are the same vertex. A
   *  rhomb tiling's distinct corners stand a good fraction of an edge
   *  apart, so this is a weld for arithmetic and not a simplification. */
  double tolerance = 1e-6;
  /** The most rhombs one tiling holds. A bound rather than a preference:
   *  a radius far larger than the spacing would otherwise answer with a
   *  vector nobody asked the size of. */
  int maxRhombs = 500000;
  bool operator==(const MultigridOptions&) const = default;
};

/** N FAMILIES OF PARALLEL LINES DUALISED INTO A TILING OF RHOMBS — de
 *  Bruijn's construction, which is the one operation an aperiodic rhomb
 *  tiling is: each crossing of two lines becomes a rhomb whose edges are
 *  the two families' normals, placed by counting how many lines of every
 *  family stand between the crossing and the origin.
 *
 *  Five evenly spread families give the Penrose rhombs, four the
 *  Ammann-Beenker squares and 45-degree rhombs, three the rhombille;
 *  families at angles of the caller's own choosing give the tiling those
 *  angles admit, and there is no other operation behind any of them.
 *
 *  SOLVED IN DOUBLE, AND THE ANSWER IS IN DOUBLE. The place of a rhomb
 *  is a count of lines, and a count is read off a crossing by a ceiling:
 *  a crossing that lands a hair on the wrong side of a line moves that
 *  rhomb a whole edge, so the arithmetic that finds it has to hold more
 *  digits than the picture it ends up in. A grid rounded to float before
 *  it is dualised does not merely blur — it tiles differently.
 *
 *  Two families that face the same way never cross and bound no rhomb,
 *  and are passed over. Fewer than two families dualise into nothing. */
MultigridTiling multigrid(std::span<const MultigridFamily> families,
                          const MultigridOptions& options = {});

}  // namespace sigil::geometry::path
