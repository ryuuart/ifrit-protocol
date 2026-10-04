/** @file
 * The fact table: copy-on-write storage under the value, lookup by name,
 * and the order-free equality the reconciler compares two tables by.
 */

#include "sigilcompose/core/Attributes.h"

#include <cstring>

namespace sigil::compose {

const Attributes::Entry* Attributes::find(std::string_view name) const {
  if (!m_entries) return nullptr;
  for (const Entry& entry : *m_entries)
    if (entry.name == name) return &entry;
  return nullptr;
}

bool Attributes::sameType(const std::type_info& written,
                          const std::type_info& read) {
  if (written == read) return true;
  const char* spelled = written.name();
  if (std::strstr(spelled, "_GLOBAL__N_")) return false;
  return std::strcmp(spelled, read.name()) == 0;
}

void Attributes::setEntry(std::string_view name, Entry entry) {
  entry.name = std::string(name);
  // Writing copies the table first: another description may share it,
  // and a description is a value that never changes behind a holder.
  std::vector<Entry> entries = m_entries ? *m_entries : std::vector<Entry>();
  for (Entry& standing : entries)
    if (standing.name == name) {
      standing = std::move(entry);
      m_entries =
          std::make_shared<const std::vector<Entry>>(std::move(entries));
      return;
    }
  entries.push_back(std::move(entry));
  m_entries = std::make_shared<const std::vector<Entry>>(std::move(entries));
}

void Attributes::merge(const Attributes& other) {
  if (!other.m_entries) return;
  if (!m_entries) {
    m_entries = other.m_entries;
    return;
  }
  for (const Entry& entry : *other.m_entries) setEntry(entry.name, entry);
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
    if (!sameType(*entry.type, *match->type)) return false;
    if (!entry.equals(entry.value.get(), match->value.get())) return false;
  }
  return true;
}

}  // namespace sigil::compose
