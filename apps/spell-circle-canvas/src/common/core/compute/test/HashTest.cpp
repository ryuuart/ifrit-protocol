/** @file
 * The identifying folds, pinned to the exact numbers they produce.
 *
 * These are the arithmetic several implementations have to agree on to
 * the bit: the same fold is written again in a shader, in a tool and in
 * whatever reads this repository's data next, and a second
 * implementation is only correct if it answers these numbers. A property
 * cannot say that — "deterministic", "in range" and "different for
 * different inputs" all survive a drifted fold — so the assertions here
 * are the values themselves.
 */

#include <gtest/gtest.h>
#include <sigilcore/compute/Hash.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace {

using namespace sigil::core;

/** One fold: what goes in, and the number every implementation of the
 *  fold must answer with. */
struct Fold {
  const char* name;
  std::variant<uint64_t, std::string_view> input;
  uint64_t folded;
};

std::string foldName(const testing::TestParamInfo<Fold>& info) {
  return info.param.name;
}

struct Fnv1aFold : testing::TestWithParam<Fold> {};

}  // namespace

TEST(Fnv1a, TheBasisAndThePrimeAreTheOnesEveryBucketUses) {
  EXPECT_EQ(hash::kFnvOffset, 1469598103934665603ull);
  EXPECT_EQ(hash::kFnvPrime, 1099511628211ull);
}

TEST_P(Fnv1aFold, AnswersTheNumberASecondImplementationMustAlsoAnswer) {
  const Fold& fold = GetParam();
  EXPECT_EQ(std::visit(
                [](auto v) { return hash::fnv1a(hash::kFnvOffset, v); },
                fold.input),
            fold.folded);
}

INSTANTIATE_TEST_SUITE_P(
    Folds, Fnv1aFold,
    testing::Values(
        Fold{"AWordOfZero", uint64_t{0}, 5187598658539770339ull},
        Fold{"AWordOfOne", uint64_t{1}, 2955283251572180930ull},
        Fold{"AWordOfMixedBits", uint64_t{0x0123456789abcdefull},
             16263046467545340003ull},
        Fold{"NoTextAtAll", std::string_view{}, 1469598103934665603ull},
        Fold{"OneLetter", std::string_view{"a"}, 4953267810257967366ull},
        Fold{"AShortWord", std::string_view{"sphere"}, 6372007673032843326ull},
        Fold{"ASentence", std::string_view{"the quick brown fox"},
             14575528814630447928ull}),
    foldName);

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

TEST(Combine, TheStirAnswersTheNumberASecondImplementationMustAlsoAnswer) {
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
