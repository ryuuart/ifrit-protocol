#pragma once

/** @file
 * @ingroup draw-pen
 *
 * p5's `noise()` as a value: a seeded, layered field over core's lattice
 * mixer.
 */

#include <cstdint>

namespace sigil::draw {

/** A SMOOTH RANDOM FIELD, sampled anywhere in one, two or three
 *  dimensions and always answering the same number at the same place
 *  for the same seed. It has p5's shape — octaves at doubling frequency
 *  with a falloff between them, a value in [0, 1) — over this
 *  repository's own lattice mixer.
 *  @trap `geometry::path::valueNoise` is a DIFFERENT FIELD over the same
 *  mixer, signed and eased differently, not a rounding of this one. */
class NoiseField {
 public:
  explicit NoiseField(uint32_t seed = 0) : m_seed(seed) {}

  /** Re-seeds the whole field; the same seed gives back the same field. */
  void seed(uint32_t seed) { m_seed = seed; }
  /** How many octaves are layered and how much each successive one
   *  contributes relative to the one before — p5's `noiseDetail`. */
  void detail(int octaves, float falloff);

  [[nodiscard]] float at(float x, float y = 0.0f, float z = 0.0f) const;

  /** WHAT ONE LATTICE CORNER HOLDS: core's `lattice` word for the seed
   *  and the integer position, squeezed to [0, 1) through the 24 bits a
   *  float holds exactly. At an integer position with one octave the
   *  field IS this value, which is what pins it to the mixer. */
  [[nodiscard]] static float corner(uint32_t seed, int x, int y, int z);

  [[nodiscard]] uint32_t seed() const { return m_seed; }
  [[nodiscard]] int octaves() const { return m_octaves; }
  [[nodiscard]] float falloff() const { return m_falloff; }

 private:
  uint32_t m_seed;
  int m_octaves = 4;
  float m_falloff = 0.5f;
};

}  // namespace sigil::draw
