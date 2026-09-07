/** @file
 * The two things a value needs before anything can decide it did not
 * change, which is one subject: type erasure that keeps a value's own
 * equality — empty, copies of one value, two comparable models compared
 * by type and by value, and the escape hatch equal to nothing but its own
 * copies — and the field pin a hand-written comparator sits under, over
 * aggregates of the shapes a comparable value takes.
 */

#include <gtest/gtest.h>
#include <sigilcore/comparable/Erased.h>
#include <sigilcore/comparable/Fields.h>

#include <cstdint>
#include <string>
#include <vector>

using sigil::core::Erased;
using sigil::core::kFieldCount;

namespace {

struct Ops {
  virtual ~Ops() = default;
  virtual int answer() const = 0;
};

struct Comparable : Ops {
  explicit Comparable(int n) : n(n) {}
  int n;
  int answer() const override { return n; }
  bool operator==(const Comparable& o) const { return n == o.n; }
};

struct OtherComparable : Ops {
  explicit OtherComparable(int n) : n(n) {}
  int n;
  int answer() const override { return -n; }
  bool operator==(const OtherComparable& o) const { return n == o.n; }
};

struct Opaque : Ops {
  explicit Opaque(int n) : n(n) {}
  int n;
  int answer() const override { return n * 10; }
};

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

TEST(Erased, EmptyHoldsNothing) {
  Erased<Ops> empty;
  EXPECT_FALSE(empty);
  EXPECT_EQ(empty.get(), nullptr);
  EXPECT_FALSE(empty.comparable());
  EXPECT_TRUE(empty == Erased<Ops>{});  // shared (null) state
  Erased<Ops> value = Comparable{1};
  EXPECT_FALSE(empty == value);
}

TEST(Erased, AComparableModelComparesByTypeAndValue) {
  Erased<Ops> a = Comparable{3};
  Erased<Ops> b = Comparable{3};
  Erased<Ops> c = Comparable{4};
  Erased<Ops> d = OtherComparable{3};
  EXPECT_TRUE(a.comparable());
  EXPECT_EQ(a->answer(), 3);
  EXPECT_EQ((*a).answer(), 3);
  EXPECT_TRUE(a == b);   // separately constructed, same type, same value
  EXPECT_FALSE(a == c);  // same type, different value
  EXPECT_FALSE(a == d);  // different type, equal-looking value
}

TEST(Erased, CopiesShareStateAndAreEqual) {
  Erased<Ops> a = Comparable{3};
  // the copy is what the test compares
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  Erased<Ops> copy = a;
  EXPECT_TRUE(a == copy);
  EXPECT_EQ(a.get(), copy.get());
}

TEST(Erased, TheEscapeHatchIsEqualOnlyToItsOwnCopies) {
  Erased<Ops> a{Opaque{2}};
  Erased<Ops> same{Opaque{2}};
  // the copy is what the test compares
  // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
  Erased<Ops> copy = a;
  EXPECT_TRUE(a);
  EXPECT_FALSE(a.comparable());
  EXPECT_EQ(a->answer(), 20);
  EXPECT_FALSE(a == same);  // identity only
  EXPECT_TRUE(a == copy);
  Erased<Ops> comparable = Comparable{2};
  EXPECT_FALSE(a == comparable);
  EXPECT_FALSE(comparable == a);
}

TEST(Fields, TheCountIsTheMembersTheTypeItselfDeclares) {
  EXPECT_EQ(kFieldCount<Empty>, 0u);
  EXPECT_EQ(kFieldCount<One>, 1u);
}

TEST(Fields, ANestedAggregateIsOneField) { EXPECT_EQ(kFieldCount<Nested>, 2u); }

TEST(Fields, ATemplateIsCountedPerInstantiation) {
  EXPECT_EQ(kFieldCount<Slot<int>>, 3u);
  EXPECT_EQ(kFieldCount<Slot<std::string>>, 3u);
}
