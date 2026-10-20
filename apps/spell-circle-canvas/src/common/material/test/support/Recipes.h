#pragma once

/** @file
 * THE SMALLEST RECIPE WITH TWO PARAMETERS, for the cases about how a
 * recipe is keyed, cached, stacked and set: a scale and a colour, and a
 * body that multiplies one by the other. Beside it, what the library
 * writes to standard error while a call runs, for the cases whose claim
 * is a warning.
 */

#include <gtest/gtest.h>
#include <sigilmaterial/advanced/Recipe.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Target.h>

#include <functional>
#include <memory>
#include <string>

namespace {

struct TwoParameters {
  float uScale;
  sigil::material::Color uColor;
};

/** The SkSL body of the two-parameter recipe: the colour scaled. */
constexpr const char* kTwoBody =
    "half4 main(float2 p) { return half4(uColor * uScale); }";

std::shared_ptr<const sigil::material::Recipe> twoRecipe(
    const char* name = "two") {
  using sigil::material::Recipe;
  return std::make_shared<const Recipe>(Recipe::of<TwoParameters>(name).body(
      sigil::material::Target::SkSL, kTwoBody));
}

/** Everything written to standard error while @p call runs. */
inline std::string captureStandardError(const std::function<void()>& call) {
  testing::internal::CaptureStderr();
  call();
  return testing::internal::GetCapturedStderr();
}

}  // namespace
