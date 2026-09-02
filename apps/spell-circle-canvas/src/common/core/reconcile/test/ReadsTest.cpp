/** @file
 * The order declared reads imply: stability where nothing says otherwise,
 * a reader after what it read however they were written, and a cycle
 * broken rather than chased.
 */

#include <gtest/gtest.h>
#include <sigilcore/reconcile/Reads.h>

#include <algorithm>

#include <string>
#include <vector>

using sigil::core::Facet;
using sigil::core::orderByReads;
using sigil::core::Read;

namespace {

std::vector<uint32_t> order(std::vector<std::string> keys,
                            std::vector<std::vector<Read>> reads) {
  return orderByReads(keys, reads);
}

}  // namespace

TEST(Reads, ReadersThatReadNothingKeepTheOrderTheyWereGiven) {
  // The property the whole thing rests on: a host whose readers are
  // independent runs them in exactly the order it ran them in before, so
  // adopting this moves nothing.
  const std::vector<uint32_t> got =
      order({"a", "b", "c"}, {{}, {}, {}});
  EXPECT_EQ(got, (std::vector<uint32_t>{0, 1, 2}));
}

TEST(Reads, AReaderComesAfterWhatItReads) {
  // Declared first, resolved last: "a" reads "c", so it waits for it.
  const std::vector<uint32_t> got =
      order({"a", "b", "c"},
            {{Read{"c", Facet::Bounds}}, {}, {}});
  ASSERT_EQ(got.size(), 3u);
  const auto place = [&](uint32_t index) {
    return std::find(got.begin(), got.end(), index) - got.begin();
  };
  EXPECT_LT(place(2), place(0));
  // …and "b", which reads nothing, is not moved past anything it did not
  // have to be.
  EXPECT_EQ(got[0], 1u);
}

TEST(Reads, AChainResolvesEndToEnd) {
  const std::vector<uint32_t> got =
      order({"a", "b", "c"},
            {{Read{"b", Facet::Units}}, {Read{"c", Facet::Bounds}}, {}});
  EXPECT_EQ(got, (std::vector<uint32_t>{2, 1, 0}));
}

TEST(Reads, AReadOfAKeyNoReaderAnswersToIsNotAnEdge) {
  // It names an ordinary node, settled before any reader runs.
  const std::vector<uint32_t> got =
      order({"a", "b"}, {{Read{"a-box", Facet::Outline}}, {}});
  EXPECT_EQ(got, (std::vector<uint32_t>{0, 1}));
}

TEST(Reads, AReaderThatReadsItselfIsNotWaitingOnAnything) {
  const std::vector<uint32_t> got =
      order({"a", "b"}, {{Read{"a", Facet::Bounds}}, {}});
  EXPECT_EQ(got, (std::vector<uint32_t>{0, 1}));
}

TEST(Reads, ACycleIsBrokenWhereItClosesRatherThanChased) {
  const std::vector<uint32_t> got =
      order({"a", "b", "c"},
            {{Read{"b", Facet::Bounds}}, {Read{"a", Facet::Bounds}}, {}});
  ASSERT_EQ(got.size(), 3u);
  // Everything is emitted exactly once, and the reader outside the cycle
  // is not held up by it.
  std::vector<uint32_t> sorted = got;
  std::sort(sorted.begin(), sorted.end());
  EXPECT_EQ(sorted, (std::vector<uint32_t>{0, 1, 2}));
  EXPECT_EQ(got[0], 2u);
}

TEST(Reads, AnEmptyDeclarationOrdersNothing) {
  EXPECT_TRUE(order({}, {}).empty());
}
