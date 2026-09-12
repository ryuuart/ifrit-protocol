#pragma once

/** @file
 * @ingroup shaping
 *
 * StyleSheet — a base style and the named CLASSES over it: small, ordered,
 * comparable by value, whose lookup always answers.
 */

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"

namespace sigil::weave {

/** A BASE STYLE AND THE NAMED PARTIALS OVER IT — small, ordered,
 * comparable by value.
 *
 * The levels of a log, the states a selection switches between, the roles a
 * table's columns take: a handful of treatments fixed once, then addressed
 * at the point of use by a NAME. What carries the name is the content — a
 * row, a span, a cell — so the content stays a plain value and the type
 * treatment stays in one place.
 *
 * **An entry is a PARTIAL, not a whole style.** A class states what it
 * CHANGES — "red", "one size down", "the mono face" — and the base supplies
 * everything it is silent about. That is what lets one sheet serve a
 * document whose base size is decided elsewhere: a class that had to spell
 * the whole style would have to spell the size too, and then a page set
 * larger would take its classes at the wrong size.
 *
 * **Lookup always answers.** A name that was never registered — including
 * the empty name — resolves to the BASE alone. There is no null and no
 * failure mode: a misspelled name shows as content set in the base style,
 * never as content that did not draw. Callers who must know whether a name
 * exists ask `find()`, which returns the partial or null, or `contains()`.
 *
 * **Entries keep insertion order** and `set()` replaces in place, so a
 * sheet built by one call sequence is one value. Equality is exact and
 * order-sensitive — same base, same entries, same order — which is what
 * lets a StyleSheet sit inside a larger comparable value and be diffed with
 * it rather than reasoned about.
 *
 * Lookup is a linear scan. A style sheet names a handful of roles; one
 * large enough for that to matter has stopped being a sheet of named
 * classes and become a document's worth of formatting. */
class StyleSheet {
 public:
  using Entry = std::pair<std::string, Type>;

  StyleSheet() = default;
  explicit StyleSheet(TextStyle baseStyle) : m_base(std::move(baseStyle)) {}

  /** The style every class is resolved against, and every unregistered name
   *  resolves to by itself. */
  StyleSheet& base(TextStyle style) {
    m_base = std::move(style);
    return *this;
  }
  /** Registers @p name, replacing any entry already under it in place. */
  StyleSheet& set(std::string name, Type partial) {
    for (Entry& entry : m_entries)
      if (entry.first == name) {
        entry.second = std::move(partial);
        return *this;
      }
    m_entries.emplace_back(std::move(name), std::move(partial));
    return *this;
  }

  [[nodiscard]] const TextStyle& base() const { return m_base; }
  /** The base with @p name's partial over it, or the base alone when no
   *  such name is registered. Built per call, since a class is a partial
   *  and the style it means is not stored anywhere. */
  [[nodiscard]] TextStyle operator[](std::string_view name) const {
    const Type* partial = find(name);
    return partial != nullptr ? overlay(m_base, *partial) : m_base;
  }
  /** The partial registered under @p name, or null. */
  [[nodiscard]] const Type* find(std::string_view name) const {
    for (const Entry& entry : m_entries)
      if (entry.first == name) return &entry.second;
    return nullptr;
  }
  [[nodiscard]] bool contains(std::string_view name) const {
    return find(name) != nullptr;
  }

  /** The named entries in registration order; the base is not one of them. */
  [[nodiscard]] const std::vector<Entry>& entries() const { return m_entries; }
  [[nodiscard]] size_t size() const { return m_entries.size(); }
  [[nodiscard]] bool empty() const { return m_entries.empty(); }

  bool operator==(const StyleSheet&) const = default;

 private:
  TextStyle m_base;
  std::vector<Entry> m_entries;
};

}  // namespace sigil::weave
