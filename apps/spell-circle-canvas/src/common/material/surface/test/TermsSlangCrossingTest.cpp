/** @file
 * A BODY WRITTEN IN SLANG CROSSING INTO SKSL: the four-quadrant
 * arctangent is renamed and nothing else, the crossed text compiles and
 * answers the angle, and only whole identifiers are renamed.
 */

#include <string>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

TEST(Terms, ABodyWrittenWithAtan2CrossesIntoTheTwoArgumentAtan) {
  // Slang spells the four-quadrant arctangent `atan2(y, x)` and SkSL
  // spells it `atan(y, x)`: the same two arguments in the same order, so
  // the crossing is the rename and nothing else.
  const std::string slang =
      "public float bearing(float2 v) { return atan2(v.y, v.x); }\n";
  const std::string sksl = skSLFromSlang(slang);
  EXPECT_EQ(sksl, "float bearing(float2 v) { return atan(v.y, v.x); }\n");

  // And the crossed text compiles and answers the angle: the second
  // quadrant is three quarters of a half turn.
  const SkColor4f answer = shadeBody(
      sksl +
      "half4 main(float2 xy) { return half4("
      "half(bearing(float2(-1.0, 1.0)) / 3.14159274), 0.0, 0.0, 1.0); }");
  EXPECT_NEAR(answer.fR, 0.75f, 2e-3f);

  // Whole identifiers only, which is what lets one table serve the terms
  // as well: their own polynomial arctangent — written out because a
  // library `atan2` is two pieces of code on two targets and an equirectangular
  // lookup that disagreed would seam a reflection — keeps its name.
  EXPECT_NE(termsSource(Target::SkSL).find("float atan2P(float y, float x)"),
            std::string::npos);
}
