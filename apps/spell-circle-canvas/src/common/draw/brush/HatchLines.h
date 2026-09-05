#pragma once

/** @file
 * The hatch lattice: parallel scanlines clipped to even-odd contours.
 * Private to the library; the mass gesture is built on it too.
 */

#include <include/core/SkPoint.h>
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

/** The marks of one hatch through the contours: scanlines at the style's
 *  angle and spacing, each cut where it crosses an edge, crossings paired
 *  across every contour so holes are skipped. Jitter is applied from the
 *  pen's stream; continuous joins the marks in serpentine order. */
[[nodiscard]] std::vector<HatchSegment> hatchLines(
    Pen& pen, std::span<const std::span<const SkPoint>> contours,
    const Hatch& style);

}  // namespace sigil::draw::brush
