#pragma once

/** @file
 * THE PAVEMENT'S CONSTRUCTION, as the marble-workers set it out: a course
 * of tesserae is one cut piece repeated over a lattice, a roundel's ring is
 * one lozenge turned about its centre, and the guilloche is one band that
 * loops every roundel of a quincunx and crosses itself between them. Each
 * is a comparable value answering a path, so the nodes that wear them
 * settle once they are laid.
 */

#include <include/core/SkPath.h>
#include <include/core/SkSize.h>
#include <sigilgeometry/path/Outline.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <vector>

namespace cosmati {

using Points = std::vector<glm::vec2>;

/** A COURSE OF TESSERAE: one cut piece, given as its corners about the
 *  lattice origin, repeated at every cell `origin + i·across + j·down`
 *  that reaches into the box. A course of several stones is several
 *  courses over one lattice, each taking the cells where
 *  `(i − j) mod every == offset`, so neighbouring pieces differ.
 *  `loss` is the share of pieces worn out of the bed, chosen per cell
 *  from `seed`, which is how a floor walked on for centuries reads. */
struct Course {
  Points tessera;
  glm::vec2 across{0, 0};
  glm::vec2 down{0, 0};
  glm::vec2 origin{0, 0};
  int every = 1;
  int offset = 0;
  float loss = 0.0f;
  uint32_t seed = 0;

  bool operator==(const Course&) const = default;
  sigil::geometry::path::Outline outline(glm::vec2 size) const;
};

/** The triangle pointing up and the one pointing down of a triangular
 *  course whose pieces are @p side wide and @p rise tall, about their
 *  lattice origin; the lattice is `{side, 0}` across and
 *  `{side / 2, rise}` down. */
Points upTriangle(float side, float rise);
Points downTriangle(float side, float rise);
/** A square set on its point, @p half from its centre to each corner;
 *  its lattice is `{half, half}` across and `{-half, half}` down. */
Points squareOnPoint(float half);

/** A ROUNDEL'S RING COURSE: @p count lozenges on a ring about the box's
 *  centre, each pointing outward from @p inner to @p outer — both
 *  fractions of the box's half side — and turned by @p phase radians. A
 *  lozenge is @p width of its angular step wide at its waist. */
struct Rosette {
  int count = 8;
  float inner = 0.3f;
  float outer = 0.6f;
  float phase = 0.0f;
  float width = 0.84f;

  bool operator==(const Rosette&) const = default;
  sigil::geometry::path::Outline outline(glm::vec2 size) const;
};

/** THE GUILLOCHE OF A QUINCUNX: the one band's centreline, which runs
 *  round the centre loop, leaves it on a tangent, crosses between the
 *  loops, goes round the far side of a satellite and comes back across
 *  itself, four times. Every crossing is a knot: the band's outgoing
 *  stretch passes over its returning one. */
struct Interlace {
  SkPath spine;
  float length = 0;
  struct Knot {
    glm::vec2 at{0, 0};
    /** The two straight stretches that cross there, each end to end. */
    SkPath over, under;
    /** Where the over stretch starts and ends along the spine, in px. */
    float overFrom = 0, overTo = 0;
  };
  std::vector<Knot> knots;
};

/** The band about @p centre: its centre loop of radius @p centreLoop, and
 *  four satellite loops of radius @p satelliteLoop at @p distance, the
 *  first at @p firstAngle radians and the rest a quarter turn apart
 *  sunwise. The loops must stand clear of one another
 *  (`centreLoop + satelliteLoop < distance`) for the band to cross
 *  between them. */
Interlace quincunxInterlace(glm::vec2 centre, float centreLoop,
                            float satelliteLoop, float distance,
                            float firstAngle);

}  // namespace cosmati
