#pragma once

/** @file
 * @ingroup weave-shaping
 *
 * `Keyword` — the three things a field of a PARTIAL may be written as
 * instead of a value, and the small table that records which fields were
 * written as one. The table is generic over whatever enumerates a
 * partial's fields, so one shape serves a text style, a block, and the
 * element trees built over them.
 */

#include <cstdint>
#include <optional>
#include <vector>

namespace sigil::weave {

/** WHAT A FIELD MAY BE WRITTEN AS instead of a value — CSS's three wide
 *  keywords, each meaning something a value cannot say. A partial's
 *  unset field means "say nothing about this"; these three are
 *  statements. */
enum class Keyword : uint8_t {
  /** Take the value in force ABOVE this partial, whether or not the field
   *  is one that would arrive there on its own. */
  Inherit,
  /** Take the field's own initial value — the one it has under no
   *  ancestor at all — whatever is in force above. This is how an
   *  inherited field is stopped. */
  Initial,
  /** Whichever of the two the field's own behaviour asks for: inherit
   *  where it inherits, initial where it does not. Every field of a text
   *  style and of a block inherits, so here it is `Inherit`. */
  Unset
};

/** THE FIELDS OF ONE PARTIAL WRITTEN AS A KEYWORD, in the order they were
 *  written — @p Field being whatever enumerates that partial's fields.
 *
 *  Rare: most partials carry none, and the vector is empty and costs its
 *  own three words. Writing one twice replaces it where it stands, so the
 *  later statement wins as it does for a value.
 *
 *  A field written as a VALUE after a keyword is the later statement too,
 *  and `clear` is how the writer says so: the keyword stops standing the
 *  moment the field is written as a value again. A writer that does not
 *  call it keeps the keyword, and the two are then one layer whose order
 *  the table cannot see. */
template <class Field>
class KeywordTable {
 public:
  struct Entry {
    Field field{};
    Keyword keyword = Keyword::Unset;
    bool operator==(const Entry&) const = default;
  };

  void set(Field field, Keyword keyword) {
    for (Entry& entry : m_entries)
      if (entry.field == field) {
        entry.keyword = keyword;
        return;
      }
    m_entries.push_back({field, keyword});
  }
  /** @p field is no longer written as a keyword. The entries keep the
   *  order they were written in, so a keyword said about another field
   *  still stands where it stood. */
  void clear(Field field) {
    for (auto entry = m_entries.begin(); entry != m_entries.end(); ++entry)
      if (entry->field == field) {
        m_entries.erase(entry);
        return;
      }
  }
  /** Every entry of @p over over this table's own, later winning. */
  void overlay(const KeywordTable& over) {
    for (const Entry& entry : over.m_entries) set(entry.field, entry.keyword);
  }
  [[nodiscard]] std::optional<Keyword> find(Field field) const {
    for (const Entry& entry : m_entries)
      if (entry.field == field) return entry.keyword;
    return std::nullopt;
  }
  [[nodiscard]] const std::vector<Entry>& entries() const { return m_entries; }
  [[nodiscard]] bool empty() const { return m_entries.empty(); }
  bool operator==(const KeywordTable&) const = default;

 private:
  std::vector<Entry> m_entries;
};

}  // namespace sigil::weave
