#pragma once

// Reflective surfaces — gold foil, stainless chrome, glass — as programs
// over two textures: a normal map saying where the surface points and an
// environment saying what it reflects. Both encode device-space normals
// (+y down, +z toward the viewer) as rgb = n * 0.5 + 0.5, and roughness
// picks a pre-blurred level of the same environment. The environment
// face and normal-bridge studies of this lighting family include this
// header from here.

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/core/Terms.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Texture.h>

#include <algorithm>
#include <glm/vec2.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace shapeworks_lab {

using sigil::material::Color;

/** Dials for gold — a warm metal whose reflection is broken up by foil
 *  wrinkles. `crinkle` and `crinkleScale` set how coarse that wrinkling
 *  is; at zero the surface is polished and the environment reflects
 *  cleanly. Every field is a uniform of the body except `roughness`,
 *  which picks the environment level when the material is built, and
 *  `envSize`, which the builder fills from the environment. */
struct GoldParameters {
  Color tint = {1.0f, 0.78f, 0.34f, 1};  ///< gold F0
  float roughness = 0.25f;
  float crinkle = 0.35f;       ///< foil wrinkle strength (0 = polished)
  float crinkleScale = 0.05f;  ///< wrinkle frequency (cycles per px)
  float sparkle = 0.5f;        ///< glint pops on wrinkle highlights
  float ambient = 0.18f;       ///< floor so shadow sides stay golden
  glm::vec2 envSize = {1, 1};  ///< the environment's pixel size
};

/** Dials for chrome — a cool mirror that lives or dies by how hard the
 *  environment is pushed. `contrast` and `exposure` shape the
 *  reflection, `brushed` streaks it anisotropically, and `fresnel` sets
 *  how much brighter the glancing edges read. `roughness` and `envSize`
 *  as for GoldParameters. */
struct ChromeParameters {
  Color tint = {0.92f, 0.95f, 1.0f, 1};  ///< cool steel bias
  float roughness = 0.0f;
  float contrast = 1.6f;  ///< env contrast curve (chrome pops at ~1.6)
  float brushed = 0.0f;   ///< horizontal anisotropic streak, 0..1
  float fresnel = 0.6f;   ///< edge-vs-face reflectivity spread
  /** Env gain before the contrast curve. Procedural bakes are already
   *  display-bright (leave at 1); real HDRIs of dim studios want 2-3. */
  float exposure = 1.0f;
  glm::vec2 envSize = {1, 1};
};

/** Dials for glass — a transmissive surface that displaces the backdrop
 *  behind it rather than reflecting an environment. `refractPx` is how
 *  far the bevel bends what is behind, which is what sells the
 *  thickness. `roughness` and `envSize` as for GoldParameters. */
struct GlassParameters {
  Color tint = {0.82f, 0.93f, 0.96f, 1};  ///< transmission colour
  float refractPx = 18;      ///< max backdrop displacement at the bevel
  float reflection = 0.55f;  ///< fresnel reflection strength
  float roughness = 0.05f;
  float edgeGlow = 0.35f;  ///< bright rim where the surface turns away
  float opacity = 1;
  glm::vec2 envSize = {1, 1};
};

inline constexpr std::string_view kReflectionNoiseSlang = R"SHADER(
// THE VALUE NOISE EVERY GRAINED AND MARBLED BODY IN THE KIT READS,
// written once and crossed into SkSL by the core: a hash on a lattice,
// its smoothed interpolation, three octaves of it, the luminance fold a
// grain is applied with, and the lattice fleck a speckle is scattered
// with. One text, so a stone's grain and a chrome's marbling are the
// same noise on a device and on a raster surface.
//
// The hash is the sine-fract one, which is not a good hash and is the
// right one here: it is the same arithmetic on every device that can run
// this file, and a surface's grain has to be the same grain in a stored
// render and on screen.
float hash(float2 p) {
  return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453);
}

