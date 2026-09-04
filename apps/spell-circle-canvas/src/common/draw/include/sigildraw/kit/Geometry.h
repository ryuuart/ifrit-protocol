#pragma once

/** @file
 * Reusable polygons, relative plots and field-aware positions.
 */

#include <include/core/SkRect.h>
#include <sigildraw/kit/Shape.h>

#include <functional>
#include <optional>

namespace sigil::draw::brush {

class Engine;

struct Line {
  SkPoint point1{0, 0};
  SkPoint point2{0, 0};

  bool operator==(const Line&) const = default;
};

/** Stored polygon geometry with direct natural-media operations. */
class Polygon {
 public:
  Polygon() = default;
  explicit Polygon(std::vector<SkPoint> points);

  std::vector<SkPoint> vertices;
  std::vector<Line> sides;

  [[nodiscard]] std::vector<SkPoint> intersect(const Line& line) const;
  void draw(Pen& pen, const Brush& brush) const;
  void fill(Pen& pen, const Wash& style) const;
  void wash(Pen& pen, const Wash& style) const;
  void hatch(Pen& pen, const Brush& brush, const Hatch& style) const;
  void mass(Pen& pen, const Brush& brush, const Mass& style) const;
  const Polygon& draw(Pen& pen, const Engine& engine) const;
  const Polygon& fill(Pen& pen, const Engine& engine) const;
  const Polygon& wash(Pen& pen, const Engine& engine) const;
  const Polygon& hatch(Pen& pen, const Engine& engine) const;
  const Polygon& mass(Pen& pen, const Engine& engine) const;
  const Polygon& show(Pen& pen, const Engine& engine) const;
  [[nodiscard]] Polygon translated(float x, float y) const;

  [[nodiscard]] bool empty() const { return vertices.size() < 3; }
  explicit operator bool() const { return !empty(); }

 private:
  void rebuildSides();
};

enum class PlotType { Curve, Segments };

/** A path described by relative segment angles, lengths and pressures. */
class Plot {
 public:
  explicit Plot(PlotType type = PlotType::Curve, Constant units = RADIANS);

  PlotType type;
  std::vector<float> segments;
  std::vector<float> angles;
  std::vector<float> press;
  std::optional<SkPoint> origin;
  mutable std::optional<Polygon> pol;

  void addSegment(float angle, float length, float pressure = 1.0f);
  void endPlot(float angle, float pressure = 1.0f);
  void rotate(float angle);
  void rotate(float angle, Constant units);
  [[nodiscard]] float length() const;
  [[nodiscard]] float angle(float distance) const;
  [[nodiscard]] float pressure(float distance) const;
  [[nodiscard]] Stroke path(SkPoint origin = {0, 0}, float spacing = 1.0f,
                            float curvature = 0.5f, float scale = 1.0f) const;
  [[nodiscard]] Polygon genPol(float x = 0.0f, float y = 0.0f,
                               float spacing = 1.0f, float curvature = 0.5f,
                               float scale = 1.0f) const;
  void draw(Pen& pen, const Brush& brush, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  void fill(Pen& pen, const Wash& style, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  void wash(Pen& pen, const Wash& style, float x = 0.0f, float y = 0.0f,
            float scale = 1.0f) const;
  void hatch(Pen& pen, const Brush& brush, const Hatch& style, float x = 0.0f,
             float y = 0.0f, float scale = 1.0f) const;
  void mass(Pen& pen, const Brush& brush, const Mass& style, float x = 0.0f,
            float y = 0.0f, float scale = 1.0f) const;
  const Plot& draw(Pen& pen, const Engine& engine, float x = 0.0f,
                   float y = 0.0f, float scale = 1.0f) const;
  const Plot& fill(Pen& pen, const Engine& engine, float x = 0.0f,
                   float y = 0.0f, float scale = 1.0f) const;
  const Plot& wash(Pen& pen, const Engine& engine, float x = 0.0f,
                   float y = 0.0f, float scale = 1.0f) const;
  const Plot& hatch(Pen& pen, const Engine& engine, float x = 0.0f,
                    float y = 0.0f, float scale = 1.0f) const;
  const Plot& mass(Pen& pen, const Engine& engine, float x = 0.0f,
                   float y = 0.0f, float scale = 1.0f) const;
  const Plot& show(Pen& pen, const Engine& engine, float x = 0.0f,
                   float y = 0.0f, float scale = 1.0f) const;

  [[nodiscard]] static Plot fromStroke(std::span<const Sample> stroke,
                                       PlotType type = PlotType::Curve);
  explicit operator bool() const { return !segments.empty(); }

 private:
  [[nodiscard]] float normalized(float angle) const;

  float m_endAngle = 0.0f;
  float m_endPressure = 1.0f;
  float m_rotation = 0.0f;
  Constant m_angleMode = RADIANS;
};

/** Reusable circle geometry and the origin where its relative plot was first
 * drawn. */
struct PlacedPlot {
  Plot plot;
  SkPoint offset{0, 0};

  explicit operator bool() const { return static_cast<bool>(plot); }
};

/** A field-aware cursor with the same accumulated-distance contract as a
 * brush Plot. */
class Position {
 public:
  using Field = std::function<float(SkPoint, float)>;

  explicit Position(float x = 0.0f, float y = 0.0f, Field field = {},
                    float seconds = 0.0f, Constant units = RADIANS,
                    std::optional<SkRect> bounds = std::nullopt);

  float x = 0.0f;
  float y = 0.0f;
  float plotted = 0.0f;

  [[nodiscard]] Stroke moveTo(float direction, float length,
                              float stepLength = 1.0f, const Field& field = {},
                              float seconds = 0.0f);
  [[nodiscard]] Stroke plotTo(const Plot& plot, float length, float stepLength,
                              float scale = 1.0f);
  [[nodiscard]] float angle(const Field& field, float seconds = 0.0f) const;
  [[nodiscard]] float angle() const;
  [[nodiscard]] bool isIn() const;
  [[nodiscard]] bool isInCanvas() const;
  void update(float nextX, float nextY);
  void reset();

  void field(Field value, float seconds = 0.0f);

 private:
  Field m_field;
  float m_seconds = 0.0f;
  Constant m_angleMode = RADIANS;
  std::optional<SkRect> m_bounds;
};

/** Applies one hatch or mass gesture through an even-odd collection. The first
 * polygon is the outer boundary and subsequent polygons cut holes. */
void hatchArray(Pen& pen, const Brush& brush, std::span<const Polygon> polygons,
                const Hatch& style);
void massArray(Pen& pen, const Brush& brush, std::span<const Polygon> polygons,
               const Mass& style);

}  // namespace sigil::draw::brush
