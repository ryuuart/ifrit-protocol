#pragma once

/** @file
 * @ingroup draw-brush
 *
 * A path described relative to wherever it is drawn: segment angles,
 * lengths and pressures.
 */

#include <include/core/SkPoint.h>
#include <sigildraw/brush/Polygon.h>
#include <sigildraw/brush/Stroke.h>

#include <span>
#include <vector>

namespace sigil::draw::brush {

/** Whether a plot's samples are joined by a curve through them or by
 *  straight segments. */
enum class PlotType { Curve, Segments };

/** A plot is always relative: its angles are radians, clockwise-positive
 *  on the y-down canvas, and it takes its origin and scale from every
 *  call that draws it. */
class Plot {
 public:
  Plot() = default;
  explicit Plot(PlotType type) : m_type(type) {}

  [[nodiscard]] PlotType type() const { return m_type; }
  void addSegment(float angle, float length, float pressure = 1.0f);
  void endPlot(float angle, float pressure = 1.0f);
  /** Turns the whole plot. */
  void rotate(float angle) { m_rotation = angle; }
  [[nodiscard]] float length() const;
  /** The heading and the pressure at a distance along the plot. */
  [[nodiscard]] float angle(float distance) const;
  [[nodiscard]] float pressure(float distance) const;
  [[nodiscard]] bool empty() const { return m_segments.empty(); }

  /** The plot placed at @p origin and scaled, sampled at @p spacing —
   *  through a curve of @p curvature, or straight when the type is
   *  Segments. */
  [[nodiscard]] Stroke path(SkPoint origin = {0, 0}, float spacing = 1.0f,
                            float curvature = 0.5f, float scale = 1.0f) const;
  /** The same as a polygon. */
  [[nodiscard]] Polygon polygon(float x = 0.0f, float y = 0.0f,
                                float spacing = 1.0f, float curvature = 0.5f,
                                float scale = 1.0f) const;

  /** The stroke's segments as a plot relative to its first sample. */
  [[nodiscard]] static Plot fromStroke(std::span<const Sample> stroke,
                                       PlotType type = PlotType::Curve);

 private:
  PlotType m_type = PlotType::Curve;
  std::vector<float> m_segments;
  std::vector<float> m_angles;
  std::vector<float> m_pressures;
  float m_endAngle = 0.0f;
  float m_endPressure = 1.0f;
  float m_rotation = 0.0f;
};

/** A plot and the origin it was first drawn at, so an engine's circle,
 *  arc, spline or shape can be replayed where it was or anywhere else. */
struct PlacedPlot {
  Plot plot;
  SkPoint origin{0, 0};

  [[nodiscard]] bool empty() const { return plot.empty(); }
};

}  // namespace sigil::draw::brush
