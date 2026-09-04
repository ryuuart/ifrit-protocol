#pragma once

/** @file
 * Natural-media interiors for polygonal shapes.
 *
 * Hatching and washes take geometry as data, so the same polygon can receive
 * several treatments without rebuilding it or changing global drawing state.
 */

#include <include/core/SkColor.h>
#include <include/core/SkPoint.h>
#include <sigildraw/Constants.h>
#include <sigildraw/Pen.h>
#include <sigildraw/kit/Brush.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace sigil::draw::brush {

enum class BleedDirection { Out, In };

/** Parallel brush marks clipped to a polygon. Angles are radians, jitter is a
 * fraction of spacing and gradient changes spacing across the shape. */
struct Hatch {
  float spacing = 5.0f;
  float angle = QUARTER_PI;
  float jitter = 0.0f;
  float gradient = 0.0f;
  bool continuous = false;

  bool operator==(const Hatch&) const = default;
};

/** A layered translucent pigment wash. Bleed and texture are unit values;
 * border controls pigment gathered at the edge and layers controls how many
 * independently perturbed deposits form the body. */
struct Wash {
  SkColor4f color{0, 0, 0, 1};
  float opacity = 150.0f / 255.0f;
  float bleed = 0.07f;
  float texture = 0.8f;
  float border = 0.5f;
  bool scatter = true;
  BleedDirection bleedDirection = BleedDirection::Out;
  std::optional<float> bleedAngle;
  int layers = 18;
  Constant blend = MULTIPLY;

  bool operator==(const Wash&) const = default;
};

/** Layered, curved hand-fill gestures. Precision controls the displacement of
 * each gesture, strength controls the number of passes, gradient changes lane
 * spacing across the shape and outline finishes the boundary with the tool. */
struct Mass {
  float precision = 0.5f;
  float strength = 1.0f;
  float gradient = 0.1f;
  bool outline = false;

  bool operator==(const Mass&) const = default;
};

/** Paints parallel marks through the polygon and restores the pen state. */
void hatch(Pen& pen, const Brush& tool, std::span<const SkPoint> polygon,
           const Hatch& style = {});

/** Paints one hatch gesture through several even-odd contours. Crossings are
 * paired across the whole collection, so inner contours cut holes and
 * disjoint contours remain separate islands. */
void hatch(Pen& pen, const Brush& tool,
           std::span<const std::span<const SkPoint>> contours,
           const Hatch& style = {});

/** Builds translucent, softly displaced layers around and within the polygon,
 * adds granulation and edge pooling, then restores the pen state. */
void wash(Pen& pen, const Wash& pigment, std::span<const SkPoint> polygon);

/** Fills a polygon with overlapping curved brush gestures. */
void mass(Pen& pen, const Brush& tool, std::span<const SkPoint> polygon,
          const Mass& style = {});

/** Fills several even-odd contours as one family of curved brush gestures. */
void mass(Pen& pen, const Brush& tool,
          std::span<const std::span<const SkPoint>> contours,
          const Mass& style = {});

/** Subdivides and displaces a polygon through a direction field. The returned
 * centreline is closed by repeating its first sample. */
template <DirectionField Field>
Stroke warp(std::span<const SkPoint> polygon, float spacing, float amount,
            float seconds, const Field& field, float pressure = 1.0f) {
  Stroke result;
  if (polygon.size() < 2) return result;
  spacing = std::max(0.125f, spacing);
  for (size_t edge = 0; edge < polygon.size(); ++edge) {
    const SkPoint from = polygon[edge];
    const SkPoint to = polygon[(edge + 1) % polygon.size()];
    const float dx = to.fX - from.fX;
    const float dy = to.fY - from.fY;
    const float length = std::hypot(dx, dy);
    const int steps = std::max(1, (int)std::ceil(length / spacing));
    for (int step = 0; step < steps; ++step) {
      const float t = (float)step / (float)steps;
      SkPoint point{from.fX + dx * t, from.fY + dy * t};
      const float direction = (float)field(point, seconds);
      point.fX += std::cos(direction) * amount;
      point.fY += std::sin(direction) * amount;
      result.push_back({point, pressure});
    }
  }
  result.push_back(result.front());
  return result;
}

}  // namespace sigil::draw::brush
