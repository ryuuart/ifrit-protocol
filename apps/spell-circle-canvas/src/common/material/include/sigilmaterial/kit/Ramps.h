#pragma once

/** @file
 * THE NAMED COLOUR RAMPS: one function per name, each answering the same
 * `Ramp` value.
 *
 * They are stock values over the ramp, not types: a caller takes one,
 * changes its domain to the numbers it is reading, reverses it or eases
 * it, and it is still a ramp that every consumer of one understands. The
 * point of naming them is that these particular stop lists were measured
 * rather than picked — each sequential map below rises steadily in
 * lightness, so a difference in the data is a difference an eye reports
 * and none of the false edges a rainbow puts at its yellow and cyan
 * appear anywhere in it.
 */

#include <sigilmaterial/color/Ramp.h>

namespace sigil::material::kit {

/** SEQUENTIAL — low to high, dark to light, hue turning as it climbs.
 *
 *  The four differ in where they spend their chroma and not in whether
 *  they are even: `viridis` is the blue-green-yellow one that stays
 *  legible in grey and to a colour-blind reader; `magma` and `inferno`
 *  are the black-through-red-to-pale pair, magma cooler and paler at the
 *  top, inferno hotter; `plasma` is the same climb without the black
 *  end, which is what to reach for over a dark ground where a map that
 *  starts at black has nowhere to sit. */
[[nodiscard]] Ramp viridis();
[[nodiscard]] Ramp magma();
[[nodiscard]] Ramp inferno();
[[nodiscard]] Ramp plasma();

/** THE RAINBOW DONE PROPERLY: every hue, and no lightness cliff at the
 *  yellow or the cyan where the classic jet map invents an edge.
 *
 *  Still a rainbow, and it still says nothing about which end is more:
 *  its lightness rises to the middle and falls again, so two values
 *  either side of the middle read as equally bright. Reach for it where
 *  the eye is asked to tell many bands apart — a spectrogram, a depth
 *  slice, a lookup — and for a sequential map where the question is
 *  which value is larger. */
[[nodiscard]] Ramp turbo();

/** DIVERGING — two hues away from a pale middle, for a quantity with a
 *  meaningful zero. The middle is where the value is neither, and it is
 *  the lightest point of the ramp, so the sign reads as the hue and the
 *  magnitude as the depth of it.
 *
 *  Two, because a diverging pair is chosen for what it must survive:
 *  `redBlue` is the warm-cool pair everything from an anomaly to a
 *  swing is drawn in, and `brownTeal` is the pair that stays two colours
 *  for a reader who cannot tell red from green. */
[[nodiscard]] Ramp redBlue();
[[nodiscard]] Ramp brownTeal();

/** WHAT A CUBEHELIX IS MADE OF: a helix walked through the colour cube,
 *  which is what makes it the one named map that is GENERATED rather
 *  than tabulated — the props below are the whole definition, so a
 *  caller can move the hue path without leaving the family. */
struct CubehelixOptions {
  /** Which hue the helix starts on, as a third of a turn: 0 is blue, 1
   *  red, 2 green. */
  float start = 0.5f;
  /** How many turns of the wheel the ramp makes on its way up. Negative
   *  turns the other way, which is the direction Green's original takes.
   */
  float rotations = -1.5f;
  /** How far from grey the helix strays. 0 is a grey ramp; above about
   *  1.5 the helix leaves the cube and the ends clip. */
  float hue = 1.0f;
  /** The lightness path: 1 is even, below 1 lifts the dark end, above 1
   *  holds it down. */
  float gamma = 1.0f;
  /** How many stops the analytic curve is sampled into. The helix is a
   *  smooth function of the position, so this is a resolution and not a
   *  shape: more stops cost memory and change nothing an eye can see
   *  past about thirty. */
  int samples = 32;

  bool operator==(const CubehelixOptions&) const = default;
};

/** THE HELIX AS A RAMP: a lightness that climbs evenly from black to
 *  white with a hue spiralling around it, so the map is still readable
 *  printed in grey — which is what it was built for. */
[[nodiscard]] Ramp cubehelix(const CubehelixOptions& options = {});

}  // namespace sigil::material::kit
