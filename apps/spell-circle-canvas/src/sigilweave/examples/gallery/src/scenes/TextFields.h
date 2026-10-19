#pragma once

// Animated fields over one ABI of origin, extent, time and motion, as
// programs a glyph run is filled with — the bodies the text paints study
// carries, held here so what this code draws with is its own.

#include <include/core/SkRect.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/advanced/Recipe.h>

#include <algorithm>
#include <cmath>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <string_view>

namespace text_fields {

/** The ABI every text paint shares: where the run sits, how large it is,
 *  the clock, and a slow two-axis drift derived from it. */
struct TextPaintParameters {
  glm::vec2 origin;
  glm::vec2 extent;
  float time;
  glm::vec2 motion;
};

/** The parameters every field reads, from the box it fills and the
 *  moment: the motion is a slow Lissajous the fields drift along. */
inline TextPaintParameters fieldParameters(const SkRect& bounds,
                                           float timeSeconds) {
  return {{bounds.left(), bounds.top()},
          {std::max(1.0f, bounds.width()), std::max(1.0f, bounds.height())},
          timeSeconds,
          {std::sin(timeSeconds * 0.83f), std::cos(timeSeconds * 0.61f)}};
}

inline std::shared_ptr<const sigil::material::Recipe> fieldRecipe(
    const char* name, std::string_view body) {
  using sigil::material::Recipe;
  using sigil::material::Target;
  return std::make_shared<const Recipe>(
      Recipe::of<TextPaintParameters>(name).body(Target::SkSL,
                                                 std::string(body)));
}

inline constexpr std::string_view kWaterSkSL = R"SHADER(
half4 main(float2 point) {
  float2 uv = (point - origin) / extent;
  float waveX = sin(uv.y * 18.0 - time * 2.1 + motion.x);
  float waveY = cos(uv.x * 15.0 + time * 1.6 + motion.y);
  float2 warped = uv + float2(waveX, waveY) * 0.035;

  half3 deep = half3(0.015, 0.10, 0.30);
  half3 mid = half3(0.02, 0.42, 0.68);
  half3 aqua = half3(0.08, 0.82, 0.80);
  half3 color = mix(deep, mid, half(smoothstep(-0.05, 0.72, warped.y)));
  color =
      mix(color, aqua, half(0.30 + 0.22 * sin(warped.x * 8.0 - time * 0.7)));

  float crossing = sin((warped.x + warped.y) * 34.0 + time * 2.0) *
                   cos((warped.x - warped.y) * 27.0 - time * 1.4);
  float caustic = pow(max(0.0, crossing), 7.0);
  color += half3(0.30, 0.48, 0.40) * half(caustic);
  return half4(color, 1.0);
}
)SHADER";
inline constexpr std::string_view kMeshGradientSkSL = R"SHADER(
half4 main(float2 point) {
  float2 uv = (point - origin) / extent;
  float2 q = uv;
  q.x +=
      (q.y - 0.5) * (0.11 * motion.x) + q.y * (1.0 - q.y) * (0.08 * motion.y);
  q.y +=
      (q.x - 0.5) * (0.09 * motion.y) - q.x * (1.0 - q.x) * (0.06 * motion.x);
  float sx = smoothstep(-0.12, 1.12, q.x);
  float sy = smoothstep(-0.12, 1.12, q.y);

  half3 top = mix(half3(0.96, 0.20, 0.42), half3(0.98, 0.67, 0.18), half(sx));
  half3 bottom =
      mix(half3(0.27, 0.12, 0.70), half3(0.02, 0.78, 0.76), half(sx));
  half3 color = mix(top, bottom, half(sy));

  float2 center = float2(0.52 + 0.18 * motion.x, 0.48 + 0.16 * motion.y);
  float bloom = 1.0 - smoothstep(0.0, 0.62, length(q - center));
  color = mix(color, half3(0.53, 0.30, 0.94), half(bloom * 0.34));
  return half4(color, 1.0);
}
)SHADER";
inline constexpr std::string_view kSparkleSkSL = R"SHADER(
float sparkleHash(float2 p) {
  return fract(sin(dot(p, float2(41.3, 289.1))) * 43758.5453123);
}

