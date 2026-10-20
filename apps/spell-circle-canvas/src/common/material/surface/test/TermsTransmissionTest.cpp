/** @file
 * LIGHT THROUGH A SURFACE: what is reflected and what is transmitted
 * never exceed what arrived, and dispersion lands the three channels in
 * order across an edge.
 */

#include <string>

#include "support/TermEvaluation.h"

using namespace sigil::material;
using namespace sigil::material::test;

TEST(Terms, ReflectedAndTransmittedLightNeverExceedWhatArrived) {
  for (const float reflected : {0.0f, 0.04f, 0.3f, 0.9f, 1.0f})
    for (const float weight : {0.0f, 0.25f, 1.0f, 1.5f}) {
      const std::string expression =
          std::to_string(reflected) + " + transmittedShare(float3(" +
          std::to_string(reflected) + ", 0.0, 0.0), " + std::to_string(weight) +
          ").r";
      EXPECT_LE(scalar(expression), 1.0f + 2e-3f) << expression;
    }
}

TEST(Terms, DispersionOrdersTheThreeOffsetsAcrossAnEdge) {
  // Seen obliquely through dispersing glass, red is carried across least
  // and blue most, so the three channels land in order beside one another.
  const auto across = [](const char* channel) {
    return scalar(std::string("-refractedTravel(normalize(float3(1.0, 0.0, "
                              "1.0)), float3(0.0, 0.0, 1.0), dispersedIors("
                              "1.5, 1.0).") +
                  channel + ", 1.0).x");
  };
  EXPECT_GT(across("r"), across("g"));
  EXPECT_GT(across("g"), across("b"));
}
