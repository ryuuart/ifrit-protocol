#pragma once

/** @file
 * A mass of curved hand gestures filling a polygon.
 */

#include <include/core/SkPoint.h>
#include <sigildraw/brush/Tool.h>

#include <span>

namespace sigil::draw {
class Pen;
}

namespace sigil::draw::brush {

/** Layered, curved fill gestures. Precision steadies the hand: it narrows
 *  the lane jitter and the wobble of each arc. Strength sets the number
 *  of passes, one to three, each pass after the first displaced by up to
 *  twice the tool's scatter. Gradient changes lane spacing across the
 *  shape and outline finishes the boundary with the tool. */
struct Mass {
  float precision = 0.5f;
  float strength = 1.0f;
  float gradient = 0.1f;
  bool outline = false;

  bool operator==(const Mass&) const = default;
};

/** Fills a polygon with overlapping curved gestures: spaced chords across
 *  the shape, each bent into an arc around a pivot outside it and painted
 *  only where the arc stays inside. */
void mass(Pen& pen, const Tool& tool, std::span<const SkPoint> polygon,
          const Mass& style = {});

/** Fills several even-odd contours as one family of gestures. */
void mass(Pen& pen, const Tool& tool,
          std::span<const std::span<const SkPoint>> contours,
          const Mass& style = {});

}  // namespace sigil::draw::brush
