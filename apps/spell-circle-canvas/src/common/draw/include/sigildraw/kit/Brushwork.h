#pragma once

/** @file
 * The procedural brush kit: tools, paths, fields and natural-media interiors.
 */

#include <sigildraw/kit/Box.h>
#include <sigildraw/kit/Brush.h>
#include <sigildraw/kit/Dab.h>
#include <sigildraw/kit/Engine.h>
#include <sigildraw/kit/Fields.h>
#include <sigildraw/kit/Geometry.h>
#include <sigildraw/kit/Path.h>
#include <sigildraw/kit/Shape.h>

#include <initializer_list>
#include <optional>
#include <utility>

namespace sigil::draw::brush {

/** Chooses a value in proportion to its positive weight using the pen's
 * deterministic random stream. An empty or non-positive distribution has no
 * result. */
template <typename T>
[[nodiscard]] std::optional<T> wRand(
    Pen& pen, std::initializer_list<std::pair<T, float>> weights) {
  float total = 0.0f;
  for (const auto& [unused, weight] : weights)
    if (weight > 0.0f) total += weight;
  if (!(total > 0.0f)) return std::nullopt;

  const float choice = pen.random(total);
  float cumulative = 0.0f;
  std::optional<T> last;
  for (const auto& [value, weight] : weights) {
    if (!(weight > 0.0f)) continue;
    last = value;
    cumulative += weight;
    if (choice < cumulative) return value;
  }
  return last;
}

}  // namespace sigil::draw::brush
