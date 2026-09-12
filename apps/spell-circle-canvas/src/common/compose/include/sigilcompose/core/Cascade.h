#pragma once

/** @file
 * The custom properties a node sets and its descendants read: the value a
 * property holds and the table the properties in force at a node make.
 * The inherited font and ink are `Element::font` and `Element::ink`; what
 * a node RESOLVES to is read at paint through `PaintContext::font`,
 * `PaintContext::ink` and `PaintContext::vars`.
 */

#include <include/core/SkColor.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Var.h>

#include <utility>
#include <variant>
#include <vector>

namespace sigil::compose {

/** WHAT A CUSTOM PROPERTY HOLDS: a colour, read by `Fill::var` and
 *  `Element::ink(var(...))`; or a length, read wherever a `Dimension` is
 *  written as `var(...)`. A length may itself be relative and resolves
 *  where it is read, against the font in force there. */
using VarValue = std::variant<SkColor4f, Dimension>;

/** THE CUSTOM PROPERTIES IN FORCE AT A NODE — every name an ancestor set,
 *  the nearest ancestor winning, as one comparable value. Small and
 *  ordered: a look names a handful of properties, and a scan of a handful
 *  beats a hash of one. */
class VarTable {
 public:
  using Entry = std::pair<VarRef, VarValue>;

  /** Sets @p name, replacing an entry already under it in place. */
  void set(VarRef name, VarValue value) {
    for (Entry& entry : m_entries)
      if (entry.first == name) {
        entry.second = std::move(value);
        return;
      }
    m_entries.emplace_back(name, std::move(value));
  }
  /** Every entry of @p over written over this table. */
  void overlay(const VarTable& over) {
    for (const Entry& entry : over.m_entries) set(entry.first, entry.second);
  }
  /** The value under @p name, or null when no ancestor set it. */
  [[nodiscard]] const VarValue* find(VarRef name) const {
    for (const Entry& entry : m_entries)
      if (entry.first == name) return &entry.second;
    return nullptr;
  }
  [[nodiscard]] const std::vector<Entry>& entries() const { return m_entries; }
  [[nodiscard]] bool empty() const { return m_entries.empty(); }
  bool operator==(const VarTable&) const = default;

 private:
  std::vector<Entry> m_entries;
};

}  // namespace sigil::compose