half4 main(float2 point) {
  float cellSize = 22.0;
  float2 uv = point - origin + float2(time * 3.0, time * 1.3);
  float2 cell = floor(uv / cellSize);
  float2 local = fract(uv / cellSize) - 0.5;

  float exists = step(0.6, sparkleHash(cell + 3.0));
  float sizeSeed = sparkleHash(cell + 11.0);
  float brightSeed = sparkleHash(cell + 19.0);
  float phaseSeed = sparkleHash(cell + 27.0);
  float rateSeed = sparkleHash(cell + 5.0);
  float2 jitter = float2(sparkleHash(cell), sparkleHash(cell + 41.0)) - 0.5;

  float dist = length(local - jitter * 0.7);
  float radius = mix(0.035, 0.14, sizeSeed);
  float twinkleRate = mix(1.1, 3.6, rateSeed) + motion.x * 0.15;
  float twinklePhase = phaseSeed * 6.2831853;
  float twinkle = 0.5 + 0.5 * sin(time * twinkleRate + twinklePhase);
  twinkle = twinkle * twinkle * (3.0 - 2.0 * twinkle);

  float core = smoothstep(radius, 0.0, dist);
  float halo = smoothstep(radius * 5.5, 0.0, dist) * 0.30;
  float brightness =
      (core + halo) * mix(0.6, 2.2, brightSeed) * twinkle * exists;

  half3 tint =
      mix(half3(0.80, 0.88, 1.0), half3(1.0, 0.92, 0.75), half(brightSeed));
  half alpha = half(clamp(brightness, 0.0, 1.0));
  return half4(tint * alpha, alpha);
}
)SHADER";
inline constexpr std::string_view kStarsSkSL = R"SHADER(
// STARS: a deep field of point stars in layers, from far to near. Each
// layer seats at most one star in each square cell of its own lattice: the
// far layers are finer, their stars smaller and dimmer, the near layers
// coarser and brighter. Every layer slides across the box at a pace that
// grows with its nearness and sways along the drift by as much, so the
// near stars pass in front of the far ones. Under them lies a haze of two
// colours, so a thin stroke that catches no star still holds colour.
const int starLayers = 5;

// A value in [0, 1) for a lattice cell and a salt. The cell is carried
// along two irrational steps and the salt along a third, and the fraction
// it lands on is folded through a steep quadratic twice, so neighbouring
// cells and neighbouring salts answer unrelated values.
float starScatter(float2 cell, float salt) {
  float landing = fract(dot(cell, float2(0.7548776662, 0.5698402910)) +
                        salt * 0.6180339887);
  landing = fract(landing * (landing * 173.31 + 47.13));
  return fract(landing * (landing * 211.79 + 13.97));
}

