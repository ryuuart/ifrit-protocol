#pragma once

/** @file
 * Named integer counters — how many of each thing a run did.
 */

#include <boost/container/flat_map.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace sigil::measure {

/** A set of counters addressed by name, created on first use and
 *  iterated in name order so a printed set reads the same every run.
 *  Reading a name that was never counted is 0, not an error. */
class Counters {
 public:
  void add(std::string_view name, int64_t n = 1) {
    auto it = m_counts.find(name);
    if (it == m_counts.end())
      m_counts.emplace(std::string(name), n);
    else
      it->second += n;
  }
  int64_t get(std::string_view name) const {
    const auto it = m_counts.find(name);
    return it == m_counts.end() ? 0 : it->second;
  }
  /** Every counter back to 0 — the names are kept, so a set that is
   *  printed after a reset still lists what it counts. */
  void reset() {
    for (auto& [name, count] : m_counts) count = 0;
  }
  /** Drops every counter, names included. */
  void clear() { m_counts.clear(); }
  size_t size() const { return m_counts.size(); }

  /** Visits `(name, count)` in name order. */
  void each(const std::function<void(std::string_view, int64_t)>& fn) const {
    for (const auto& [name, count] : m_counts) fn(name, count);
  }

 private:
  boost::container::flat_map<std::string, int64_t, std::less<>> m_counts;
};

}  // namespace sigil::measure
