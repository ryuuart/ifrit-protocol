#include <sigilmeasure/advanced/Counters.h>

#include <boost/container/flat_map.hpp>

#include <string>
#include <utility>

namespace sigil::measure {

/** One sorted, contiguous run of (name, count): a few dozen names are
 *  found by a binary search over one allocation, and iterate in order. */
struct Counters::Table {
  boost::container::flat_map<std::string, int64_t, std::less<>> counts;
};

Counters::Counters() : m_table(std::make_unique<Table>()) {}
Counters::~Counters() = default;
Counters::Counters(const Counters& other)
    : m_table(std::make_unique<Table>(*other.m_table)) {}
Counters& Counters::operator=(const Counters& other) {
  if (this != &other) *m_table = *other.m_table;
  return *this;
}
Counters::Counters(Counters&& other)
    : m_table(std::exchange(other.m_table, std::make_unique<Table>())) {}
Counters& Counters::operator=(Counters&& other) noexcept {
  std::swap(m_table, other.m_table);
  return *this;
}

void Counters::add(std::string_view name, int64_t amount) {
  auto found = m_table->counts.find(name);
  if (found == m_table->counts.end())
    m_table->counts.emplace(std::string(name), amount);
  else
    found->second += amount;
}

int64_t Counters::get(std::string_view name) const {
  const auto found = m_table->counts.find(name);
  return found == m_table->counts.end() ? 0 : found->second;
}

void Counters::reset() {
  for (auto& entry : m_table->counts) entry.second = 0;
}

void Counters::clear() { m_table->counts.clear(); }

size_t Counters::size() const { return m_table->counts.size(); }

void Counters::each(
    const std::function<void(std::string_view, int64_t)>& visit) const {
  for (const auto& [name, count] : m_table->counts) visit(name, count);
}

}  // namespace sigil::measure
