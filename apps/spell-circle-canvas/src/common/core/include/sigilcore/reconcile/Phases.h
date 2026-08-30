#pragma once

/** @file
 * Host-declared phases — a list of passes a host runs once its tree needs
 * settling, each a member function answering whether it moved anything,
 * with a converging group that repeats until a round changes nothing or
 * the round cap is reached.
 */

#include <cstddef>
#include <span>

namespace sigil::core {

/** One pass of a host's phase list. `run` answers true when it changed
 *  state some other pass may have already read; the runner only asks
 *  that of a converging pass. */
template <class Impl>
struct Phase {
  const char* name;
  bool (Impl::*run)();
  /** Runs inside the convergence loop: the contiguous group of converging
   *  passes is repeated, with `settle` between rounds, until a round
   *  changes nothing or the round cap is reached. */
  bool converging;
};

/** Runs `phases` in order on `impl`. A non-converging phase runs once. The
 *  converging phases form one contiguous group, and that group is a
 *  bounded convergence loop rather than a single second pass: every
 *  writer in it is expected to be idempotent and to report `changed` only
 *  on an actual delta, so a stable tree costs one extra pass and exits,
 *  and a settling one converges within `maxRounds`. The cap is what
 *  guarantees termination if two writers ever disagree permanently — the
 *  result is a slightly-off pass instead of a hang. `settle()` runs after
 *  every round that changed something, before the next round reads.
 *  Returns how many converging rounds ran. */
template <class Impl, class Settle>
int runPhases(Impl& impl, std::span<const Phase<Impl>> phases, int maxRounds,
              Settle&& settle) {
  int rounds = 0;
  const size_t count = phases.size();
  for (size_t i = 0; i < count;) {
    if (!phases[i].converging) {
      (impl.*phases[i].run)();
      ++i;
      continue;
    }
    size_t groupEnd = i;
    while (groupEnd < count && phases[groupEnd].converging) ++groupEnd;
    for (int round = 0; round < maxRounds; ++round) {
      ++rounds;
      bool changed = false;
      for (size_t p = i; p < groupEnd; ++p) changed |= (impl.*phases[p].run)();
      if (!changed) break;
      settle();
    }
    i = groupEnd;
  }
  return rounds;
}

}  // namespace sigil::core
