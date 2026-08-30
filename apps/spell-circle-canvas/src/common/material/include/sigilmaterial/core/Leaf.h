#pragma once

/** @file
 * Leaf — a child that no recipe computes: an image with its sampling, a
 * rendered frame, anything a backend binds into a slot directly rather
 * than compiling. The core carries it as a comparable handle so a
 * material tree can hold one, prune on it and report whether it moves;
 * what it is and how it binds are the backend's to know.
 */

#include <memory>
#include <typeinfo>

namespace sigil::material {

/** A slot filler without a recipe. A subclass is defined beside the
 *  backend feature that can bind it (a Skia image shader, say) and
 *  compares itself by value so two materials holding equal leaves
 *  compare equal. The base implements equality as same dynamic type
 *  followed by `equals()`, so a subclass compares only against its own
 *  kind. */
class Leaf {
 public:
  virtual ~Leaf() = default;

  /** Whether this leaf can change between frames with no edit to the
   *  material holding it — a frame sequence, a live surface. */
  virtual bool animated() const { return false; }

  bool operator==(const Leaf& other) const {
    return typeid(*this) == typeid(other) && equals(other);
  }

 protected:
  /** Value equality against @p other, which is of this dynamic type. */
  virtual bool equals(const Leaf& other) const = 0;
};

}  // namespace sigil::material
