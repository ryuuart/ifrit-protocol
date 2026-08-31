/** @file
 * The identifying hashes, pinned to the exact numbers they produce.
 *
 * A cache key computed one way must equal the same key computed
 * anywhere else, and a drifted fold answers a different bucket for the
 * same thing without failing anything that only checks determinism. So
 * the assertions here are the values themselves.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Hash.h>

#include <cstdint>

namespace {

using namespace sigil::core;

}  // namespace

TEST(Fnv1a, TheBasisAndThePrimeAreTheOnesEveryBucketUses) {
  EXPECT_EQ(hash::kFnvOffset, 1469598103934665603ull);
  EXPECT_EQ(hash::kFnvPrime, 1099511628211ull);
}

TEST(Fnv1a, FoldingAWordIsPinned) {
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, 0ull), 5187598658539770339ull);
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, 1ull), 2955283251572180930ull);
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, 0x0123456789abcdefull),
            16263046467545340003ull);
}

TEST(Fnv1a, FoldingTextIsPinned) {
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, std::string_view{}),
            hash::kFnvOffset);
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, "a"), 4953267810257967366ull);
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, "sphere"), 6372007673032843326ull);
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, "the quick brown fox"),
            14575528814630447928ull);
}

TEST(Fnv1a, AWordAndTextFoldIntoOneRunningValue) {
  EXPECT_EQ(hash::fnv1a(hash::fnv1a(hash::kFnvOffset, 3ull), "x"),
            2937553308417855080ull);
}

TEST(Fnv1a, AByteWithItsHighBitSetFoldsUnsigned) {
  // Reading the byte as a signed char would sign-extend it and fold a
  // different word, so the two spellings of the same byte must agree.
  const char high[] = {(char)0xffu, '\0'};
  EXPECT_EQ(hash::fnv1a(hash::kFnvOffset, std::string_view{high, 1}),
            (hash::kFnvOffset ^ 0xffull) * hash::kFnvPrime);
}

TEST(Combine, TheStirIsPinned) {
  EXPECT_EQ(hash::combine(0u, 0u), 2654435769u);
  EXPECT_EQ(hash::combine(1u, 2u), 2654435834u);
  EXPECT_EQ(hash::combine(hash::combine(0x9E3779B9u, 0x3f800000u), 0xbf800000u),
            11156902649582ull);
}

TEST(Combine, OneChangedBitMovesTheWholeResult) {
  const size_t a = hash::combine(hash::combine(0u, 1u), 2u);
  const size_t b = hash::combine(hash::combine(0u, 1u), 3u);
  EXPECT_NE(a, b);
  // Order matters: the same two words the other way round is a different
  // key, which is what makes a struct's fields foldable one at a time.
  EXPECT_NE(hash::combine(hash::combine(0u, 1u), 2u),
            hash::combine(hash::combine(0u, 2u), 1u));
}
