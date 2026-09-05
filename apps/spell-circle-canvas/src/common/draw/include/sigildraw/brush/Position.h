#pragma once

/** @file
 * A field-aware cursor that walks the canvas.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>
#include <sigildraw/brush/Field.h>
#include <sigildraw/brush/Stroke.h>

#include <optional>

namespace sigil::draw::brush {

class Plot;

/** A cursor with the same accumulated-distance contract as a `Plot`: it
 *  moves through its field, or through a plot's headings, and remembers
 *  how far it has plotted. Angles are radians, clockwise-positive on the
 *  y-down canvas. With bounds, the cursor stops once it has left them by
 *  more than half their size. */
class Position {
 public:
  explicit Position(float x = 0.0f, float y = 0.0f, Direction field = {},
                    float seconds = 0.0f,
                    std::optional<SkRect> bounds = std::nullopt);

  [[nodiscard]] float x() const { return m_x; }
  [[nodiscard]] float y() const { return m_y; }
  [[nodiscard]] float plotted() const { return m_plotted; }

  /** Walks @p length in steps, each step heading the field's answer plus
   *  @p direction — @p field for this move, or the cursor's own. */
  [[nodiscard]] Stroke moveTo(float direction, float length,
                              float stepLength = 1.0f,
                              const Direction& field = {},
                              float seconds = 0.0f);
  /** Walks @p length along the plot's headings at the plotted distance,
   *  the plot scaled by @p scale, each heading turned by the field. */
  [[nodiscard]] Stroke plotTo(const Plot& plot, float length,
                              float stepLength, float scale = 1.0f);
  [[nodiscard]] float angle(const Direction& field,
                            float seconds = 0.0f) const;
  [[nodiscard]] float angle() const;
  [[nodiscard]] bool isIn() const;
  [[nodiscard]] bool isInCanvas() const;
  void place(float x, float y);
  void reset() { m_plotted = 0.0f; }

  void field(Direction value, float seconds = 0.0f);

 private:
  float m_x = 0.0f;
  float m_y = 0.0f;
  float m_plotted = 0.0f;
  Direction m_field;
  float m_seconds = 0.0f;
  std::optional<SkRect> m_bounds;
};

}  // namespace sigil::draw::brush
