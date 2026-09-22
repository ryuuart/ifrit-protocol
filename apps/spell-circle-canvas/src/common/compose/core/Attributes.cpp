/** @file
 * The fact table: copy-on-write storage under the value, lookup by name,
 * and the order-free equality the reconciler compares two tables by.
 */

#include "sigilcompose/core/Attributes.h"

namespace sigil::compose {

const Attributes::Entry* Attributes::find(std::string_view name) const {
  if (!m_entries) return nullptr;
  for (const Entry& entry : *m_entries)
    if (entry.name == name) return &entry;
  return nullptr;
}

void Attributes::setEntry(std::string_view name, std::any value,
                          bool (*equals)(const std::any&, const std::any&)) {
  // Writing copies the table first: another description may share it,
  // and a description is a value that never changes behind a holder.
  std::vector<Entry> entries = m_entries ? *m_entries : std::vector<Entry>();
  for (Entry& entry : entries)
    if (entry.name == name) {
      entry.value = std::move(value);
      entry.equals = equals;
      m_entries = std::make_shared<const std::vector<Entry>>(std::move(entries));
      return;
    }
  entries.push_back(Entry{std::string(name), std::move(value), equals});
  m_entries = std::make_shared<const std::vector<Entry>>(std::move(entries));
}

void Attributes::merge(const Attributes& other) {
  if (!other.m_entries) return;
  if (!m_entries) {
    m_entries = other.m_entries;
    return;
  }
  for (const Entry& entry : *other.m_entries)
    setEntry(entry.name, entry.value, entry.equals);
}

std::vector<std::string> Attributes::names() const {
  std::vector<std::string> out;
  if (!m_entries) return out;
  out.reserve(m_entries->size());
  for (const Entry& entry : *m_entries) out.push_back(entry.name);
  return out;
}

bool Attributes::operator==(const Attributes& other) const {
  if (m_entries == other.m_entries) return true;
  if (size() != other.size()) return false;
  if (!m_entries) return true;
  for (const Entry& entry : *m_entries) {
    const Entry* match = other.find(entry.name);
    if (!match) return false;
    if (entry.value.type() != match->value.type()) return false;
    if (!entry.equals(entry.value, match->value)) return false;
  }
  return true;
}

}  // namespace sigil::compose
