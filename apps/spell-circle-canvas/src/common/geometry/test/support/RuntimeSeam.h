#pragma once

/** @file
 * What every runtime seam in this library promises, written once.
 *
 * A seam is a comparable erased value over an executor: the built-in one
 * is a single value however it is reached, two seams holding equal models
 * are equal, and a default-constructed seam holds no executor at all. Each
 * seam would otherwise state that three times over, so it is stated here
 * once and instantiated per seam with a traits type declaring
 *
 *     using Seam = <the seam type>;
 *     static Seam builtIn();                  // the stock executor
 *     static Seam holding(const char* label); // a model comparing by label
 *
 * and one line beside it:
 *
 *     INSTANTIATE_TYPED_TEST_SUITE_P(<what the seam is>, RuntimeSeam, Traits);
 *
 * The suite is declared at file scope rather than in a namespace because
 * the instantiation macro pastes the suite's name, which a qualified name
 * cannot be pasted onto.
 */

#include <gtest/gtest.h>

template <typename Traits>
class RuntimeSeam : public ::testing::Test {};

TYPED_TEST_SUITE_P(RuntimeSeam);

TYPED_TEST_P(RuntimeSeam, TheBuiltInExecutorIsOneValueHoweverItIsReached) {
  EXPECT_TRUE((bool)TypeParam::builtIn());
  EXPECT_EQ(TypeParam::builtIn(), TypeParam::builtIn());
}

TYPED_TEST_P(RuntimeSeam, TwoSeamsAreEqualWhenTheModelsTheyHoldAre) {
  const auto a = TypeParam::holding("a");
  const auto b = TypeParam::holding("a");
  const auto c = TypeParam::holding("c");
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(a, TypeParam::builtIn());
}

TYPED_TEST_P(RuntimeSeam, AnEmptySeamHoldsNoExecutorAndIsNotTheBuiltInOne) {
  using Seam = typename TypeParam::Seam;
  EXPECT_FALSE((bool)Seam());
  EXPECT_EQ(Seam(), Seam());
  EXPECT_NE(Seam(), TypeParam::builtIn());
  EXPECT_NE(Seam(), TypeParam::holding("a"));
}

REGISTER_TYPED_TEST_SUITE_P(RuntimeSeam,
                            TheBuiltInExecutorIsOneValueHoweverItIsReached,
                            TwoSeamsAreEqualWhenTheModelsTheyHoldAre,
                            AnEmptySeamHoldsNoExecutorAndIsNotTheBuiltInOne);
