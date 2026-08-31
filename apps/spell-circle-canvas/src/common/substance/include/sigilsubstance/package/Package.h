#pragma once

/** @file
 * A Package: a loaded .sbsar — the archive description, one Graph per
 * graph in it, and the engine renderer they share.
 */

#include <sigilsubstance/graph/Graph.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace sigil::substance {

/** A loaded .sbsar. */
class Package {
 public:
  /** Load from bytes. nullptr (and @p error) on a malformed archive or
   *  when the engine cannot start. */
  static std::unique_ptr<Package> load(const void* bytes, size_t size,
                                       std::string* error = nullptr);
  static std::unique_ptr<Package> load(const std::filesystem::path& file,
                                       std::string* error = nullptr);
  ~Package();
  Package(const Package&) = delete;
  Package& operator=(const Package&) = delete;

  size_t graphCount() const;
  Graph& graph(size_t index);
  const Graph& graph(size_t index) const;
  /** By label or url; null when absent. */
  Graph* find(std::string_view labelOrUrl);

  /** The engine's own version string, for diagnostics. */
  static std::string engineVersion();

  struct Impl;

 private:
  Package();
  std::unique_ptr<Impl> m_impl;
};

}  // namespace sigil::substance
