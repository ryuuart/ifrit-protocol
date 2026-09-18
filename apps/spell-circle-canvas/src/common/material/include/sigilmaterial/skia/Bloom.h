#pragma once

#include <sigilmaterial/skia/Effect.h>

namespace sigil::material::skia {

/** Optical bloom in local pixels. Sigma sets the near Gaussian; spread
 * multiplies it for the broad halo. Strength and tail weight those two
 * blurs. Softness blurs the source independently of the emitted light. */
struct BloomParameters {
  float sigma = 4;
  float strength = 1;
  float spread = 4;
  float tail = 1;
  float threshold = 0.2f;
  float knee = 0.2f;
  float softness = 0;
  /** The share of the way a lit source moves toward white at its own
   *  peak, as an overexposed core does, so it reads lighter than the
   *  deeper halo around it. Colour below the threshold is untouched. */
  float whitening = 0;
  /** Local pixels the extracted light grows by before either blur, so
   *  the halo's colour extends past the source as a body and then fades,
   *  as a shadow's spread does. Growth is a blur whose coverage is then
   *  raised, so corners stay round and gaps between letters stay open
   *  until the dilation reaches them. */
  float dilation = 0;
  /** How far a fading halo sinks toward its strongest channel, as a tone
   *  curve's toe drops the weaker channels of faint light first: orange
   *  deepens toward red, yellow toward orange, a blue-leaning cyan toward
   *  blue, and the brightest channel holds. The straight colour is raised
   *  to 1 + deepening × (1 − coverage), so a dense halo keeps the source's
   *  colour and a thin one takes the whole depth. */
  float deepening = 0;
  float maxOpacity = 0.65f;  ///< keeps dark lettering visible inside lit panels
};

/** Extract bright colour, spread it with two separable Gaussian filters,
 * then screen the light over the source — `Effect::brightPass`,
 * `dilate`, `blur`, `deepen` and `whiten` joined by `emit`, so a glow
 * that needs another arrangement is written from the same stages. Hold
 * the returned effect: its filter graph compares by identity, and copies
 * share that graph. */
Effect bloom(const BloomParameters& parameters = {});

}  // namespace sigil::material::skia
