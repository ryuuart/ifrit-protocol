#pragma once

/** @file
 * @ingroup measure-stats
 * Named integer counters — how many of each thing a run did.
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace sigil::measure {

/** A set of counters addressed by name, created on first use and
 *  iterated in name order so a printed set reads the same every run.
 *  Reading a name that was never counted is 0, not an error. The table
 *  is kept compact and ordered behind the implementation. */
class Counters {
 public:
  Counters();
  ~Counters();
  Counters(const Counters& other);
  Counters& operator=(const Counters& other);
  Counters(Counters&& other);
  Counters& operator=(Counters&&) noexcept;

  /** Adds @p amount to the counter called @p name, starting it at
   *  @p amount when nothing has counted under that name yet. */
  void add(std::string_view name, int64_t amount = 1);
  /** What stands under @p name; 0 when nothing counted there. */
  [[nodiscard]] int64_t get(std::string_view name) const;
  /** Every counter back to 0 — the names are kept, so a set that is
   *  printed after a reset still lists what it counts. */
  void reset();
  /** Drops every counter, names included. */
  void clear();
  /** How many names are counted. */
  [[nodiscard]] size_t size() const;

  /** Visits `(name, count)` in name order. */
  void each(const std::function<void(std::string_view, int64_t)>& visit) const;

 private:
  struct Table;
  std::unique_ptr<Table> m_table;
};

}  // namespace sigil::measure
