/** @file
 * The layout schemes whose arithmetic steps through SigilGeometry's
 * arrangement and contour bodies: the ring, the path walk and the
 * jittered grid, placed out of line so the kit's header names no
 * renderer type.
 */

#include "sigilcompose/kit/Layouts.h"

#include <sigilgeometry/path/Arrange.h>
#include <sigilgeometry/path/Contour.h>
#include <sigilgeometry/path/Numeric.h>
#include <sigilgeometry/path/Skia.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace sigil::compose::layouts {

void Radial::arrange(Arrangement& arrangement) const {
  const size_t n = arrangement.children.size();
  if (n == 0) return;
  const float cx = arrangement.box.width() / 2;
  const float cy = arrangement.box.height() / 2;
  auto frac = [&](size_t i) {
    return i < radiusAt.size() ? radiusAt[i] : radiusFraction;
  };
  // A full circle spaces n children evenly (endpoint excluded); a
  // partial sweep includes both endpoints. The test is made in degrees,
  // the unit the author stated the sweep in.
  const bool closed = std::abs(std::abs(sweepDeg) - 360.0f) < 1e-3f;
  const geometry::arrange::Turn turn = closed
                                           ? geometry::arrange::Turn::Closed
                                           : geometry::arrange::Turn::Open;
  const float start = startDeg * geometry::path::kDegToRad;
  const float sweep = sweepDeg * geometry::path::kDegToRad;
  const float count = divisions > 0 ? divisions : (float)n;
  for (size_t i = 0; i < n; ++i) {
    Arrangement::Child& child = arrangement.children[i];
    const float r = frac(i);
    const std::optional<float> fact =
        lane.empty() ? std::nullopt : child.number(lane);
    float angle;
    if (fact) {
      // A fact's fraction of the sweep: a closed ring divides into
      // `count` steps, an open fan into `count - 1` so the last fact
      // reaches the far end exactly as the last index does.
      const float steps = closed ? count : std::max(count - 1.0f, 1.0f);
      angle = start + sweep * (*fact / steps);
    } else {
      angle = geometry::arrange::along(start, sweep, i, n, turn);
    }
    child.centreAt(geometry::path::fromSk(
        geometry::arrange::onEllipse({cx, cy}, {cx * r, cy * r}, angle)));
    // Standing along the radius: a child at twelve o'clock (−90°) is
    // upright, one at three o'clock turned a quarter clockwise.
    if (facing) child.turn(angle * geometry::path::kRadToDeg + 90.0f);
  }
}

void AlongPath::arrange(Arrangement& arrangement) const {
  const size_t n = arrangement.children.size();
  if (n == 0 || !path) return;
  const std::vector<geometry::path::Contour> contours =
      geometry::path::Contour::of(
          geometry::path::toSk(path(arrangement.box.size())));
  if (contours.empty()) return;
  const geometry::path::Contour& contour = contours.front();
  const float length = contour.length();
  const float d0 = length * startFraction;
  const float d1 = length * endFraction;
  // Closed stretches exclude the duplicate endpoint; open ones hit
  // both ends. Arc length divides among n children exactly as an angle
  // does around a ring, so the same run arithmetic answers both.
  const bool loop =
      contour.closed() && startFraction == 0.0f && endFraction == 1.0f;
  const geometry::arrange::Turn turn =
      loop ? geometry::arrange::Turn::Closed : geometry::arrange::Turn::Open;
  for (size_t i = 0; i < n; ++i) {
    const auto sample =
        contour.at(geometry::arrange::along(d0, d1 - d0, i, n, turn));
    if (!sample) continue;
    arrangement.children[i].centreAt(sample->position);
    if (facing)
      arrangement.children[i].turn(
          std::atan2(sample->tangent.y, sample->tangent.x) *
          geometry::path::kRadToDeg);
  }
}

std::vector<geometry::path::Rect> Jittered::place(const LayoutInput& in) const {
  const size_t n = in.childSizes.size();
  std::vector<geometry::path::Rect> rects(n);
  if (n == 0) return rects;
  const int cols = (int)std::ceil(std::sqrt((float)n));
  const int rows = (int)std::ceil((float)n / (float)cols);
  // The regular grid the jitter is measured against is the same grid a
  // modular layout lays down: gapless modules filling the container.
  const auto module = geometry::arrange::moduleSize(
      {in.container.x, in.container.y}, cols, rows, {0, 0});
  for (size_t i = 0; i < n; ++i) {
    const float jx = core::noise::hash(seed, (uint32_t)(i * 2)) * jitter *
                     module.width() / 2;
    const float jy = core::noise::hash(seed, (uint32_t)(i * 2 + 1)) * jitter *
                     module.height() / 2;
    const auto cell = geometry::arrange::cellRect(
                            geometry::arrange::cellAt(i, cols), module)
                            .center();
    // Clamped into the container so jitter never clips children away.
    rects[i] = heldInside(geometry::path::Rect::centredOn(
                              {cell.x() + jx, cell.y() + jy}, in.childSizes[i]),
                          in.container);
  }
  return rects;
}

}  // namespace sigil::compose::layouts
