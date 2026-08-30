#pragma once

/** @file
 * FrameData — the values a renderer injects into a material once per
 * frame, which no params struct carries because the author never sets
 * them: the clock, the surface, and the node's placement in the world.
 */

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

namespace sigil::material {

/** What the frame supplies. A recipe declares which of these it reads
 *  (`Recipe::frame`), and only those are uploaded — a material that reads
 *  none of them is a pure function of its parameters and can be cached
 *  across frames. */
struct FrameData {
  /** Seconds since the clock started; the `uTime` uniform. */
  double seconds = 0.0;
  /** The painted node's size in pixels; the `uResolution` uniform. */
  glm::vec2 resolution{0.0f, 0.0f};
  /** Device pixels per logical pixel; the `uContentScale` uniform. */
  float contentScale = 1.0f;
  /** The node's local space to the root, column-major; the `uWorld`
   *  uniform. Identity when the material is not anchored to the root. */
  glm::mat3 world{1.0f};
};

}  // namespace sigil::material
