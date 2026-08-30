/** @file
 * The pose reads over a rail and over the spline that builds one.
 */

#include "sigilgeometry/mesh/curve/Pose.h"

#include <algorithm>

#include "sigilgeometry/mesh/Vec.h"
#include "sigilgeometry/path/Numeric.h"

namespace sigil::geometry::mesh::curve {

using glm::cross;
using glm::dot;

namespace {

/** @p a and @p b blended at @p f and made an orthonormal frame again:
 *  the tangent leads, the normal is whatever of the blend survives
 *  projecting the tangent out of it, and the binormal follows. A blend
 *  of two frames that already agree returns the frame unchanged. */
Frame3 between(const Frame3& a, const Frame3& b, float f) {
  Frame3 out;
  out.position = a.position + (b.position - a.position) * f;
  out.t = a.t + (b.t - a.t) * f;
  out.tangent = normalized(a.tangent + (b.tangent - a.tangent) * f, a.tangent);
  glm::vec3 normal = a.normal + (b.normal - a.normal) * f;
  normal = normal - out.tangent * dot(normal, out.tangent);
  out.normal = normalized(normal, a.normal);
  out.binormal = cross(out.tangent, out.normal);
  return out;
}

}  // namespace

Frame3 poseAlong(const std::vector<Frame3>& rail, float distance,
                 path::Wrap policy) {
  if (rail.size() < 2) return rail.empty() ? Frame3{} : rail.front();

  // The chord length before each frame, and the total.
  std::vector<float> starts(rail.size(), 0.0f);
  for (size_t i = 1; i < rail.size(); ++i)
    starts[i] =
        starts[i - 1] + glm::length(rail[i].position - rail[i - 1].position);
  const float total = starts.back();
  if (!(total > 0)) return rail.front();

  const float want = policy == path::Wrap::Around
                         ? path::wrap(distance, total)
                         : std::clamp(distance, 0.0f, total);
  const size_t hi = (size_t)std::distance(
      starts.begin(), std::upper_bound(starts.begin(), starts.end(), want));
  const size_t b = std::clamp<size_t>(hi, 1, rail.size() - 1);
  const size_t a = b - 1;
  const float span = starts[b] - starts[a];
  const float f = span < 1e-9f ? 0.0f : (want - starts[a]) / span;
  return between(rail[a], rail[b], f);
}

Frame3 poseAlong(const Spline3& spline, float distance, path::Wrap policy,
                 int samples, glm::vec3 up) {
  // An open curve has no seam to come round through, so it parks even
  // when the caller asked to wrap.
  const path::Wrap effective = spline.closed ? policy : path::Wrap::Clamp;
  return poseAlong(frames(spline, samples, up), distance, effective);
}

}  // namespace sigil::geometry::mesh::curve
