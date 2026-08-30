/** @file
 * The phase runner: a non-converging phase runs once, the converging group
 * repeats until a round changes nothing, the settle step runs after every
 * changed round, and the round cap ends a group that never settles.
 */

#include <gtest/gtest.h>
#include <sigilcore/reconcile/Phases.h>

#include <string>
#include <vector>

using sigil::core::Phase;
using sigil::core::runPhases;

namespace {

struct Layout {
  std::vector<std::string> log;
  int pinMoves = 2;  // how many rounds the pin phase keeps moving
  int routeMoves = 0;
  bool always = false;

  bool yoga() {
    log.push_back("yoga");
    return false;
  }
  bool pins() {
    log.push_back("pins");
    if (always) return true;
    return pinMoves-- > 0;
  }
  bool routes() {
    log.push_back("routes");
    return routeMoves-- > 0;
  }
  bool sync() {
    log.push_back("sync");
    return false;
  }
  void settle() { log.push_back("settle"); }

  static constexpr Phase<Layout> phases[] = {
      {"yoga", &Layout::yoga, false},
      {"pins", &Layout::pins, true},
      {"routes", &Layout::routes, true},
      {"sync", &Layout::sync, false},
  };
  int run(int maxRounds) {
    return runPhases(*this, std::span<const Phase<Layout>>(phases), maxRounds,
                     [this] { settle(); });
  }
};

}  // namespace

TEST(Phases, AStableTreeRunsTheGroupOnceAndSettlesNothing) {
  Layout layout;
  layout.pinMoves = 0;
  EXPECT_EQ(layout.run(3), 1);
  EXPECT_EQ(layout.log,
            (std::vector<std::string>{"yoga", "pins", "routes", "sync"}));
}

TEST(Phases, TheGroupRepeatsUntilARoundChangesNothing) {
  Layout layout;
  layout.pinMoves = 2;
  EXPECT_EQ(layout.run(5), 3);
  EXPECT_EQ(layout.log, (std::vector<std::string>{
                            "yoga", "pins", "routes", "settle", "pins",
                            "routes", "settle", "pins", "routes", "sync"}));
}

TEST(Phases, EveryPhaseOfTheGroupRunsEachRound) {
  // A later phase that moves keeps the earlier one running too: the round
  // is the unit, not the phase.
  Layout layout;
  layout.pinMoves = 0;
  layout.routeMoves = 1;
  EXPECT_EQ(layout.run(5), 2);
  EXPECT_EQ(layout.log,
            (std::vector<std::string>{"yoga", "pins", "routes", "settle",
                                      "pins", "routes", "sync"}));
}

TEST(Phases, TheRoundCapEndsAGroupThatNeverSettles) {
  Layout layout;
  layout.always = true;
  EXPECT_EQ(layout.run(3), 3);
  int settles = 0, pins = 0;
  for (const std::string& e : layout.log) {
    settles += e == "settle";
    pins += e == "pins";
  }
  EXPECT_EQ(pins, 3);
  EXPECT_EQ(settles, 3);  // every round changed, every round settled
  EXPECT_EQ(layout.log.back(), "sync");
}
