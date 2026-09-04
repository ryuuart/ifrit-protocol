#pragma once

/** @file
 * An instance-owned catalogue of brush definitions.
 */

#include <sigildraw/kit/Brush.h>

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sigil::draw::brush {

/** A brush catalogue with no process-global selection or mutable state. */
class Box {
 public:
  /** Forms a box containing the stock p5.brush tool names. */
  [[nodiscard]] static Box stock();

  /** Adds or replaces a definition. Empty names are rejected. */
  bool add(std::string name, Brush brush);
  [[nodiscard]] const Brush* find(std::string_view name) const;
  [[nodiscard]] bool contains(std::string_view name) const;
  [[nodiscard]] std::vector<std::string> names() const;

  /** Scales the spatial properties of every definition. */
  void scale(float factor);

 private:
  std::unordered_map<std::string, Brush> m_brushes;
};

}  // namespace sigil::draw::brush