float valueNoise(float2 p) {
  float2 i = floor(p);
  float2 f = frac(p);
  float2 u = f * f * (3.0 - 2.0 * f);
  float a = hash(i);
  float b = hash(i + float2(1.0, 0.0));
  float c = hash(i + float2(0.0, 1.0));
  float d = hash(i + float2(1.0, 1.0));
  return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float fbm(float2 p) {
  return valueNoise(p) * 0.55 + valueNoise(p * 2.13) * 0.3 +
         valueNoise(p * 4.41) * 0.15;
}

float3 grained(float3 c, float g, float k) {
  return c * (1.0 + (g - 0.5) * 2.0 * k);
}

float2 fleck(float2 q, float cell, float density, float s) {
  float2 c = floor(q / cell);
  float h = hash(c + s);
  if (h >= density) return float2(0.0, 0.0);
  float r = cell * (0.12 + 0.18 * hash(c + s + 7.0));
  float2 jitter = float2(hash(c + s + 17.0), hash(c + s + 31.0)) - 0.5;
  float2 centre = (c + 0.5) * cell + jitter * (cell - 2.0 * r);
  float d = length(q - centre);
  float cover = 1.0 - smoothstep(r - 0.75, r + 0.75, d);
  return float2(cover, step(0.5, hash(c + s + 3.0)));
}
)SHADER";
inline constexpr std::string_view kReflectivePreludeSkSL = R"SHADER(
float3 readNormal(float2 xy) {
  half4 t = normals.eval(xy);
  float3 n = float3(t.rgb) * 2.0 - 1.0;
  float len = length(n);
  return len < 0.001 ? float3(0.0, 0.0, 1.0) : n / len;
}

// The equirectangular convention every consumer of an environment map shares:
// u = 0.5 + atan2(d.x, -d.z) / 2pi, v = acos(d.y) / pi.
float2 envUv(float3 dir) {
  float u = atan(dir.x, -dir.z) / 6.2831853 + 0.5;
  float v = acos(clamp(dir.y, -1.0, 1.0)) / 3.1415927;
  return float2(u, v);
}

float3 envAt(float2 uv) {
  return float3(env.eval(uv * envSize).rgb);
}

float3 envSample(float3 dir) {
  return envAt(envUv(dir));
}

)SHADER";
inline constexpr std::string_view kReflectiveGoldSkSL = R"SHADER(
half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // Foil crinkle: fbm gradient folded into the normal field. The scale
  // is cycles per pixel; forty of them is one noise cell.
  float2 np = xy * max(crinkleScale, 1e-4) * 40.0;
  float e = 0.45;
  float gx = fbm(np + float2(e, 0.0)) - fbm(np - float2(e, 0.0));
  float gy = fbm(np + float2(0.0, e)) - fbm(np - float2(0.0, e));
  n = normalize(float3(n.x + gx * crinkle * 2.2,
                       n.y + gy * crinkle * 2.2, n.z));

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 e3 = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);
  float3 f0 = tint.rgb;
  // Metals tint their reflection; fresnel whitens the grazing rim.
  float3 spec = e3 * mix(f0, float3(1.0), fres);
  float3 col = f0 * ambient + spec;

  // Glints: sparse hash cells, gated to lit wrinkles.
  float cell = hash(floor(xy * 0.7));
  float lum = dot(e3, float3(0.299, 0.587, 0.114));
  float glint = step(0.992, cell) * sparkle * smoothstep(0.35, 1.2, lum);
  col += float3(glint);
  return half4(half3(col), 1.0);
}
)SHADER";
inline constexpr std::string_view kReflectiveChromeSkSL = R"SHADER(
half4 main(float2 xy) {
  float3 n = readNormal(xy);
  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float2 uv = envUv(R);

  // Brushed steel: smear the lookup along the azimuth.
  float3 e3 = float3(0.0);
  if (brushed > 0.001) {
    float total = 0.0;
    for (int k = -3; k <= 3; ++k) {
      float w = 1.0 - abs(float(k)) / 4.0;
      float du = float(k) * brushed * 0.02;
      // Jitter breaks the 7-tap banding into grain — the brushed tell.
      du += (hash(xy + float(k)) - 0.5) * brushed * 0.012;
      e3 += envAt(float2(fract(uv.x + du), uv.y)) * w;
      total += w;
    }
    e3 /= total;
  } else {
    e3 = envAt(uv);
  }

  // The chrome move: crush the environment's midtones.
  e3 = e3 * exposure;
  e3 = (e3 - 0.5) * contrast + 0.5;
  e3 = max(e3, float3(0.0));

  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float face = mix(1.0 - fresnel, 1.0, pow(1.0 - ndv, 3.0));
  float3 col = e3 * tint.rgb * (0.55 + 0.45 * face);
  col += float3(0.9) * pow(1.0 - ndv, 6.0); // hot silhouette rim
  return half4(half3(col), 1.0);
}
)SHADER";
inline constexpr std::string_view kReflectiveGlassSkSL = R"SHADER(
half4 main(float2 xy) {
  float3 n = readNormal(xy);
  // The bevel bends the view ray; the flat interior passes straight
  // through, so refraction reads at the rim — the lens look.
  float2 offset = n.xy * refractPx;
  float3 bg = float3(backdrop.eval(xy + offset).rgb);

  float3 V = float3(0.0, 0.0, 1.0);
  float3 R = reflect(-V, n);
  float3 e3 = envSample(R);
  float ndv = clamp(dot(n, V), 0.0, 1.0);
  float fres = pow(1.0 - ndv, 5.0);

  float3 trans = bg * tint.rgb;
  float mixv = clamp(reflection * (0.06 + 0.94 * fres), 0.0, 1.0);
  float3 col = mix(trans, e3, mixv);
  col += edgeGlow * pow(1.0 - ndv, 2.5) * float3(1.0);
  return half4(half3(col), 1.0) * opacity;
}
)SHADER";

