/** @file
 * Comparable type erasure: an empty value, copies of one value, two
 * comparable models compared by type and value, and the escape hatch that
 * compares equal to nothing but its own copies.
 */

#include <gtest/gtest.h>
#include <sigilcore/comparable/Erased.h>

using sigil::core::Erased;

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
