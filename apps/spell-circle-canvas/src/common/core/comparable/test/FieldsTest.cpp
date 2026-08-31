/** @file
 * The field pin: the count a hand-written comparator is held to, over
 * structs of the shapes a comparable value takes.
 */

#include <gtest/gtest.h>
#include <sigilcore/comparable/Fields.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

using sigil::core::kFieldCount;

struct Empty {};
struct One {
  int a;
};
struct Mixed {
  float a;
  bool b;
  std::string c;
  std::vector<int> d;
};
struct Nested {
  Mixed inner;  // one field, not four
  int tail;
};
template <typename T>
struct Slot {
  T value;
  T from;
  bool held;
};

// The pin as a comparator writes it: the count is asserted beside the
// body, so a field added to Mixed fails HERE rather than silently
// dropping out of equality.
static_assert(kFieldCount<Mixed> == 4,
              "Mixed gained or lost a field — rule on it in mixedEqual() "
              "below, then bump this count.");
bool mixedEqual(const Mixed& a, const Mixed& b) {
  return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
}

}  // namespace

TEST(Fields, CountsDirectMembers) {
  EXPECT_EQ(kFieldCount<Empty>, 0u);
  EXPECT_EQ(kFieldCount<One>, 1u);
  EXPECT_EQ(kFieldCount<Mixed>, 4u);
}

TEST(Fields, ANestedAggregateIsOneField) { EXPECT_EQ(kFieldCount<Nested>, 2u); }

TEST(Fields, ATemplateIsCountedPerInstantiation) {
  EXPECT_EQ(kFieldCount<Slot<int>>, 3u);
  EXPECT_EQ(kFieldCount<Slot<std::string>>, 3u);
}

TEST(Fields, ThePinnedComparatorSeesEveryField) {
  const Mixed a{1.0f, true, "x", {1, 2}};
  EXPECT_TRUE(mixedEqual(a, a));
  EXPECT_FALSE(mixedEqual(a, Mixed{2.0f, true, "x", {1, 2}}));
  EXPECT_FALSE(mixedEqual(a, Mixed{1.0f, false, "x", {1, 2}}));
  EXPECT_FALSE(mixedEqual(a, Mixed{1.0f, true, "y", {1, 2}}));
  EXPECT_FALSE(mixedEqual(a, Mixed{1.0f, true, "x", {1}}));
}
