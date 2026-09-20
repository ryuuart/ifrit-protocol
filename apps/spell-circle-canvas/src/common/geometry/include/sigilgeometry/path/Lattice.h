#pragma once
/** @file
 * @ingroup geometry-path
 *
 * The scanline lattice: parallel lines cut to the inside of a set of
 * rings, and the multigrid those lines dualise into a tiling of rhombs.
 *
 * A hatch, a plotter fill, a shaded region and a mass of strokes are all
 * one construction — lines at an angle, a spacing apart, kept where they
 * are inside — and what it answers with are CENTRELINES, which a clipped
 * line pattern is not: a centreline can be walked, drawn along with a
 * natural-media tool, split, or joined to the next one.
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

  /** Value equality: the same two ends in the same order. */
  bool operator==(const LatticeMark&) const = default;
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

/** PARALLEL LINES CUT TO THE EVEN-ODD INTERIOR of @p rings: a ring
 *  inside another is a hole and two rings side by side are two islands,
 *  the rule `containsEvenOdd` answers a point with. The marks come in
 *  scan order, each running the way the lines run.
 *  @silent every ring holds fewer than three points, which bound no
 *  area; a ring's two ends are joined whether or not it says it is
 *  closed, because an area is what is being filled. */
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
   *  and only the FRACTIONAL part of one means anything.
   *  @trap Which fractions may be used together is the regularity rule
   *  `multigrid` judges: a singular set answers nothing. */
  double offset = 0;

  /** Value equality: the same direction at the same spacing and phase.
   *  Exact, as the arithmetic behind it is — a family read off a
   *  rounded offset is a different family and tiles differently. */
  bool operator==(const MultigridFamily&) const = default;
};

/** @p count FAMILIES EVENLY SPREAD, all at one spacing and one phase.
 *  The spread is a WHOLE TURN for an odd count and a HALF TURN for an
 *  even one, which is the smallest turn giving @p count distinct line
 *  directions. Five is the Penrose rhombs, four the Ammann-Beenker
 *  octagonal tiling, three the rhombille. */
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

  /** Value equality: the same two families crossing at the same two
   *  lines, over the same corners. */
  bool operator==(const MultigridRhomb&) const = default;
};

/** A MULTIGRID'S DUAL: every rhomb, over one deduplicated set of
 *  corners, so two rhombs meeting at a corner name the same vertex. */
struct MultigridTiling {
  /** Plane positions in the tiling's OWN units, in which every rhomb
   *  edge is one long whatever the lines that made it were spaced. */
  std::vector<glm::dvec2> vertices;
  std::vector<MultigridRhomb> rhombs;

  /** Value equality: the same corners and the same rhombs over them. */
  bool operator==(const MultigridTiling&) const = default;
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
 *  Bruijn's construction: each crossing of two lines becomes a rhomb
 *  whose edges are the two families' normals, placed by counting how
 *  many lines of every family stand between the crossing and the origin.
 *  Five evenly spread families give the Penrose rhombs, four the
 *  Ammann-Beenker tiling, three the rhombille. Solved in double, and the
 *  answer is in double, because a count read off a crossing by a ceiling
 *  moves a rhomb a whole edge when it falls the other way.
 *  @silent the offsets are SINGULAR — some point of the plane lies on
 *  the lines of three or more families, so no count exists there — or
 *  fewer than two families can span the plane. ALL-ZERO offsets are
 *  singular whatever the families. */
MultigridTiling multigrid(std::span<const MultigridFamily> families,
                          const MultigridOptions& options = {});

}  // namespace sigil::geometry::path
