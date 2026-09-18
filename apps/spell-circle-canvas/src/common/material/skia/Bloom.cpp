#include <sigilmaterial/skia/Bloom.h>

#include <include/core/SkColorFilter.h>

#include <algorithm>
#include <array>
#include <cstdint>

namespace sigil::material::skia {

namespace {

/** Coverage scaled by @p gain, colour kept: a rung's strength. */
Effect coverageGain(float gain) {
  const float scale[20] = {1, 0, 0, 0, 0,
                           0, 1, 0, 0, 0,
                           0, 0, 1, 0, 0,
                           0, 0, 0, std::max(0.0f, gain), 0};
  return Effect::filter(SkColorFilters::Matrix(scale));
}

/** Coverage held at or below @p ceiling. */
Effect coverageCeiling(float ceiling) {
  std::array<uint8_t, 256> table{};
  const int most = static_cast<int>(std::clamp(ceiling, 0.0f, 1.0f) * 255);
  for (int i = 0; i < 256; ++i)
    table[i] = static_cast<uint8_t>(std::min(i, most));
  return Effect::filter(
      SkColorFilters::TableARGB(table.data(), nullptr, nullptr, nullptr));
}

}  // namespace

Effect bloom(const BloomParameters& p) {
  // Native separable Gaussian filters spread light continuously at wide
  // radii; sparse source gathers leave distinct copies of fine lettering.
  const Effect light = Effect::brightPass(p.threshold, p.knee)
                           .then(Effect::dilate(p.dilation));
  const auto rung = [&](float sigma, float strength) {
    return light.then(Effect::blur(sigma))
        .then(Effect::deepen(p.deepening))
        .then(coverageGain(strength));
  };
  const float sigma = std::max(0.0f, p.sigma);
  const Effect halo =
      rung(sigma, p.strength)
          .emit(rung(sigma * std::max(1.0f, p.spread), p.tail),
                SkBlendMode::kPlus)
          .then(coverageCeiling(p.maxOpacity));
  Effect core;
  if (p.softness > 0) core = Effect::blur(p.softness);
  return core.then(Effect::whiten(p.whitening, p.threshold, p.knee))
      .emit(halo);
}

}  // namespace sigil::material::skia
