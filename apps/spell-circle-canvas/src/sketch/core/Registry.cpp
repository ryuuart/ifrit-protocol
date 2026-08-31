/** @file
 * The registry behind SIGIL_SKETCH: what a binary was built with, in an
 * order that does not depend on the linker.
 */

#include "sigilsketch/core/Registry.h"

#include <algorithm>
#include <cctype>

namespace sigil::sketch {

namespace {

/** The registry as it is written to, before it is read.
 *
 *  Function-local: registrations run during static initialization, and a
 *  namespace-scope vector could be constructed after the first of them. */
std::vector<Entry>& recorded() {
  static std::vector<Entry> entries;
  return entries;
}

/** …and the order it is read in, settled once. A registration after the
 *  first read would not be seen, which is exactly right: registrations
 *  are static initializers and all of them run before main. */
bool& settled() {
  static bool done = false;
  return done;
}

std::string lowered(std::string_view text) {
  std::string out(text);
  for (char& c : out) c = (char)std::tolower((unsigned char)c);
  return out;
}

}  // namespace

bool add(const char* key, const char* name, const char* category,
         const char* blurb, Kind (*kind)()) noexcept {
  if (!key || !kind) return false;
  try {
    recorded().push_back({key, name && *name ? name : key,
                          category ? category : "", blurb ? blurb : "", kind});
  } catch (...) {
    // The registry failed to grow. This runs during static
    // initialization, where no handler exists and an unwinding exception
    // is fatal — and a process that cannot allocate a few pointers
    // before main() is about to fail loudly anyway.
    return false;
  }
  return true;
}

const std::vector<Entry>& registry() {
  std::vector<Entry>& entries = recorded();
  if (!settled()) {
    settled() = true;
    std::sort(
        entries.begin(), entries.end(), [](const Entry& a, const Entry& b) {
          const int byCategory = std::string_view(a.category)
                                     .compare(std::string_view(b.category));
          if (byCategory != 0) return byCategory < 0;
          return std::string_view(a.name) < std::string_view(b.name);
        });
  }
  return entries;
}

int find(std::string_view query) {
  if (query.empty()) return -1;
  const std::vector<Entry>& entries = registry();
  const int count = (int)entries.size();
  if (query.find_first_not_of("0123456789") == std::string_view::npos) {
    const int index = std::stoi(std::string(query));
    return index >= 0 && index < count ? index : -1;
  }
  const std::string needle = lowered(query);
  // Exact wins over substring, and both win over a later entry: two
  // passes rather than one, so `hello` cannot be captured by a sketch
  // whose name merely contains it.
  for (int i = 0; i < count; ++i)
    if (lowered(entries[i].name) == needle || lowered(entries[i].key) == needle)
      return i;
  for (int i = 0; i < count; ++i)
    if (lowered(entries[i].name).find(needle) != std::string::npos ||
        lowered(entries[i].key).find(needle) != std::string::npos)
      return i;
  return -1;
}

std::string title(std::string_view name) {
  std::string out(name);
  for (char& c : out)
    if (c == '_') c = ' ';
  return out;
}

}  // namespace sigil::sketch
