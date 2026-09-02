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

// The pin a hand-written comparator sits under: the count is asserted
// beside the type, so a field added to Mixed fails HERE rather than
// silently dropping out of an equality that names its members one by one.
static_assert(kFieldCount<Mixed> == 4,
              "Mixed gained or lost a field — rule on it wherever the type "
              "is compared, then bump this count.");

}  // namespace

TEST(Fields, CountsDirectMembers) {
  EXPECT_EQ(kFieldCount<Empty>, 0u);
  EXPECT_EQ(kFieldCount<One>, 1u);
}

TEST(Fields, ANestedAggregateIsOneField) { EXPECT_EQ(kFieldCount<Nested>, 2u); }

TEST(Fields, ATemplateIsCountedPerInstantiation) {
  EXPECT_EQ(kFieldCount<Slot<int>>, 3u);
  EXPECT_EQ(kFieldCount<Slot<std::string>>, 3u);
}
