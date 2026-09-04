/** @file
 * The order, read off the declarations. Every resource has versions —
 * one per pass that writes it, in declaration order — and the three
 * edges follow from them: a read runs after the write it sees, a write
 * runs after the write before it, and a write runs after every read of
 * the version it replaces. Among the passes whose dependencies are all
 * met, the one declared first runs first, so an order is a function of
 * the declarations and never of the machine.
 */

#include <algorithm>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <string>
#include <vector>

#include "Build.h"

namespace sigil::world::graph::detail {

namespace {

/** Which passes write a resource and which read it — from the declared
 *  reads and writes only. `previous()` names last frame's copy and
 *  therefore orders nothing, which is what lets a feedback loop be
 *  declared without one. */
struct Touchers {
  std::vector<size_t> writers;
  std::vector<size_t> readers;
};

boost::container::flat_map<std::string, Touchers> touchersOf(
    std::span<const Pass> passes) {
  boost::container::flat_map<std::string, Touchers> byName;
  for (size_t i = 0; i < passes.size(); ++i) {
    for (const std::string& name : passes[i].writes())
      byName[name].writers.push_back(i);
    for (const std::string& name : passes[i].reads())
      byName[name].readers.push_back(i);
  }
  return byName;
}

/** The cycle the ordering could not break, as the passes on it. It
 *  walks backwards along the dependencies, taking the lowest remaining
 *  one at every step, so the report is the same every run. */
std::string nameCycle(
    std::span<const Pass> passes,
    const std::vector<boost::container::flat_set<size_t>>& before,
    const std::vector<bool>& emitted) {
  size_t start = 0;
  while (start < passes.size() && emitted[start]) ++start;
  std::vector<size_t> walk;
  std::vector<bool> seen(passes.size(), false);
  size_t at = start;
  while (!seen[at]) {
    seen[at] = true;
    walk.push_back(at);
    // Every pass still waiting has a predecessor still waiting, so this
    // walk cannot run out before it repeats itself.
    for (size_t candidate : before[at])
      if (!emitted[candidate]) {
        at = candidate;
        break;
      }
  }
  std::vector<size_t> cycle(std::find(walk.begin(), walk.end(), at),
                            walk.end());
  std::reverse(cycle.begin(), cycle.end());
  std::string report = "the passes wait on each other:";
  for (size_t i : cycle) report += " " + passes[i].name() + " ->";
  return report + " " + passes[cycle.front()].name();
}

}  // namespace

std::string order(std::span<const Pass> passes, std::vector<size_t>& into) {
  into.clear();
  const boost::container::flat_map<std::string, Touchers> byName =
      touchersOf(passes);

  std::vector<boost::container::flat_set<size_t>> after(passes.size());
  std::vector<boost::container::flat_set<size_t>> before(passes.size());
  std::vector<int> waiting(passes.size(), 0);
  const auto edge = [&](size_t from, size_t to) {
    if (from == to) return;
    if (!after[from].insert(to).second) return;
    before[to].insert(from);
    ++waiting[to];
  };

  for (const auto& [name, touchers] : byName) {
    if (touchers.writers.empty()) continue;
    for (size_t reader : touchers.readers) {
      // The version this read sees: the last write declared before it,
      // or the first write of all when the reader was declared first —
      // a declaration order is not an execution order, and a reader
      // written down before its producer still reads what it produced.
      size_t seen = touchers.writers.front();
      for (size_t writer : touchers.writers)
        if (writer < reader) seen = writer;
      edge(seen, reader);
    }
    for (size_t version = 1; version < touchers.writers.size(); ++version) {
      const size_t replaced = touchers.writers[version - 1];
      const size_t writer = touchers.writers[version];
      edge(replaced, writer);
      for (size_t reader : touchers.readers)
        if (reader > replaced && reader < writer) edge(reader, writer);
    }
  }

  boost::container::flat_set<size_t> ready;
  for (size_t i = 0; i < passes.size(); ++i)
    if (waiting[i] == 0) ready.insert(i);
  std::vector<bool> emitted(passes.size(), false);
  while (!ready.empty()) {
    const size_t next = *ready.begin();
    ready.erase(ready.begin());
    into.push_back(next);
    emitted[next] = true;
    for (size_t dependent : after[next])
      if (--waiting[dependent] == 0) ready.insert(dependent);
  }
  if (into.size() == passes.size()) return {};
  into.clear();
  return nameCycle(passes, before, emitted);
}

}  // namespace sigil::world::graph::detail
