#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * TypeSheet — the type half of a sheet: a base style and the named CLASSES over
 * it: small, ordered, comparable by value, whose lookup always answers.
 */

#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"

namespace sigil::weave {

/** THE TYPE HALF OF A SHEET: a base style and the named PARTIALS over it,
 * each stating what it changes while the base supplies the rest. Entries
 * keep insertion order, `set` replaces in place, and equality is exact
 * and order-sensitive, so one call sequence is one value. Lookup is a
 * linear scan over a handful of roles.
 * @trap Lookup always answers: an unregistered name — the empty one
 * included — resolves to the BASE alone rather than failing, so a
 * misspelling shows as content set in the base style. Ask
 * `TypeSheet::find` or `TypeSheet::contains` to know. */
class TypeSheet {
 public:
  using Entry = std::pair<std::string, Type>;

  TypeSheet() = default;
  explicit TypeSheet(TextStyle baseStyle) : m_base(std::move(baseStyle)) {}
  /** A sheet spelled as a literal, an entry per class:
   *  `TypeSheet{{"ts", {.size = 11}}, {"dim", {.color = grey}}}`. A name
   *  spelled twice keeps the later entry, in the earlier one's place. */
  TypeSheet(std::initializer_list<Entry> entries) {
    for (const Entry& entry : entries) set(entry.first, entry.second);
  }
  TypeSheet(TextStyle baseStyle, std::initializer_list<Entry> entries)
      : m_base(std::move(baseStyle)) {
    for (const Entry& entry : entries) set(entry.first, entry.second);
  }

  /** The style every class is resolved against, and every unregistered name
   *  resolves to by itself. */
  TypeSheet& base(TextStyle style) {
    m_base = std::move(style);
    return *this;
  }
  /** Registers @p name, replacing any entry already under it in place. */
  TypeSheet& set(std::string name, Type partial) {
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

  bool operator==(const TypeSheet&) const = default;

 private:
  TextStyle m_base;
  std::vector<Entry> m_entries;
};

}  // namespace sigil::weave
