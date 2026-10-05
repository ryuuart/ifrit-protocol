/** @file
 * The one term text, in each language a renderer speaks.
 */

#include "sigilmaterial/advanced/Terms.h"

#include <sigilshaders/MaterialCore.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sigil::material {

namespace {

/** THE MODULE, as a device compiler takes it. A Slang renderer loads
 *  this text into its compiler session under the name `Shading` and its
 *  own shaders import it, which is how one text serves both the
 *  renderer's scaffold and every material body compiled beside it. */
std::string slangSource() { return std::string(shaderSource("Shading.slang")); }

}  // namespace

std::string skSLFromSlang(std::string_view slang) {
  std::string out(slang);
  const auto strip = [&out](std::string_view what) {
    for (size_t at = out.find(what); at != std::string::npos;
         at = out.find(what, at))
      out.erase(at, what.size());
  };
  strip("module Shading;");
  strip("public ");

  const auto isWord = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
  };
  const auto rename = [&](std::string_view from, std::string_view to) {
    for (size_t at = out.find(from); at != std::string::npos;
         at = out.find(from, at)) {
      const bool before = at > 0 && isWord(out[at - 1]);
      const size_t after = at + from.size();
      const bool behind = after < out.size() && isWord(out[after]);
      if (before || behind) {
        at = after;
        continue;
      }
      out.replace(at, from.size(), to);
      at += to.size();
    }
  };
  rename("frac", "fract");
  rename("lerp", "mix");
  // Two arguments, same order in both: Slang's atan2(y, x) is SkSL's
  // two-argument atan(y, x). Whole identifiers only, so `atan2P` — the
  // polynomial the terms use where an equirectangular seam would show — is not
  // touched, and neither is a one-argument atan already spelled the same.
  rename("atan2", "atan");
  return out;
}

const std::string& termsSource(Target target) {
  static const std::string kSlang = slangSource();
  static const std::string kSkSL = skSLFromSlang(kSlang);
  switch (target) {
    case Target::Slang:
      return kSlang;
    case Target::SkSL:
      return kSkSL;
  }
  return kSkSL;
}

namespace {

/** The i-th of @p count points spread evenly over the unit square: an
 *  even step in one axis and the bits of i reversed in the other. */
glm::vec2 hammersley(uint32_t i, uint32_t count) {
  uint32_t bits = i;
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return {((float)i + 0.5f) / (float)count, (float)bits * 2.3283064e-10f};
}

}  // namespace

glm::vec2 environmentBrdf(float roughness, float nDotV) {
  // The same width the terms spread a roughness over, and the same
  // visibility, so the table is the integral of what a light shades with.
  const float r = std::fmin(std::fmax(roughness, 0.0f), 1.0f);
  const float a = std::fmax(r * r, 1e-3f);
  const float a2 = a * a;
  const float v = std::fmin(std::fmax(nDotV, 1e-3f), 1.0f);
  const float vx = std::sqrt(1.0f - v * v);
  constexpr uint32_t kSamples = 512;
  double scale = 0;
  double bias = 0;
  for (uint32_t i = 0; i < kSamples; ++i) {
    // A half vector drawn from the distribution itself, so a narrow lobe
    // is sampled where it stands.
    const glm::vec2 u = hammersley(i, kSamples);
    const float phi = 6.2831853f * u.x;
    const float cosH = std::sqrt((1.0f - u.y) / (1.0f + (a2 - 1.0f) * u.y));
    const float sinH = std::sqrt(std::fmax(1.0f - cosH * cosH, 0.0f));
    const float hx = sinH * std::cos(phi);
    const float vh = vx * hx + v * cosH;
    const float lz = 2.0f * vh * cosH - v;
    if (lz <= 0.0f || vh <= 0.0f) continue;
    const float lambdaV = lz * std::sqrt(v * v * (1.0f - a2) + a2);
    const float lambdaL = v * std::sqrt(lz * lz * (1.0f - a2) + a2);
    const float visibility = 0.5f / (lambdaV + lambdaL);
    // The sample's weight is the lobe over the density it was drawn
    // with: visibility times four, the light's cosine and the half
    // vector's angle to the eye, over its angle to the normal.
    const float weight = visibility * 4.0f * lz * vh / cosH;
    const float c = 1.0f - vh;
    const float c2 = c * c;
    const float fresnel = c2 * c2 * c;
    scale += (1.0f - fresnel) * weight;
    bias += fresnel * weight;
  }
  return {(float)(scale / kSamples), (float)(bias / kSamples)};
}

