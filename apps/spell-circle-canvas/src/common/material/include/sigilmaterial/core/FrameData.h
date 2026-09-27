#pragma once

/** @file
 * @ingroup material-core
 *
 * FrameData — the values a renderer injects into a material once per
 * frame, which no parameter struct carries because the author never sets
 * them: the clock, the surface, and the node's placement in the world.
 */

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

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
  /** The root's laid-out size in pixels — what `uResolution` becomes for
   *  a material anchored to the root. Zero falls back to `resolution`. */
  glm::vec2 rootResolution{0.0f, 0.0f};
  /** Device pixels per logical pixel; the `uContentScale` uniform. */
  float contentScale = 1.0f;
  /** The node's local space to the root, column-major; the `uWorld`
   *  uniform. Identity when the material is not anchored to the root. */
  glm::mat3 world{1.0f};
  /** The recorder the frame is drawn through, where it is drawn on a
   *  device: a texture whose pixels stand on that device is bound there
   *  as it stands. Null — a raster canvas, a recording kept to replay —
   *  reads such a texture back into host memory instead. */
  skgpu::graphite::Recorder* recorder = nullptr;
};

}  // namespace sigil::material
