#pragma once

// THE GLOBE: an orthographic attitude ball — sky over ground, a
// graticule, a lit limb — inscribed in the node it fills, one arithmetic
// written in Slang and crossed into SkSL, reading the node's resolution
// because the disc is inscribed in the node. Bind `yaw`, `pitch` and
// `roll` to drive an attitude.

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/program/Shader.h>
#include <sigilmaterial/advanced/Terms.h>

#include <glm/vec4.hpp>
#include <string>
#include <string_view>

namespace ksp_mapview {

using sigil::material::Color;

/** THE GLOBE'S DIALS: the two hemispheres and how each fades toward its
 *  pole, the attitude, the graticule's three pitches and weights, and
 *  the one light. The hemisphere colours are the ones AT THE HORIZON,
 *  with a second pair at the poles; the attitude is in RADIANS, the
 *  graticule's pitches in DEGREES, and every width and feather in
 *  pixels.
 *  @trap The alpha falls to nothing across `edgeFeather`, so nothing
 *  outside the disc is painted. */
struct GlobeParameters {
  Color sky = {0.24f, 0.48f, 0.71f, 1};
  Color skyPole = {0.12f, 0.30f, 0.49f, 1};
  Color ground = {0.54f, 0.42f, 0.24f, 1};
  Color groundPole = {0.29f, 0.21f, 0.13f, 1};
  Color grid = {1, 1, 1, 1};
  glm::vec4 light = {-0.42f, 0.52f, 0.74f, 0.0f};
  float yaw = 0.0f;    ///< radians
  float pitch = 0.0f;  ///< radians
  float roll = 0.0f;   ///< radians
  float horizonBlend = 0.010f;
  float minorDeg = 10.0f;
  float meridianDeg = 90.0f;
  float parallelDeg = 30.0f;
  float lineWidth = 0.85f;  ///< px
  float minorWeight = 0.34f;
  float majorWeight = 0.62f;
  float horizonWeight = 0.95f;
  float ambient = 0.58f;
  float diffuse = 0.42f;
  float specular = 0.16f;
  float shininess = 14.0f;
  float edgeFeather = 2.2f;  ///< px
  float fill = 1.0f;         ///< the inscribed disc's radius the sphere takes
};

inline constexpr std::string_view kGlobePreludeSlang = R"SHADER(
// THE GLOBE, written once and crossed into SkSL by the core: the disc
// inscribed in the node, inverted back onto the near hemisphere, carried
// into the sphere's own frame, and read for its two hemispheres, its
// graticule and its horizon. One text, so a globe on a device and a
// globe on a raster surface are the same ball.
//
// The colour comes back premultiplied by the limb's feather, because the
// alpha the entry returns is that same feather; each target's entry is
// the one line that spells the return in its own types.
float4 globeAt(float2 p) {
  float2 middle = uResolution * 0.5;
  float R = min(middle.x, middle.y) * fill;
  float2 q = (p - middle) / R;
  float rr = dot(q, q);
  if (rr > 1.0) { return float4(0.0, 0.0, 0.0, 0.0); }
  // z = +sqrt(1 - r^2) picks the FRONT of the sphere; the back is never
  // sampled, so no depth test is needed to hide it.
  float z = sqrt(max(0.0, 1.0 - rr));
  float3 v = float3(q.x, -q.y, z);

  // The eye ray carried back into the sphere's frame: roll, then pitch,
  // then heading, each undone.
  float cr = cos(-roll), sr = sin(-roll);
  v = float3(v.x * cr - v.y * sr, v.x * sr + v.y * cr, v.z);
  float cp = cos(-pitch), sp = sin(-pitch);
  v = float3(v.x, v.y * cp - v.z * sp, v.y * sp + v.z * cp);
  float cy = cos(yaw), sy = sin(yaw);
  v = float3(v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy);

  float lat = asin(clamp(v.y, -1.0, 1.0));
  float lon = atan2(v.x, v.z);
  float px = 1.0 / R;

  float hemi = smoothstep(-horizonBlend, horizonBlend, v.y);
  float3 above = lerp(sky.rgb, skyPole.rgb, clamp(lat / 1.5707963, 0.0, 1.0));
  float3 below =
      lerp(ground.rgb, groundPole.rgb, clamp(-lat / 1.5707963, 0.0, 1.0));
  float3 col = lerp(below, above, hemi);

  // A rule is the distance to a PLANE: a meridian's plane stands through
  // the poles at its own longitude, a parallel's lies at its own sine.
  // The crowding toward the limb and toward the poles is then exact.
  float fine = minorDeg * 0.017453292;
  float fineLon = floor(lon / fine + 0.5) * fine;
  float dFineMeridian = abs(v.x * cos(fineLon) - v.z * sin(fineLon));
  float fineLat = floor(lat / fine + 0.5) * fine;
  float dFineParallel = abs(v.y - sin(fineLat));
  float meridianStep = meridianDeg * 0.017453292;
  float heavyLon = floor(lon / meridianStep + 0.5) * meridianStep;
  float dMeridian = abs(v.x * cos(heavyLon) - v.z * sin(heavyLon));
  float parallelStep = parallelDeg * 0.017453292;
  float heavyLat = floor(lat / parallelStep + 0.5) * parallelStep;
  float dParallel = abs(v.y - sin(heavyLat));

  float w = lineWidth * px;
  float minor = max(1.0 - smoothstep(w, w * 2.4, dFineMeridian),
                    1.0 - smoothstep(w, w * 2.4, dFineParallel));
  float major = max(1.0 - smoothstep(w * 1.5, w * 3.4, dMeridian),
                    1.0 - smoothstep(w * 1.5, w * 3.4, dParallel));
  float horizon = 1.0 - smoothstep(w * 1.7, w * 3.6, abs(v.y));

  col = lerp(col, grid.rgb,
             clamp(minor * minorWeight + major * majorWeight, 0.0, 1.0));
  col = lerp(col, grid.rgb, horizon * horizonWeight);

  // The ball: what a point keeps at the limb plus what it gains facing
  // the eye, and one specular from the named direction.
  col *= (ambient + diffuse * z);
  float3 L = normalize(light.xyz);
  float lit = pow(max(0.0, dot(L, float3(q.x, -q.y, z))), shininess);
  col += lit * specular;

  float edge = 1.0 - smoothstep(1.0 - edgeFeather * px, 1.0, sqrt(rr));
  return float4(col * edge, edge);
}
)SHADER";

inline constexpr std::string_view kGlobeSkSL = R"SHADER(
// The paint-side entry: the globe read at the pixel, in SkSL's types.
half4 main(float2 p) {
  float4 c = globeAt(p);
  return half4(half3(c.rgb), half(c.a));
}
)SHADER";

inline sigil::material::Material globe(const GlobeParameters& parameters = {}) {
  return sigil::material::shader(
      sigil::material::skSLFromSlang(std::string(kGlobePreludeSlang)).append(kGlobeSkSL),
      parameters, {.key = "ksp_mapview.globe"});
}

}  // namespace ksp_mapview