const std::vector<glm::vec2>& environmentBrdfTable() {
  static const std::vector<glm::vec2> table = [] {
    std::vector<glm::vec2> out;
    out.reserve((size_t)kEnvironmentBrdfSize * kEnvironmentBrdfSize);
    const float last = (float)(kEnvironmentBrdfSize - 1);
    for (int y = 0; y < kEnvironmentBrdfSize; ++y)
      for (int x = 0; x < kEnvironmentBrdfSize; ++x)
        out.push_back(environmentBrdf((float)y / last, (float)x / last));
    return out;
  }();
  return table;
}

namespace {

/** The exponent of the sheen's fitted shadowing, as `sheenLambdaExponent`
 *  writes it. */
double sheenLambdaExponent(double x, double alpha) {
  const double smooth = (1.0 - alpha) * (1.0 - alpha);
  const double a = 21.5473 + (25.32450 - 21.5473) * smooth;
  const double b = 3.82987 + (3.32435 - 3.82987) * smooth;
  const double c = 0.19823 + (0.16801 - 0.19823) * smooth;
  const double d = -1.97760 + (-1.27393 + 1.97760) * smooth;
  const double e = -4.32054 + (-4.85967 + 4.32054) * smooth;
  return a / (1.0 + b * std::pow(std::fmax(x, 0.0), c)) + d * x + e;
}

double sheenLambda(double cosTheta, double alpha) {
  const double c = std::fabs(cosTheta);
  if (c < 0.5) return std::exp(sheenLambdaExponent(c, alpha));
  return std::exp(2.0 * sheenLambdaExponent(0.5, alpha) -
                  sheenLambdaExponent(1.0 - c, alpha));
}

}  // namespace

float sheenAlbedo(float sheenRoughness, float nDotV) {
  // The same distribution and visibility the terms light a sheen with,
  // integrated over the hemisphere on an even grid in the polar angle and
  // the azimuth, each cell weighted by the solid angle it covers. The lobe
  // is broad at every roughness, so an even grid holds it; the azimuth is
  // mirror-symmetric about the plane of the eye and is integrated over
  // half a turn and doubled.
  const double r = std::fmin(std::fmax(sheenRoughness, 1e-3f), 1.0f);
  const double alpha = r * r;
  const double inverse = 1.0 / alpha;
  const double v = std::fmin(std::fmax(nDotV, 1e-3f), 1.0f);
  const double vx = std::sqrt(1.0 - v * v);
  const double lambdaV = sheenLambda(v, alpha);
  constexpr int kPolar = 64;
  constexpr int kAzimuth = 32;
  const double dTheta = 1.5707963267948966 / kPolar;
  const double dPhi = 3.141592653589793 / kAzimuth;
  double sum = 0;
  for (int i = 0; i < kPolar; ++i) {
    const double theta = (i + 0.5) * dTheta;
    const double l = std::cos(theta);
    const double sinTheta = std::sin(theta);
    const double lambdaL = sheenLambda(l, alpha);
    const double visibility = std::fmin(
        1.0 / ((1.0 + lambdaV + lambdaL) * std::fmax(4.0 * v * l, 5e-8)), 1.0);
    for (int j = 0; j < kAzimuth; ++j) {
      const double phi = (j + 0.5) * dPhi;
      const double lx = sinTheta * std::cos(phi);
      const double ly = sinTheta * std::sin(phi);
      const double hx = lx + vx;
      const double hy = ly;
      const double hz = l + v;
      const double length = std::sqrt(hx * hx + hy * hy + hz * hz);
      const double nh = length > 0 ? hz / length : 1.0;
      const double s2 = std::fmax(1.0 - nh * nh, 0.0078125);
      const double distribution =
          (2.0 + inverse) * std::pow(s2, inverse * 0.5) / 6.283185307179586;
      sum += distribution * visibility * l * sinTheta;
    }
  }
  return (float)(2.0 * sum * dTheta * dPhi);
}

const std::vector<float>& sheenAlbedoTable() {
  static const std::vector<float> table = [] {
    std::vector<float> out;
    out.reserve((size_t)kEnvironmentBrdfSize * kEnvironmentBrdfSize);
    const float last = (float)(kEnvironmentBrdfSize - 1);
    // Within a few degrees of grazing the sheen's fitted visibility holds
    // at one where the exact one would fall, and its albedo there rises
    // past one; the table holds it to one, so a sheen never returns more
    // than arrives.
    for (int y = 0; y < kEnvironmentBrdfSize; ++y)
      for (int x = 0; x < kEnvironmentBrdfSize; ++x)
        out.push_back(
            std::fmin(sheenAlbedo((float)y / last, (float)x / last), 1.0f));
    return out;
  }();
  return table;
}

}  // namespace sigil::material
