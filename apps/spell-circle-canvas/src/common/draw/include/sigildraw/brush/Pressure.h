#pragma once

/** @file
 * The pressure envelope a tool applies along a stroke.
 */

#include <functional>
#include <optional>

namespace sigil::draw::brush {

/** How pressure runs from the start of a stroke to its end.
 *
 *  Three forms, tried in this order: a caller's `curve` over unit
 *  progress; a bell (`gaussian`) whose centre and width are re-rolled per
 *  stroke by their jitters; and the three-point envelope, which
 *  interpolates start to middle over the first half and middle to end
 *  over the second, re-rolled per stroke by `variation` — an offset, a
 *  scale, a warp along the stroke and a tilt from one end to the other.
 *  The re-roll is what makes two strokes with one tool differ. */
struct Pressure {
  struct Gaussian {
    float center = 0.5f;
    float width = 0.5f;
    float sharpness = 3.25f;
    float minimum = 0.0f;
    float maximum = 1.0f;
    float centerJitter = 0.0f;
    float widthJitter = 0.0f;
  };

  struct Variation {
    float offset = 0.08f;
    float scale = 0.08f;
    float warp = 0.06f;
    float tilt = 0.06f;

    bool operator==(const Variation&) const = default;
  };

  float start = 1.0f;
  float middle = 1.0f;
  float end = 1.0f;
  std::function<float(float)> curve;
  std::optional<Gaussian> gaussian;
  std::optional<Variation> variation = Variation{};

  /** The pressure at unit @p progress along the stroke. */
  [[nodiscard]] float at(float progress) const;
  /** A bell between @p minimum and @p maximum whose centre and width move
   *  by up to the two jitters on every stroke. */
  [[nodiscard]] static Pressure gaussianProfile(float centerJitter,
                                                float widthJitter,
                                                float minimum, float maximum);
};

}  // namespace sigil::draw::brush
