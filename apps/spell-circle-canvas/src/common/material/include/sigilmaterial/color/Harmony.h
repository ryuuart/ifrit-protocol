#pragma once

/** @file
 * COLOURS CHOSEN FROM ONE COLOUR: the hue rotation, and the schemes a
 * painter's wheel names, as one function whose scheme is a prop.
 */

#include <sigilmaterial/color/Color.h>

namespace sigil::material {

/** WHERE THE OTHER COLOURS SIT relative to the one asked about.
 *
 *  Each is a set of angles on the hue circle, and the base is always the
 *  first entry answered, so a caller that wants "this colour and its
 *  opposite" reads entries 0 and 1 whatever scheme it asked for. */
enum class Scheme : uint8_t {
  /** The base and the hue half a turn away: the highest-contrast pair
   *  the wheel holds. Two entries; the spread is not read. */
  Complement,
  /** The base and the two hues either side of its opposite: the contrast
   *  of a complement without the vibration of the exact pair. Three
   *  entries, `spreadDegrees` off the opposite. */
  SplitComplement,
  /** The base and its two neighbours: one family, three steps. Three
   *  entries, `spreadDegrees` apart. */
  Analogous,
  /** Three hues a third of a turn apart. Three entries; the spread is
   *  not read. */
  Triad,
  /** Two complementary pairs — a rectangle on the wheel, which is a
   *  square at a spread of 90. Four entries. */
  Tetrad,
};

/** THE SAME COLOUR AT ANOTHER HUE: @p degrees around the OKLCH circle,
 *  holding the lightness it had.
 *
 *  In OKLCH rather than on the HSV wheel, and the difference is the whole
 *  reason this is a library verb: rotating an HSV hue holds `value`,
 *  which is the largest channel and not a brightness, so a scheme built
 *  that way lands a yellow and a blue at wildly different weights and the
 *  set does not read as one family. Here the only thing that changed is
 *  which colour it is.
 *
 *  A hue sRGB cannot show at the base's chroma gives up CHROMA and keeps
 *  its lightness and its hue, through `fitToSrgb`: the alternative — the
 *  component-wise cut — moves the hue and the lightness both, so a set
 *  built by turning one colour would come back at several hues and
 *  several weights, which is the one thing a harmony exists to avoid.
 *  Alpha is carried through. */
[[nodiscard]] Color rotateHue(const Color& base, float degrees);

/** THE COLOURS @p scheme NAMES around @p base, the base first.
 *
 *  A palette rather than a ramp, because a harmony is a set of colours
 *  with nothing between them: the colour half way between a base and its
 *  complement is a grey nobody asked for.
 *
 *  @p spreadDegrees is how far off its anchor each flanking hue sits. It
 *  is read by `SplitComplement`, `Analogous` and `Tetrad` and ignored by
 *  the schemes whose angles are fixed, the way any value with props
 *  ignores the props its kind does not read. */
[[nodiscard]] Palette harmony(const Color& base, Scheme scheme,
                              float spreadDegrees = 30.0f);

}  // namespace sigil::material
