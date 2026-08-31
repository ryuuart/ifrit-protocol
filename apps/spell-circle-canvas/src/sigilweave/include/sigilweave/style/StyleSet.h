#pragma once

/** @file
 * @ingroup shaping
 *
 * StyleSet — a named registry of styles: small, ordered, comparable by
 * value, whose lookup always answers.
 */

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/style/TextStyle.h"

namespace sigil::weave {

/** A NAMED REGISTRY OF STYLES — small, ordered, comparable by value.
 *
 * The levels of a log, the states a selection switches between, the roles a
 * table's columns take: a handful of styles fixed once, then addressed at
 * the point of use by a NAME. What carries the name is the content — a row,
 * a span, a cell — so the content stays a plain value and the type
 * treatment stays in one place.
 *
 * **Lookup always answers.** A name that was never registered — including
 * the empty name — resolves to the BASE entry, a default-constructed
 * TextStyle until `base()` sets one. There is no null and no failure mode:
 * a misspelled name shows as content set in the base style, never as
 * content that did not draw. Callers who must know whether a name exists
 * ask `find()`, which returns null for an absent name, or `contains()`.
 *
 * **Entries keep insertion order** and `set()` replaces in place, so a set
 * built by one call sequence is one value. Equality is exact and
 * order-sensitive — same base, same entries, same order — which is what
 * lets a StyleSet sit inside a larger comparable value and be diffed with
 * it rather than reasoned about.
 *
 * Lookup is a linear scan. A style set names a handful of roles; one large
 * enough for that to matter has stopped being a set of named styles and
 * become a document's worth of formatting. */
class StyleSet {
 public:
  using Entry = std::pair<std::string, TextStyle>;

  StyleSet() = default;
  explicit StyleSet(TextStyle baseStyle) : m_base(std::move(baseStyle)) {}

  /** The style every unregistered name resolves to. */
  StyleSet& base(TextStyle style) {
    m_base = std::move(style);
    return *this;
  }
  /** Registers @p name, replacing any entry already under it in place. */
  StyleSet& set(std::string name, TextStyle style) {
    for (Entry& e : m_entries)
      if (e.first == name) {
        e.second = std::move(style);
        return *this;
      }
    m_entries.emplace_back(std::move(name), std::move(style));
    return *this;
  }

  const TextStyle& base() const { return m_base; }
  /** The style registered under @p name, or the base entry. */
  const TextStyle& operator[](std::string_view name) const {
    const TextStyle* style = find(name);
    return style != nullptr ? *style : m_base;
  }
  /** The style registered under @p name, or null. */
  const TextStyle* find(std::string_view name) const {
    for (const Entry& e : m_entries)
      if (e.first == name) return &e.second;
    return nullptr;
  }
  bool contains(std::string_view name) const { return find(name) != nullptr; }

  /** The named entries in registration order; the base is not one of them. */
  const std::vector<Entry>& entries() const { return m_entries; }
  size_t size() const { return m_entries.size(); }
  bool empty() const { return m_entries.empty(); }

  bool operator==(const StyleSet&) const = default;

 private:
  TextStyle m_base;
  std::vector<Entry> m_entries;
};

}  // namespace sigil::weave
