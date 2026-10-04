#pragma once

/** @file
 * THE FRINGE: the warp left unwoven past the last pick, drawn as one
 * outline of knotted tassels.
 */

#include <sigilcompose/kit/Specimen.h>
#include <sigilcore/compute/Noise.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace {

/** The fringe: the warp left unwoven past the last pick, knotted four
 *  ends to a tassel. Each tassel runs from the fell into an overhand knot
 *  a little wider than the strand below it, then hangs free to a cut end;
 *  its length and its swing wander from tassel to tassel, as hand-tied
 *  ones do. The outline starts @p hidden pixels behind the cloth so the
 *  ends pass under its edge, and its bounds are the box it is drawn in —
 *  `height` is the longest tassel's reach — because an SVG outline is
 *  stretched from its bounds onto its box. A warp end is @p thread pixels
 *  wide. */
struct Fringe {
  std::string outline;
  float height = 0;
};

Fringe tassels(int ends, float hidden, float length, float thread) {
  constexpr int kEndsPerTassel = 4;
  const float pitch = kEndsPerTassel * thread;
  Fringe fringe;
  for (int tassel = 0; tassel * kEndsPerTassel < ends; ++tassel) {
    const auto index = (uint32_t)tassel;
    const float left = (float)tassel * pitch, right = left + pitch;
    const float centre = left + pitch / 2;
    const float knot = hidden + 2;
    const float tip =
        hidden + length - 3.5f * (sigil::core::noise::hash(11, index) + 1);
    const float swing = 1.1f * sigil::core::noise::hash(23, index);
    const float hang = centre + swing;
    fringe.outline += sigil::compose::kit::formatted(
        "M%g 0 L%g 0 C%g %g %g %g %g %g C%g %g %g %g %g %g "
        "L%g %g L%g %g L%g %g L%g %g L%g %g L%g %g "
        "C%g %g %g %g %g %g C%g %g %g %g %g 0 Z ",
        left, right,
        // From the fell the four ends gather into the knot…
        right, knot - 2, centre + 3, knot - 1, centre + 2.2f, knot,
        // …which swells round the turn of the tie…
        centre + 3.7f, knot + 1, centre + 3.7f, knot + 3.5f, centre + 2,
        knot + 4.5f,
        // …and the strand hangs, flaring a little, to a ragged cut end.
        hang + 2.7f, tip - 1.6f, hang + 1.3f, tip, hang + 0.3f, tip - 1.1f,
        hang - 0.9f, tip - 0.2f, hang - 2.7f, tip - 1.4f, centre - 2,
        knot + 4.5f, centre - 3.7f, knot + 3.5f, centre - 3.7f, knot + 1,
        centre - 2.2f, knot, centre - 3, knot - 1, left, knot - 2, left);
    fringe.height = std::max(fringe.height, tip);
  }
  return fringe;
}

}  // namespace
