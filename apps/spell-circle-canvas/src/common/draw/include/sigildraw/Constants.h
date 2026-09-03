#pragma once

/** @file
 * p5's constants, spelled as p5 spells them: the words a verb takes, the
 * caps and joins, the shape kinds, the angle and colour models, and the
 * angles.
 */

#include <cstdint>

namespace sigil::draw {

/** THE WORDS A VERB TAKES.
 *
 *  One enumeration for all of them, because p5 keeps one namespace for
 *  them and `CENTER` is the same word to rectMode, ellipseMode, imageMode
 *  and textAlign. A verb handed a word it does not take ignores it, as p5
 *  does. `POLYGON` is the one word p5 does not spell: it is what
 *  `beginShape()` with no kind means, and nothing needs to write it. */
enum Constant : uint8_t {
  // rectMode, ellipseMode, imageMode
  CORNER,
  CORNERS,
  CENTER,
  RADIUS,
  // angleMode
  RADIANS,
  DEGREES,
  // colorMode
  RGB,
  HSB,
  HSL,
  // arc
  OPEN,
  CHORD,
  PIE,
  // endShape
  CLOSE,
  // strokeCap and strokeJoin
  ROUND,
  SQUARE,
  PROJECT,
  MITER,
  BEVEL,
  // textAlign
  LEFT,
  RIGHT,
  TOP,
  BOTTOM,
  BASELINE,
  // textStyle
  NORMAL,
  ITALIC,
  BOLD,
  BOLDITALIC,
  // beginShape
  POLYGON,
  POINTS,
  LINES,
  TRIANGLES,
  TRIANGLE_FAN,
  TRIANGLE_STRIP,
  QUADS,
  QUAD_STRIP,
  // blendMode
  BLEND,
  ADD,
  DARKEST,
  LIGHTEST,
  DIFFERENCE,
  EXCLUSION,
  MULTIPLY,
  SCREEN,
  REPLACE,
  REMOVE,
  OVERLAY,
  HARD_LIGHT,
  SOFT_LIGHT,
  DODGE,
  BURN,
  SUBTRACT,
};

inline constexpr float PI = 3.14159265358979323846f;
inline constexpr float TWO_PI = 2.0f * PI;
inline constexpr float TAU = TWO_PI;
inline constexpr float HALF_PI = 0.5f * PI;
inline constexpr float QUARTER_PI = 0.25f * PI;

}  // namespace sigil::draw
