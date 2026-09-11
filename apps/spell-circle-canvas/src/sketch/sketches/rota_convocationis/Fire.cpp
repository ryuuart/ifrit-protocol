#include "RotaConvocationis.h"

auto RotaConvocationis::strike(double tc, double at, float rest) const
    -> float {
  if (tc < at) return 0.0f;
  const double since = tc - at;
  const float onset = (float)std::min(1.0, since / 0.30);
  const float flash = (float)std::exp(-since / 0.34) * 1.0f;
  const float charged =
      tc > tIgnite ? 0.10f + 0.05f * (float)std::sin((tc - tIgnite) * 1.35)
                   : 0.0f;
  return std::min(1.0f,
                  onset * (rest + charged) + flash * (1.0f - rest * 0.4f));
}

auto RotaConvocationis::stepFire(double tc) -> void {
  litRim = strike(tc, tVox + voxSpanS, 0.34f);
  litNom = strike(tc, tNames + nomSpanS, 0.30f);
  litArc = strike(tc, tArc + 1.1, 0.30f);
  litSpur = strike(tc, tVox + voxSpanS * 0.5, 0.30f);
  litStar = strike(tc, tStar + 1.1, 0.42f);
  litInner = strike(tc, tInner + 1.1, 0.36f);
  litHub = strike(tc, tHub + hubSpanS * 0.9, 0.46f);
  for (int k = 0; k < kSeals; ++k)
    litSeal[k] = strike(tc, tSeal[k] + 0.25 + sealSpanS, 0.30f);

  // The morph walks its ladder over the strike's own half-second. The
  // walk runs from one step BEFORE the first to one PAST the last, so
  // the ladder is dark at both ends: nothing of the scribble stands on
  // the plate before the strike, and nothing of it is left behind when
  // the emissive stack takes the figure over.
  const double morphAt = tStar + 1.1;
  morphStep = -1.0f + (float)(std::clamp((tc - morphAt) / 0.78, 0.0, 1.0) *
                              (double)(starSteps.size() + 1));

  // IGNITION IS FULL-FRAME. The rays are the widest and the shortest,
  // the flood the deepest, the fringe half a second at the crest alone.
  raysA = 0.85f * burst(tc, tIgnite + 0.15, 0.22, 0.85);
  floodA = 0.9f * burst(tc, tIgnite + 0.1, 0.30, 1.5) +
           (tc > tIgnite + 2.0
                ? 0.06f + 0.03f * (float)std::sin((tc - tIgnite) * 1.35)
                : 0.0f);
  const float fringe = 1.7f * burst(tc, tIgnite + 0.26, 0.09, 0.20);
  fringeK = fringe;
  fringeA = fringe > 0.02f ? 1.0f : 0.0f;
  humScale = tc > tIgnite
                 ? 1.0f + 0.005f * (float)std::sin((tc - tIgnite) * 1.35 - 1.57)
                 : 1.0f;

  stepEmbers(tc);
}

auto RotaConvocationis::stepEmbers(double tc) -> void {
  if (!embers) return;
  const double since = tc - tIgnite;
  emberA = since < 0 ? 0.0f
                     : std::min(1.0f, (float)(since / 0.25)) *
                           (0.30f + 0.70f * burst(tc, tIgnite, 0.25, 1.1));
  if (emberA.value() <= 0.0f) return;
  auto positions = embers->positions();
  auto scales = embers->scales();
  auto tints = embers->tints();
  auto frames = embers->frames();
  for (int i = 0; i < kEmbers; ++i) {
    const float u = (float)i / (float)kEmbers;
    const float seed = std::fmod(u * 7919.0f, 1.0f);
    const float life = 2.2f + seed * 2.4f;
    const float age = (float)std::fmod(std::max(0.0, since) + u * life, life);
    const float t = age / life;
    const float th = (u * 360.0f + seed * 53.0f) * kDeg;
    // Struck off the RIM and rising: the sparks belong to the edge of
    // the figure, not to the field it encloses, and lettering is not
    // improved by fireflies over it.
    const float r = kR * (0.90f + 0.10f * seed);
    const float sway = std::sin(age * 1.9f + seed * 6.28f) * 13.0f;
    positions[(size_t)i] = {kEye.x() + r * std::sin(th) + sway,
                            kEye.y() - r * std::cos(th) - t * 190.0f};
    scales[(size_t)i] = (0.45f + seed * 0.75f) * (1.0f - t * 0.5f);
    const float a = std::pow(1.0f - t, 1.4f);
    tints[(size_t)i] = {1.0f, 0.93f - 0.12f * seed, 0.74f, a};
    frames[(size_t)i] = emberFrame;
  }
}
