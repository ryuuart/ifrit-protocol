#pragma once

/** @file
 * An instance-owned catalogue of named tools.
 */

#include <sigildraw/brush/Tool.h>

#include <boost/container_hash/hash.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::draw::brush {

/** The hash of a tool's name, taking it as the view a caller holds it in,
 *  so a lookup by `std::string_view` copies nothing. */
struct NameHash {
  using is_transparent = void;
  using is_avalanching = void;
  size_t operator()(std::string_view name) const noexcept {
    return boost::hash<std::string_view>{}(name);
  }
};

/** Named tools, with no process-global selection or mutable state. */
class Catalogue {
 public:
  /** The thirteen stock names: `2B`, `HB`, `2H`, `cpencil`, `pen`,
   *  `rotring`, `spray`, `marker`, `marker2`, `charcoal`, `hatch_brush`,
   *  `pastel` and `crayon`. */
  [[nodiscard]] static Catalogue stock();

  /** Adds or replaces a definition and answers it; an empty name is not
   *  a name, and answers null. */
  const Tool* add(std::string name, Tool tool);
  [[nodiscard]] const Tool* find(std::string_view name) const;
  [[nodiscard]] bool contains(std::string_view name) const;
  [[nodiscard]] std::vector<std::string> names() const;

  /** Scales every definition's width, scatter and spacing. */
  void scale(float factor);

 private:
  boost::unordered_node_map<std::string, Tool, NameHash, std::equal_to<>>
      m_tools;
};

}  // namespace sigil::draw::brush
