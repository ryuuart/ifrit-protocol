#pragma once

/** @file
 * Which nodes a thing addresses, as a comparable value: the four terms —
 * a tag, a key, everything under a key, a material — and the three
 * combinators that build an expression out of them.
 */

#include <sigilmaterial/core/Material.h>

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::world {

/** WHAT A SELECTOR IS ASKED ABOUT: one node, as the terms read it. The
 *  ancestor keys run from the root down to the node's parent, so
 *  `sel::under("rig")` answers true for everything below the node keyed
 *  "rig" and false for that node itself. */
struct Subject {
  std::string_view key;
  std::span<const std::string> tags;
  std::span<const std::string_view> ancestorKeys;
  const ::sigil::material::Material* material = nullptr;
};

/** A SET OF NODES, described rather than enumerated.
 *
 *  A default-constructed Selector matches everything, which is what a
 *  caller that did not narrow anything means. The terms compose with
 *  `|`, `&` and `!`, and the result is a value: two selectors built the
 *  same way compare equal, so a description carrying one prunes like any
 *  other field. */
class Selector {
 public:
  /** Which question a term asks. */
  enum class Op : uint8_t {
    All,       ///< every node
    Tag,       ///< the node carries this word
    Key,       ///< the node answers to this key
    Under,     ///< an ancestor answers to this key
    Material,  ///< the node's material equals this one
    Or,
    And,
    Not,
  };

  Selector() = default;

  /** Does @p subject belong to this set? */
  [[nodiscard]] bool matches(const Subject& subject) const;

  /** The term this selector is, for a caller printing or inspecting one.
   *  A default-constructed selector answers `All`. */
  [[nodiscard]] Op op() const { return m_term ? m_term->op : Op::All; }
  /** The word a Tag, Key or Under term holds; empty for the rest. */
  [[nodiscard]] const std::string& word() const;
  /** The operands a combinator holds; empty for a leaf term. */
  [[nodiscard]] std::span<const Selector> operands() const;

  /** Value equality, term by term and operand by operand — a material
   *  term compares its material by value. */
  bool operator==(const Selector& other) const;

  /** @private the term constructors the `sel::` factories and the
   *  combinators build through. */
  static Selector leaf(Op op, std::string word);
  static Selector leaf(Op op, ::sigil::material::Material material);
  static Selector combine(Op op, std::vector<Selector> operands);

 private:
  struct Term {
    Op op = Op::All;
    std::string word;
    std::shared_ptr<const ::sigil::material::Material> material;
    std::vector<Selector> operands;
  };
  std::shared_ptr<const Term> m_term;
};

/** Either set. */
Selector operator|(Selector a, Selector b);
/** Both sets. */
Selector operator&(Selector a, Selector b);
/** Everything the set leaves out. */
Selector operator!(Selector a);

/** The terms a selector is built from. */
namespace sel {

/** Nodes carrying @p word — what `Element::tag()` put there. */
Selector tag(std::string word);
/** The node answering to @p k. */
Selector key(std::string k);
/** Everything BELOW the node answering to @p k, not that node. */
Selector under(std::string k);
/** Nodes whose material equals @p m, by value. */
Selector material(::sigil::material::Material m);

}  // namespace sel

}  // namespace sigil::world
