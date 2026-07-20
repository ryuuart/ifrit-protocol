/** @file
 * sigil::weave::InlineVector — the one container SigilWeave defines itself
 * (Word::segments' inline storage). Exercises inline→heap growth, value
 * semantics, and element lifetime accounting.
 */

#include <sigilweave/InlineVector.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

using sigil::weave::InlineVector;

namespace {

/// Counts live instances so lifetime bugs (double-destroy, leaked slots)
/// show up as instance-count mismatches.
struct Tracked {
  static int liveCount;
  std::string value;

  explicit Tracked(std::string trackedValue) : value(std::move(trackedValue)) {
    ++liveCount;
  }
  Tracked(const Tracked &other) : value(other.value) { ++liveCount; }
  Tracked(Tracked &&other) noexcept : value(std::move(other.value)) {
    ++liveCount;
  }
  Tracked &operator=(const Tracked &) = default;
  Tracked &operator=(Tracked &&) = default;
  ~Tracked() { --liveCount; }

  bool operator==(const Tracked &other) const { return value == other.value; }
};

int Tracked::liveCount = 0;

} // namespace

TEST(InlineVector, StaysInlineUpToCapacity) {
  InlineVector<int, 2> values;
  EXPECT_TRUE(values.empty());
  EXPECT_EQ(values.capacity(), 2u);
  values.push_back(1);
  values.push_back(2);
  EXPECT_EQ(values.size(), 2u);
  EXPECT_EQ(values.capacity(), 2u) << "two elements must not allocate";
  EXPECT_EQ(values.front(), 1);
  EXPECT_EQ(values.back(), 2);
}

TEST(InlineVector, GrowsPastInlineCapacityAndPreservesElements) {
  InlineVector<Tracked, 1> values;
  for (int index = 0; index < 20; ++index)
    values.emplace_back(std::to_string(index));
  ASSERT_EQ(values.size(), 20u);
  EXPECT_GE(values.capacity(), 20u);
  for (int index = 0; index < 20; ++index)
    EXPECT_EQ(values[static_cast<size_t>(index)].value, std::to_string(index));
  EXPECT_EQ(Tracked::liveCount, 20);
  values.clear();
  EXPECT_EQ(Tracked::liveCount, 0);
  EXPECT_GE(values.capacity(), 20u) << "clear() must keep the storage";
}

TEST(InlineVector, CopyIsDeepAndIndependent) {
  InlineVector<Tracked, 1> source;
  source.emplace_back("a");
  source.emplace_back("b"); // forces heap storage

  InlineVector<Tracked, 1> copy(source);
  ASSERT_EQ(copy.size(), 2u);
  EXPECT_EQ(copy, source);
  copy[0].value = "changed";
  EXPECT_EQ(source[0].value, "a") << "copies must not share storage";

  InlineVector<Tracked, 1> assigned;
  assigned.emplace_back("overwritten");
  assigned = source;
  EXPECT_EQ(assigned, source);
}

TEST(InlineVector, MoveStealsHeapAndEmptiesSource) {
  InlineVector<Tracked, 1> source;
  for (int index = 0; index < 5; ++index)
    source.emplace_back(std::to_string(index));
  const Tracked *heapData = source.data();

  InlineVector<Tracked, 1> moved(std::move(source));
  EXPECT_EQ(moved.size(), 5u);
  EXPECT_EQ(moved.data(), heapData) << "heap storage must transfer by pointer";
  EXPECT_TRUE(source.empty());
  EXPECT_EQ(source.capacity(), 1u) << "moved-from reverts to inline capacity";

  // Inline-mode move: elements move one by one, source is emptied.
  InlineVector<Tracked, 2> inlineSource;
  inlineSource.emplace_back("x");
  InlineVector<Tracked, 2> inlineMoved(std::move(inlineSource));
  ASSERT_EQ(inlineMoved.size(), 1u);
  EXPECT_EQ(inlineMoved[0].value, "x");
  EXPECT_TRUE(inlineSource.empty());
}

TEST(InlineVector, DestructionReleasesEveryElement) {
  {
    InlineVector<Tracked, 2> values;
    values.emplace_back("inline");
    EXPECT_EQ(Tracked::liveCount, 1);
  }
  EXPECT_EQ(Tracked::liveCount, 0);
  {
    InlineVector<Tracked, 2> values;
    for (int index = 0; index < 9; ++index)
      values.emplace_back(std::to_string(index));
    EXPECT_EQ(Tracked::liveCount, 9);
    InlineVector<Tracked, 2> other = std::move(values);
    EXPECT_EQ(Tracked::liveCount, 9);
  }
  EXPECT_EQ(Tracked::liveCount, 0);
}

TEST(InlineVector, HoldsMoveOnlyElements) {
  InlineVector<std::unique_ptr<int>, 1> values;
  values.push_back(std::make_unique<int>(7));
  values.emplace_back(std::make_unique<int>(9)); // grows to heap
  ASSERT_EQ(values.size(), 2u);
  EXPECT_EQ(*values[0], 7);
  EXPECT_EQ(*values[1], 9);
  InlineVector<std::unique_ptr<int>, 1> moved = std::move(values);
  EXPECT_EQ(*moved.back(), 9);
}

TEST(InlineVector, EqualityComparesElementwise) {
  InlineVector<int, 2> left;
  InlineVector<int, 2> right;
  EXPECT_EQ(left, right);
  left.push_back(1);
  EXPECT_FALSE(left == right);
  right.push_back(1);
  EXPECT_EQ(left, right);
  for (int value : {2, 3, 4})
    left.push_back(value);
  for (int value : {2, 3, 4})
    right.push_back(value);
  EXPECT_EQ(left, right) << "inline/heap boundary must not affect equality";
  right.back() = 5;
  EXPECT_FALSE(left == right);
}
