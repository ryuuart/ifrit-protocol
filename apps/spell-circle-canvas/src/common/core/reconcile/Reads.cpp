/** @file
 * The order declared reads imply: every reader after what it read, with
 * the order it was given kept wherever nothing says otherwise.
 */

#include "sigilcore/reconcile/Reads.h"

#include <algorithm>
#include <unordered_map>

namespace sigil::core {

std::vector<uint32_t> orderByReads(std::span<const std::string> keys,
                                   std::span<const std::vector<Read>> reads) {
  const auto count = static_cast<uint32_t>(std::min(keys.size(), reads.size()));
  std::vector<uint32_t> order;
  order.reserve(count);
  if (count == 0) return order;

  // Which reader answers to which key. A key two readers claim is the
  // host's mistake and the FIRST one wins, because that is the one every
  // other lookup in a keyed tree finds.
  std::unordered_map<std::string, uint32_t> answersTo;
  for (uint32_t index = 0; index < count; ++index)
    if (!keys[index].empty()) answersTo.emplace(keys[index], index);

  // One edge per read that names another reader. A read of a key no reader
  // answers to is not an edge: it names an ordinary node, which is settled
  // before any reader runs.
  std::vector<std::vector<uint32_t>> readBy(count);
  std::vector<uint32_t> waitingOn(count, 0);
  for (uint32_t index = 0; index < count; ++index)
    for (const Read& read : reads[index]) {
      const auto found = answersTo.find(read.key);
      if (found == answersTo.end() || found->second == index) continue;
      readBy[found->second].push_back(index);
      waitingOn[index]++;
    }

  std::vector<bool> emitted(count, false);
  while (order.size() < count) {
    // The first reader, IN THE ORDER GIVEN, that is waiting on nothing —
    // which is what makes this stable: readers that read none of each other
    // come out exactly as they went in.
    uint32_t next = count;
    for (uint32_t index = 0; index < count; ++index)
      if (!emitted[index] && waitingOn[index] == 0) {
        next = index;
        break;
      }
    if (next == count) {
      // A CYCLE: nothing is waiting on nothing. It is broken at the first
      // reader still unemitted, which keeps the declaration order among
      // the readers caught in it — a slightly-off pass rather than a hang.
      for (uint32_t index = 0; index < count; ++index)
        if (!emitted[index]) {
          next = index;
          break;
        }
    }
    emitted[next] = true;
    order.push_back(next);
    for (const uint32_t reader : readBy[next])
      if (waitingOn[reader] > 0) waitingOn[reader]--;
  }
  return order;
}

}  // namespace sigil::core
