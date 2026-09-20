#pragma once

/** @file
 * @ingroup draw-pen
 *
 * p5's constants, spelled as p5 spells them: the words a verb takes, the
 * caps and joins, the shape kinds, the angle and colour models, and the
 * angles.
 */

#include <cstdint>

namespace sigil::draw {

/** THE WORDS A VERB TAKES — one enumeration for all of them, because p5
 *  keeps one namespace and `CENTER` is the same word to four verbs.
 *  `POLYGON` is the one word p5 does not spell: what `beginShape()` with
 *  no kind means.
 *  @silent the verb does not take the word it was handed, as in p5. */
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
  // fill and stroke: what a material's coordinates are measured against
  CANVAS,
  SHAPE,
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

/** @name p5's angle constants
 *  Half a turn, a whole turn under both of its names, and the two
 *  fractions a sketch reaches for, all in the float the pen computes
 *  in. They are radians whatever the pen's angle mode is, since a
 *  constant cannot be read in a mode.
 *  @{ */
inline constexpr float PI = 3.14159265358979323846f;  ///< half a turn
inline constexpr float TWO_PI = 2.0f * PI;             ///< a whole turn
inline constexpr float TAU = TWO_PI;                   ///< a whole turn
inline constexpr float HALF_PI = 0.5f * PI;            ///< a quarter turn
inline constexpr float QUARTER_PI = 0.25f * PI;        ///< an eighth of a turn
/** @} */

}  // namespace sigil::draw
