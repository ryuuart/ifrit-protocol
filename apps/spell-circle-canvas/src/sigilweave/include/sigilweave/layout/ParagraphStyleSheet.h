#pragma once

/** @file
 * @ingroup layout
 *
 * `ParagraphStyleSheet` — block partials under names: what a document
 * resolves "heading" and "body" through, and the block half of a class.
 */

#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "sigilweave/layout/Block.h"

namespace sigil::weave {

/**
 * Block partials under names — the registry a document resolves "heading"
 * and "body" through, and the block half of a class.
 *
 * An entry is a `Block`: the fields that block CHANGES over what the
 * passage inherits, so one sheet serves documents whose leading or
 * alignment was decided elsewhere. `find` is the form that admits
 * absence, and a consumer decides what an absent name means. Order of
 * registration is kept and compared, which is what lets a sheet sit
 * inside a larger comparable value and be diffed with it. Lookup is a
 * linear scan: a document names a handful of styles, and a scan of a
 * handful beats a hash of one.
 */
class ParagraphStyleSheet {
 public:
  using Entry = std::pair<std::string, Block>;

  ParagraphStyleSheet() = default;
  /** A sheet spelled as a literal, an entry per class:
   *  `ParagraphStyleSheet{{"body", {.leading = Leading::multiple(1.4f)}}}`. */
  ParagraphStyleSheet(std::initializer_list<Entry> entries) {
    for (const Entry& entry : entries) set(entry.first, entry.second);
  }

  /** Registers or replaces `name`. */
  ParagraphStyleSheet& set(std::string name, Block partial) {
    for (Entry& entry : m_entries)
      if (entry.first == name) {
        entry.second = std::move(partial);
        return *this;
      }
    m_entries.emplace_back(std::move(name), std::move(partial));
    return *this;
  }
  /** The partial registered under `name`, or null. */
  [[nodiscard]] const Block* find(std::string_view name) const {
    for (const Entry& entry : m_entries)
      if (entry.first == name) return &entry.second;
    return nullptr;
  }
  [[nodiscard]] bool contains(std::string_view name) const {
    return find(name) != nullptr;
  }
  /** The entries, in registration order. */
  [[nodiscard]] std::span<const Entry> entries() const { return m_entries; }
  [[nodiscard]] size_t size() const { return m_entries.size(); }
  [[nodiscard]] bool empty() const { return m_entries.empty(); }

  bool operator==(const ParagraphStyleSheet&) const = default;

 private:
  std::vector<Entry> m_entries;
};

}  // namespace sigil::weave
