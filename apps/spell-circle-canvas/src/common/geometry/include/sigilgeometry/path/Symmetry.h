#pragma once
/** @file
 * REPEATING A FIGURE INTO ITS OWN COPIES.
 *
 * A kaleidoscope, a rosette, a mandala's spokes, a mirrored ornament, a
 * tiled pattern: every one of them is the same figure drawn several
 * times under several transforms, and the only thing that differs is
 * WHICH transforms. So there is one value that says which, and it
 * answers a list of matrices — which is what makes it apply equally to a
 * path, a polyline, a point set, and to anything a caller draws itself
 * inside a save-and-restore.
 *
 * A wallpaper group is a NAMED STOCK VALUE over that one type, not a
 * type of its own. `wallpaper()` below spells the ones this value can
 * express exactly.
 */
#include <include/core/SkMatrix.h>
#include <include/core/SkPath.h>

#include <cstdint>
#include <glm/vec2.hpp>
#include <span>
#include <vector>

#include "sigilgeometry/path/Polyline.h"

namespace sigil::geometry::path {

/** THE COPIES A FIGURE IS REPEATED INTO, as numbers.
 *
 *  Three independent things multiply together: a rotation about a
 *  centre, a reflection across an axis through it, and a translation
 *  lattice. Left at its defaults the value is the identity — one copy,
 *  where the figure already is — so each field switches one repetition
 *  on without disturbing the others.
 *
 *  Every member is a plain number, so the value compares exactly and a
 *  memo keyed on one may be skipped; a symmetry chosen once for a
 *  drawing is carried as a token rather than as a loop rewritten at
 *  every call site. */
struct Symmetry {
  /** What the rotation turns about, and what the mirror axis passes
   *  through. */
  glm::vec2 centre{0, 0};
  /** How many copies the turn is divided into. One is no rotation. */
  int order = 1;
  /** Where the first copy sits, in radians. */
  float start = 0;
  /** How much of a whole turn the copies are spread over, in radians. A
   *  full turn is the rosette; less is a fan. */
  float sweep = 6.28318531f;
  /** Reflect each rotated copy as well, doubling the count. */
  bool mirror = false;
  /** The axis reflected across, in radians from +x through the centre. */
  float mirrorAngle = 0;
  /** The two steps of the translation lattice. Either one at zero length
   *  repeats nothing along that axis. */
  glm::vec2 cellU{0, 0};
  glm::vec2 cellV{0, 0};
  /** How many cells along each lattice step, counting the original. One
   *  each is no translation. */
  int repeatU = 1;
  int repeatV = 1;

  friend bool operator==(const Symmetry&, const Symmetry&) = default;
};

/** THE TRANSFORMS THEMSELVES, the identity first.
 *
 *  The order is the lattice outermost, then the rotation, then the
 *  mirror, so a caller drawing them in order lays down one whole rosette
 *  per cell rather than one spoke across every cell. */
[[nodiscard]] std::vector<SkMatrix> copies(const Symmetry& symmetry);

/** The polyline under each of those transforms, in the same order. */
[[nodiscard]] std::vector<Polyline> copies(const Symmetry& symmetry,
                                           const Polyline& line);
/** The points under each of them, all in one list — the copies run
 *  end to end, so copy `c` of point `i` is at `c * points.size() + i`. */
[[nodiscard]] std::vector<glm::vec2> copies(const Symmetry& symmetry,
                                            std::span<const glm::vec2> points);
/** ONE PATH holding every copy, which is what a fill or a stroke of the
 *  whole figure wants. */
[[nodiscard]] SkPath copies(const Symmetry& symmetry, const SkPath& path);

/** THE PLANE SYMMETRY GROUPS this value expresses exactly: those whose
 *  point group is one rotation, or one rotation and one reflection,
 *  about a single centre. The seventeen wallpaper groups also include
 *  ones built on GLIDE reflections and on centres of different orders in
 *  one cell, and a value made of one rotation and one mirror cannot
 *  spell those — so they are not named here rather than named and
 *  approximated. */
enum class Wallpaper : uint8_t {
  /** Translation only. */
  P1,
  /** Half turns. */
  P2,
  /** Third turns. */
  P3,
  /** Quarter turns. */
  P4,
  /** Sixth turns. */
  P6,
  /** One mirror. */
  Pm,
  /** Half turns and mirrors. */
  Pmm,
  /** Quarter turns and mirrors. */
  P4m,
  /** Sixth turns and mirrors. */
  P6m,
};

/** The symmetry of `group` over the lattice `cellU`, `cellV`, repeated
 *  `repeatU` by `repeatV` cells about `centre`. */
[[nodiscard]] Symmetry wallpaper(Wallpaper group, glm::vec2 cellU,
                                 glm::vec2 cellV, int repeatU = 1,
                                 int repeatV = 1, glm::vec2 centre = {0, 0});

}  // namespace sigil::geometry::path
