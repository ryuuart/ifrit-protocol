/** @file
 * The named counters and the order they read back in.
 */

#include <gtest/gtest.h>
#include <sigilmeasure/stats/Counters.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace sigil::measure;

TEST(Counters, AddsGetsAndReadsUnknownAsZero) {
  Counters c;
  EXPECT_EQ(c.get("never"), 0);
  c.add("a");
  c.add("a", 4);
  c.add("b", -2);
  EXPECT_EQ(c.get("a"), 5);
  EXPECT_EQ(c.get("b"), -2);
  EXPECT_EQ(c.size(), 2u);
}

TEST(Counters, EachVisitsInNameOrderAndResetKeepsNames) {
  Counters c;
  c.add("zeta", 3);
  c.add("alpha", 1);
  std::vector<std::string> names;
  std::vector<int64_t> counts;
  c.each([&](std::string_view n, int64_t v) {
    names.emplace_back(n);
    counts.push_back(v);
  });
  EXPECT_EQ(names, (std::vector<std::string>{"alpha", "zeta"}));
  EXPECT_EQ(counts, (std::vector<int64_t>{1, 3}));
  c.reset();
  EXPECT_EQ(c.size(), 2u);
  EXPECT_EQ(c.get("zeta"), 0);
  c.clear();
  EXPECT_EQ(c.size(), 0u);
}