half4 main(float2 point) {
  float2 drift = motion;
  // Shares of the box's height, so a cell is square in the box.
  float2 position = (point - origin) / extent.y;

  float3 light = float3(0.0);
  for (int layer = 0; layer < starLayers; layer++) {
    float nearness = float(layer + 1) / float(starLayers);
    float salt = float(layer) * 8.0;
    float cellsPerHeight = mix(30.0, 6.0, nearness);
    float2 slide = float2(time * 0.04, time * 0.006) * nearness +
                   drift * (0.05 * nearness) + float2(salt * 1.3, salt * 0.7);
    float2 lattice = (position + slide) * cellsPerHeight;
    float2 cell = floor(lattice);
    float2 inside = fract(lattice) - 0.5;

    // Two cells in five hold a star in the far layers, one in three near.
    float present = step(mix(0.6, 0.67, nearness), starScatter(cell, salt));
    // The star sits within a quarter cell of the centre and its glow
    // reaches a quarter cell, so neither crosses into the next cell.
    float2 seat = (float2(starScatter(cell, salt + 1.0),
                          starScatter(cell, salt + 2.0)) -
                   0.5) *
                  0.5;
    float reach = length(inside - seat);
    float radius = mix(0.035, 0.09, starScatter(cell, salt + 3.0));
    float core = exp(-(reach * reach) / (radius * radius));
    float glow = smoothstep(0.25, 0.0, reach);
    glow *= glow * 0.22;

    // A star dims with its distance: the farthest layer keeps a little over
    // a quarter of the nearest layer's light.
    float strength = mix(0.25, 1.0, nearness * nearness) *
                     mix(0.6, 1.8, starScatter(cell, salt + 4.0));
    float twinkle =
        0.78 + 0.22 * sin(time * mix(1.3, 3.9, starScatter(cell, salt + 5.0)) +
                          starScatter(cell, salt + 6.0) * 6.2831853);
    float3 temperature = mix(float3(0.66, 0.78, 1.0), float3(1.0, 0.84, 0.64),
                             starScatter(cell, salt + 7.0));
    light += temperature * (core + glow) * strength * twinkle * present;
  }

  // The haze: two broad interfering waves that sway with the drift, laying
  // violet and teal over a deep blue ground. It stays darker than the
  // stars and lighter than the dark a run of type is set on, so the
  // letters read by the haze and the stars stand out on them.
  float2 swayed = position + drift * 0.04;
  float violet =
      0.5 + 0.5 * sin(swayed.x * 2.9 + sin(swayed.y * 2.1 + 1.3) * 1.6);
  float teal = 0.5 + 0.5 * sin(swayed.y * 3.7 - swayed.x * 1.4 + 2.2);
  float3 ground = float3(0.10, 0.10, 0.24) +
                  float3(0.34, 0.12, 0.40) * violet * violet +
                  float3(0.02, 0.24, 0.26) * teal * teal;
  return half4(half3(clamp(ground + light, 0.0, 1.0)), 1.0);
}
)SHADER";
inline constexpr std::string_view kCloudsSkSL = R"SHADER(
// CLOUDS: a sky deepening toward the top of the box under one layer of
// cloud. The cloud is a sum of smooth value noise at five rising
// frequencies, each half as strong as the one before, read through a
// gentle warp of the same sum so its edges billow. The layer travels
// slowly across the box and sways along the drift. It is lit from above
// and to one side: a second reading a short step toward the light says how
// much more cloud stands between a point and the light, and that much
// shadow greys the point.

// A value in [0, 1) for a lattice corner. The corner is carried along two
// irrational steps, and the fraction it lands on is folded through a steep
// quadratic twice, so neighbouring corners answer unrelated values.
float cloudScatter(float2 corner) {
  float landing =
      fract(dot(corner, float2(0.5698402910, 0.7548776662)) + 0.31);
  landing = fract(landing * (landing * 157.37 + 61.19));
  return fract(landing * (landing * 229.43 + 17.71));
}

// Smooth noise: the scattered values of the four lattice corners around a
// point, blended by a quintic ease whose slope and curvature are zero at
// each corner, so the surface shows no creases along the lattice.
float cloudValueNoise(float2 position) {
  float2 corner = floor(position);
  float2 within = fract(position);
  float2 ease =
      within * within * within * (within * (within * 6.0 - 15.0) + 10.0);
  float lowerLeft = cloudScatter(corner);
  float lowerRight = cloudScatter(corner + float2(1.0, 0.0));
  float upperLeft = cloudScatter(corner + float2(0.0, 1.0));
  float upperRight = cloudScatter(corner + float2(1.0, 1.0));
  return mix(mix(lowerLeft, lowerRight, ease.x),
             mix(upperLeft, upperRight, ease.x), ease.y);
}

// Five octaves of the noise. Each octave's frequency is a little over
// twice the one before and its lattice is turned half a radian and
// shifted, so no two octaves' lattices line up; the sum stays in [0, 1).
float cloudOctaves(float2 position) {
  float total = 0.0;
  float strength = 0.5;
  float turnCosine = cos(0.5);
  float turnSine = sin(0.5);
  for (int octave = 0; octave < 5; octave++) {
    total += strength * cloudValueNoise(position);
    position = float2(turnCosine * position.x - turnSine * position.y,
                      turnSine * position.x + turnCosine * position.y) *
                   2.07 +
               float2(4.3, 1.9);
    strength *= 0.5;
  }
  return total;
}