/** The prelude every reflective body starts with: the value noise crossed
 *  from Slang, then the normal decode and the equirectangular lookup. */
inline std::string reflectiveBody(std::string_view model) {
  static const std::string noise =
      sigil::material::skSLFromSlang(kReflectionNoiseSlang);
  return std::string(noise).append(kReflectivePreludeSkSL).append(model);
}

inline glm::vec2 environmentSize(const sigil::material::EnvironmentMap& env) {
  const glm::ivec2 s = env.size();
  return {(float)std::max(s.x, 1), (float)std::max(s.y, 1)};
}

inline const std::shared_ptr<const sigil::material::Recipe>& goldRecipe() {
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GoldParameters>("gold").slot("normals").slot("env").body(
          Target::SkSL, reflectiveBody(kReflectiveGoldSkSL)));
  return recipe;
}

inline const std::shared_ptr<const sigil::material::Recipe>& chromeRecipe() {
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<ChromeParameters>("chrome").slot("normals").slot("env").body(
          Target::SkSL, reflectiveBody(kReflectiveChromeSkSL)));
  return recipe;
}

inline const std::shared_ptr<const sigil::material::Recipe>& glassRecipe() {
  using sigil::material::Recipe;
  using sigil::material::Target;
  static const auto recipe = std::make_shared<const Recipe>(
      Recipe::of<GlassParameters>("glass")
          .slot("normals")
          .slot("env")
          .slot("backdrop")
          .body(Target::SkSL, reflectiveBody(kReflectiveGlassSkSL)));
  return recipe;
}

inline sigil::material::Material gold(
    sigil::material::Texture normals,
    const sigil::material::EnvironmentMap& environment,
    const GoldParameters& parameters = {}) {
  GoldParameters p = parameters;
  p.envSize = environmentSize(environment);
  sigil::material::Material m(goldRecipe(), p);
  m.slot("normals", std::move(normals));
  m.slot("env", environment.texture(parameters.roughness));
  return m;
}

inline sigil::material::Material chrome(
    sigil::material::Texture normals,
    const sigil::material::EnvironmentMap& environment,
    const ChromeParameters& parameters = {}) {
  ChromeParameters p = parameters;
  p.envSize = environmentSize(environment);
  sigil::material::Material m(chromeRecipe(), p);
  m.slot("normals", std::move(normals));
  m.slot("env", environment.texture(parameters.roughness));
  return m;
}

inline sigil::material::Material glass(
    sigil::material::Texture normals,
    const sigil::material::EnvironmentMap& environment,
    sigil::material::Texture backdrop, const GlassParameters& parameters = {}) {
  GlassParameters p = parameters;
  p.envSize = environmentSize(environment);
  sigil::material::Material m(glassRecipe(), p);
  m.slot("normals", std::move(normals));
  m.slot("env", environment.texture(parameters.roughness));
  m.slot("backdrop", std::move(backdrop));
  return m;
}

}  // namespace shapeworks_lab
