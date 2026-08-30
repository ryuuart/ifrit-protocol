/** @file
 * Acquiring and releasing a cooked artefact.
 */

#include "Resources.h"

#include <algorithm>
#include <utility>
#include <variant>

namespace sigil::world {

Resource* ResourceStore::acquire(Geometry geometry, int64_t* cooked) {
  if (std::holds_alternative<std::monostate>(geometry)) return nullptr;
  const uint64_t bucket = signature(geometry);
  for (const std::unique_ptr<Resource>& entry : m_entries) {
    if (entry->bucket != bucket) continue;
    if (!(entry->geometry == geometry)) continue;
    ++entry->references;
    return entry.get();
  }
  auto entry = std::make_unique<Resource>();
  entry->bucket = bucket;
  entry->cooked = cook(geometry);
  entry->geometry = std::move(geometry);
  entry->references = 1;
  if (cooked) ++*cooked;
  Resource* raw = entry.get();
  m_entries.push_back(std::move(entry));
  return raw;
}

void ResourceStore::release(Resource* resource) {
  if (!resource) return;
  if (--resource->references > 0) return;
  const auto it = std::find_if(m_entries.begin(), m_entries.end(),
                               [resource](const std::unique_ptr<Resource>& e) {
                                 return e.get() == resource;
                               });
  if (it != m_entries.end()) m_entries.erase(it);
}

}  // namespace sigil::world