half4 main(float2 point) {
  float2 drift = motion;
  float2 position = (point - origin) / extent;
  // Two and a half noise cells to the box's height, square in the box.
  float2 field = position * float2(extent.x / extent.y, 1.0) * 2.5;
  field += float2(time * 0.06, time * 0.01) + drift * 0.12;

  float2 warp = float2(cloudOctaves(field * 0.7 + 3.1),
                       cloudOctaves(field * 0.7 + 8.7)) -
                0.5;
  float2 billowed = field + warp * 1.1;
  float density = cloudOctaves(billowed);
  float towardLight = cloudOctaves(billowed + float2(0.05, -0.09));

  float cover = 0.41;
  float cloud = smoothstep(cover, cover + 0.16, density);
  float lit = clamp(0.62 + (density - towardLight) * 5.0, 0.0, 1.0);
  float thickness = smoothstep(cover + 0.06, cover + 0.3, density);
  float shade = lit * mix(1.0, 0.78, thickness);

  float3 sky = mix(float3(0.13, 0.32, 0.66), float3(0.60, 0.78, 0.94),
                   smoothstep(0.0, 1.0, position.y));
  float3 cloudColour =
      mix(float3(0.46, 0.52, 0.64), float3(1.0, 0.98, 0.94), shade);
  return half4(half3(mix(sky, cloudColour, cloud)), 1.0);
}
)SHADER";
inline constexpr std::string_view kTunnelSkSL = R"SHADER(
// TUNNEL: looking down a round tunnel. A point's angle around the centre
// and its distance from it name a place on the wall: depth is the inverse
// of the distance, so equal steps of depth crowd toward the far end. The
// wall is banded in rings along the depth, dark gaps between bright bands,
// and cut by seams into panels around the angle; the rings stream toward
// the viewer with time, the panels twist with depth, and the centre
// wanders with the drift. Toward the far end the wall fades into a glow.
half4 main(float2 point) {
  float2 drift = motion;
  // Shares of the box's height, so the tunnel is round in the box.
  float2 position = (point - origin) / extent.y;
  float2 centre = float2(0.5 * extent.x / extent.y, 0.5) + drift * 0.07;
  float2 fromCentre = position - centre;
  float distanceFromCentre = max(length(fromCentre), 0.001);
  float turns = atan(fromCentre.y, fromCentre.x) / 6.2831853;

  float depth = 0.8 / distanceFromCentre;
  float travelled = depth + time * 0.8;
  float ring = 0.5 + 0.5 * cos(travelled * 6.2831853);
  ring = smoothstep(0.2, 0.8, ring);
  float twisted = turns * 9.0 + depth * 0.9 + drift.x * 0.4;
  float seam = smoothstep(0.82, 1.0, abs(fract(twisted) - 0.5) * 2.0);
  float wall = ring * (1.0 - 0.75 * seam);

  // Where the rings crowd too finely to resolve, the wall settles to its
  // mean, and its light falls away toward the far end.
  float resolved = smoothstep(0.08, 0.22, distanceFromCentre);
  wall = mix(0.4, wall, resolved);
  float nearLight = smoothstep(0.0, 0.5, distanceFromCentre);

  float band = floor(travelled);
  float3 cool = float3(0.10, 0.55, 1.0);
  float3 warm = float3(0.95, 0.30, 0.85);
  float3 hue = mix(cool, warm, 0.5 + 0.5 * sin(band * 1.9));
  float farGlow = exp(-distanceFromCentre * 14.0) * 0.7;
  float3 color = hue * wall * mix(0.12, 1.25, nearLight) +
                 float3(0.85, 0.75, 1.0) * farGlow + float3(0.02, 0.025, 0.06);
  return half4(half3(clamp(color, 0.0, 1.0)), 1.0);
}
)SHADER";

inline sigil::material::Material water(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.water", kWaterSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

inline sigil::material::Material meshGradient(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.meshGradient", kMeshGradientSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

inline sigil::material::Material sparkle(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.sparkle", kSparkleSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

inline sigil::material::Material stars(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.stars", kStarsSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

inline sigil::material::Material clouds(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.clouds", kCloudsSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

inline sigil::material::Material tunnel(const SkRect& bounds, float timeSeconds) {
  static const std::shared_ptr<const sigil::material::Recipe> recipe =
      fieldRecipe("text_fields.tunnel", kTunnelSkSL);
  return sigil::material::Material(recipe, fieldParameters(bounds, timeSeconds));
}

}  // namespace text_fields
