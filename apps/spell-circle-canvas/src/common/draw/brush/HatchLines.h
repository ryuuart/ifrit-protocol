#pragma once

/** @file
 * The hatch lattice as marks a tool is drawn along: SigilGeometryPath's
 * scanline fill, with the pen's jitter and the serpentine join over it.
 * Private to the library; the mass gesture is built on it too.
 */

#include <include/core/SkPoint.h>
#include <sigilgeometry/path/Polyline.h>
#include <sigildraw/brush/Hatch.h>

#include <span>
#include <vector>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

struct HatchSegment {
  SkPoint from;
  SkPoint to;
  /** A serpentine join between two marks rather than a mark. */
  bool connector = false;
};

/** The marks of one hatch through the rings: the lattice at the style's
 *  angle and spacing, its gaps opened or crowded by the style's gradient,
 *  cut to the even-odd interior. Jitter is applied from the pen's stream
 *  AFTER the cut, so a jittered mark may cross an edge; continuous joins
 *  the marks in serpentine order. */
[[nodiscard]] std::vector<HatchSegment> hatchLines(
    Pen& pen, std::span<const geometry::path::Polyline> rings,
    const Hatch& style);

}  // namespace sigil::draw::brush
