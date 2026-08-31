/** @file
 * The pose reads: one contour addressed by distance, and a list of them
 * walked as one coordinate.
 */

#include "sigilgeometry/path/Pose.h"

#include <algorithm>

#include "sigilgeometry/path/Numeric.h"

namespace sigil::geometry::path {

namespace {

/** The pose at an ALREADY-RESOLVED distance — the policy has run and
 *  @p distance sits inside the contour. */
Pose poseAt(const Contour& contour, float distance) {
  const std::optional<Contour::Sample> sample = contour.at(distance);
  if (!sample) return {};
  Pose out;
  out.position = sample->position;
  out.tangent = sample->tangent;
  // Right of travel in Skia's y-down space: the tangent turned toward
  // +y, which is the side a positive sideways offset lies on.
  out.normal = {-sample->tangent.y, sample->tangent.x};
  out.distance = distance;
  return out;
}

}  // namespace

Pose poseAlong(const Contour& contour, float distance, Wrap policy) {
  const float len = contour.length();
  if (!(len > 0)) return {};
  const float d = policy == Wrap::Around && contour.closed()
                      ? wrap(distance, len)
                      : std::clamp(distance, 0.0f, len);
  return poseAt(contour, d);
}

float totalLength(std::span<const Contour> contours) {
  float total = 0;
  for (const Contour& c : contours) total += c.length();
  return total;
}

bool closedThroughout(std::span<const Contour> contours) {
  if (contours.empty()) return false;
  for (const Contour& c : contours)
    if (!c.closed()) return false;
  return true;
}

Pose poseAlong(std::span<const Contour> contours, float distance, Wrap policy) {
  if (contours.empty()) return {};
  const float total = totalLength(contours);
  if (!(total > 0)) return {};
  const float want = policy == Wrap::Around && closedThroughout(contours)
                         ? wrap(distance, total)
                         : std::clamp(distance, 0.0f, total);

  // The piece the distance lands in, and where it lands inside it. The
  // cumulative starts are summed in contour order and in the same
  // arithmetic the total was, so the two agree at every boundary.
  size_t index = contours.size() - 1;
  float start = 0;
  float running = 0;
  for (size_t c = 0; c < contours.size(); ++c) {
    if (c + 1 < contours.size() && want < running + contours[c].length()) {
      index = c;
      start = running;
      break;
    }
    start = running;
    running += contours[c].length();
  }
  const float len = contours[index].length();
  const float local = std::clamp(want - start, 0.0f, len);
  Pose out = poseAt(contours[index], local);
  out.distance = start + local;
  return out;
}

}  // namespace sigil::geometry::path
