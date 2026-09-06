/** @file
 * The named ramps: the measured stop lists, and the helix that is
 * computed rather than listed.
 */

#include "sigilmaterial/kit/Ramps.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace sigil::material::kit {
namespace {

/** A table of packed colours, evenly spaced from 0 to 1.
 *
 *  Evenly spaced because that is how every measured map below is
 *  published: the table IS the map sampled at equal steps, and moving a
 *  stop would be claiming the map turns somewhere it does not. The walk
 *  between them is perceptual, which is what keeps a map that was
 *  measured for even steps even between the samples that were kept. */
Ramp evenly(std::initializer_list<uint32_t> colors) {
  Ramp ramp;
  ramp.space = RampSpace::Oklab;
  const size_t count = colors.size();
  ramp.stops.reserve(count);
  size_t index = 0;
  for (uint32_t packed : colors) {
    ramp.stops.push_back(
        {count > 1 ? (float)index / (float)(count - 1) : 0.0f, rgb(packed)});
    ++index;
  }
  return ramp;
}

}  // namespace

Ramp viridis() {
  return evenly({0x440154, 0x46327e, 0x3b528b, 0x2c728e, 0x21918c, 0x28ae80,
                 0x5ec962, 0xaddc30, 0xfde725});
}

Ramp magma() {
  return evenly({0x000004, 0x1c1044, 0x440f76, 0x721f81, 0x9e2f7f, 0xcd4071,
                 0xf1605d, 0xfe9f6d, 0xfcfdbf});
}

Ramp inferno() {
  return evenly({0x000004, 0x1b0c41, 0x4a0c6b, 0x781c6d, 0xa52c60, 0xcf4446,
                 0xed6925, 0xfb9906, 0xfcffa4});
}

Ramp plasma() {
  return evenly({0x0d0887, 0x41049d, 0x6a00a8, 0x8f0da4, 0xb12a90, 0xcc4778,
                 0xe16462, 0xf2844b, 0xf0f921});
}

Ramp turbo() {
  return evenly({0x30123b, 0x4777ef, 0x1bd0d5, 0x62fc6b, 0xd2e935, 0xfe9b2d,
                 0xdb3a07, 0x7a0403});
}

Ramp redBlue() {
  return evenly({0xb2182b, 0xd6604d, 0xf4a582, 0xfddbc7, 0xf7f7f7, 0xd1e5f0,
                 0x92c5de, 0x4393c3, 0x2166ac});
}

Ramp brownTeal() {
  return evenly({0x8c510a, 0xbf812d, 0xdfc27d, 0xf6e8c3, 0xf5f5f5, 0xc7eae5,
                 0x80cdc1, 0x35978f, 0x01665e});
}

Ramp cubehelix(const CubehelixOptions& options) {
  Ramp ramp;
  // The stops are the curve sampled, so the walk between two of them is
  // a straight line in the space the curve was drawn in — sRGB — and
  // not a second, different curve through OKLab.
  ramp.space = RampSpace::Srgb;
  const int samples = std::max(options.samples, 2);
  ramp.stops.reserve((size_t)samples);
  for (int i = 0; i < samples; ++i) {
    const float position = (float)i / (float)(samples - 1);
    const float lightness = std::pow(position, options.gamma);
    const float angle =
        2.0f * 3.14159265358979323846f *
        (options.start / 3.0f + 1.0f + options.rotations * position);
    // The amplitude closes to zero at both ends of the lightness ramp,
    // which is what keeps the helix inside the cube where there is no
    // room left for chroma.
    const float amplitude = options.hue * lightness * (1.0f - lightness) * 0.5f;
    const float cosine = std::cos(angle), sine = std::sin(angle);
    const Color color{
        std::clamp(
            lightness + amplitude * (-0.14861f * cosine + 1.78277f * sine),
            0.0f, 1.0f),
        std::clamp(
            lightness + amplitude * (-0.29227f * cosine - 0.90649f * sine),
            0.0f, 1.0f),
        std::clamp(lightness + amplitude * (1.97294f * cosine), 0.0f, 1.0f),
        1.0f};
    ramp.stops.push_back({position, color});
  }
  return ramp;
}

}  // namespace sigil::material::kit
