/** @file
 * The transforms a symmetry stands for, and the groups that are stock
 * values over it.
 */
#include "sigilgeometry/path/Symmetry.h"

#include <include/core/SkPathBuilder.h>

#include <algorithm>
#include <cmath>

namespace sigil::geometry::path {

std::vector<SkMatrix> copies(const Symmetry& symmetry) {
  const int spokes = std::max(symmetry.order, 1);
  const int alongU = std::max(symmetry.repeatU, 1);
  const int alongV = std::max(symmetry.repeatV, 1);
  const int mirrors = symmetry.mirror ? 2 : 1;

  std::vector<SkMatrix> matrices;
  matrices.reserve((size_t)spokes * (size_t)mirrors * (size_t)alongU *
                   (size_t)alongV);

  // The reflection across a line at `mirrorAngle` through the origin,
  // which is the rotation to that line, a flip in y, and the rotation
  // back — written out, because three multiplications of a matrix would
  // round where four exact cosines do not.
  const float twice = 2.0f * symmetry.mirrorAngle;
  SkMatrix reflect;
  reflect.setAll(std::cos(twice), std::sin(twice), 0, std::sin(twice),
                 -std::cos(twice), 0, 0, 0, 1);

  // A sweep of a whole turn puts the last copy one step short of the
  // first rather than on top of it; a partial sweep puts a copy on each
  // end of it. Both ends occupied is what a fan means.
  const bool wholeTurn =
      std::abs(std::abs(symmetry.sweep) - 6.28318531f) < 1e-4f;
  const float divisor =
      spokes > 1 ? (float)(wholeTurn || spokes == 1 ? spokes : spokes - 1)
                 : 1.0f;

  for (int v = 0; v < alongV; ++v)
    for (int u = 0; u < alongU; ++u) {
      const glm::vec2 offset =
          symmetry.cellU * (float)u + symmetry.cellV * (float)v;
      for (int spoke = 0; spoke < spokes; ++spoke) {
        const float angle =
            symmetry.start + symmetry.sweep * (float)spoke / divisor;
        for (int flip = 0; flip < mirrors; ++flip) {
          SkMatrix matrix = SkMatrix::Translate(offset.x, offset.y);
          matrix.preTranslate(symmetry.centre.x, symmetry.centre.y);
          matrix.preRotate(angle * 180.0f / 3.14159265f);
          if (flip == 1) matrix.preConcat(reflect);
          matrix.preTranslate(-symmetry.centre.x, -symmetry.centre.y);
          matrices.push_back(matrix);
        }
      }
    }
  return matrices;
}

std::vector<Polyline> copies(const Symmetry& symmetry, const Polyline& line) {
  const std::vector<SkMatrix> matrices = copies(symmetry);
  std::vector<Polyline> lines;
  lines.reserve(matrices.size());
  for (const SkMatrix& matrix : matrices) {
    Polyline copy = line;
    for (glm::vec2& point : copy.points) {
      const SkPoint mapped = matrix.mapPoint({point.x, point.y});
      point = {mapped.fX, mapped.fY};
    }
    lines.push_back(std::move(copy));
  }
  return lines;
}

std::vector<glm::vec2> copies(const Symmetry& symmetry,
                              std::span<const glm::vec2> points) {
  const std::vector<SkMatrix> matrices = copies(symmetry);
  std::vector<glm::vec2> mapped;
  mapped.reserve(matrices.size() * points.size());
  for (const SkMatrix& matrix : matrices)
    for (const glm::vec2 point : points) {
      const SkPoint at = matrix.mapPoint({point.x, point.y});
      mapped.push_back({at.fX, at.fY});
    }
  return mapped;
}

SkPath copies(const Symmetry& symmetry, const SkPath& path) {
  SkPathBuilder builder;
  for (const SkMatrix& matrix : copies(symmetry)) builder.addPath(path, matrix);
  return builder.detach();
}

Symmetry wallpaper(Wallpaper group, glm::vec2 cellU, glm::vec2 cellV,
                   int repeatU, int repeatV, glm::vec2 centre) {
  Symmetry symmetry;
  symmetry.centre = centre;
  symmetry.cellU = cellU;
  symmetry.cellV = cellV;
  symmetry.repeatU = repeatU;
  symmetry.repeatV = repeatV;
  switch (group) {
    case Wallpaper::P1:
      break;
    case Wallpaper::P2:
      symmetry.order = 2;
      break;
    case Wallpaper::P3:
      symmetry.order = 3;
      break;
    case Wallpaper::P4:
      symmetry.order = 4;
      break;
    case Wallpaper::P6:
      symmetry.order = 6;
      break;
    case Wallpaper::Pm:
      symmetry.mirror = true;
      break;
    case Wallpaper::Pmm:
      symmetry.order = 2;
      symmetry.mirror = true;
      break;
    case Wallpaper::P4m:
      symmetry.order = 4;
      symmetry.mirror = true;
      break;
    case Wallpaper::P6m:
      symmetry.order = 6;
      symmetry.mirror = true;
      break;
  }
  return symmetry;
}

}  // namespace sigil::geometry::path
