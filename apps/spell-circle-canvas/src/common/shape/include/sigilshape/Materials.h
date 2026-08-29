#pragma once

/** @file
 * SigilShape literal materials — gold foil, stainless chrome, glass as
 * SkSL runtime effects: real reflection models sampled per pixel, not
 * gradient impressions (the peer to SigilCompose's LayerStyles ramps).
 *
 * The pipeline is a two-channel deferred pass in miniature:
 *
 *   normals  — where the surface points. bevelNormals() derives a
 *              rounded-bevel normal map from any SkPath's coverage
 *              (blur -> smoothstep shoulder -> Sobel), and
 *              space::drawMesh(Mode::Normals) rasterizes true 3D
 *              normals; both encode DEVICE-space normals (+y down) as
 *              rgb = n * 0.5 + 0.5.
 *   env      — what the surface reflects. An Environment is an
 *              equirect image sampled by reflection direction;
 *              procedural studio()/sunset() bakes need no assets, and
 *              fromEquirect() wraps a loaded HDRI. Roughness picks a
 *              pre-blurred mip of the same environment.
 *
 * Each material is a shader over those two children: gold adds foil
 * crinkle + glints, chrome adds the contrast curve and brushed
 * anisotropy, glass refracts a backdrop child through the normal field
 * with a fresnel-weighted reflection on top. The draw* helpers run the
 * whole pipeline for one path in one call.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPath.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkShader.h>

#include <map>

namespace sigil::shape::materials {

/** An equirect environment (u = azimuth, v = 0 at zenith) with cached
 *  roughness blurs. Copyable handle; blur cache shared. */
class Environment {
 public:
  Environment() = default;

  /** Neutral photo-studio bake: graded sky, floor bounce, three
   *  softboxes — the default for steel and gold. */
  static Environment studio(int width = 512);
  /** The y2k chrome horizon: banded sky over dark ground with a hot
   *  sun stripe — sunset chrome text lives here. */
  static Environment sunset(int width = 512);
  /** Wrap a loaded equirect panorama (LDR or F16/F32 HDR SkImage). */
  static Environment fromEquirect(sk_sp<SkImage> image);

  bool valid() const { return m_base != nullptr; }
  /** The environment at @p roughness in [0,1]: 0 = mirror-sharp base,
   *  higher = progressively blurred copies (bucketed, cached). */
  sk_sp<SkImage> image(float roughness = 0) const;

 private:
  sk_sp<SkImage> m_base;
  std::shared_ptr<std::map<int, sk_sp<SkImage>>> m_blurs;
};

/** Derive a rounded-bevel normal map from a path's coverage. The map
 *  covers @p bounds (device px); pixels outside the shape encode flat
 *  (0,0,1). @p bevelPx is the shoulder width; @p heightScale steepens
 *  the bevel (1 = bevel as deep as wide). */
sk_sp<SkImage> bevelNormals(const SkPath& path, SkIRect bounds, float bevelPx,
                            float heightScale = 1);

/** Dials for the `gold()` shader — a warm metal whose reflection is
 *  broken up by foil wrinkles. `crinkle` and `crinkleScale` set how
 *  coarse that wrinkling is; at zero the surface is polished and the
 *  environment reflects cleanly. */
struct GoldParams {
  SkColor4f tint = {1.0f, 0.78f, 0.34f, 1};  ///< gold F0
  float roughness = 0.25f;
  float crinkle = 0.35f;       ///< foil wrinkle strength (0 = polished)
  float crinkleScale = 0.05f;  ///< wrinkle frequency (cycles per px)
  float sparkle = 0.5f;        ///< glint pops on wrinkle highlights
  float ambient = 0.18f;       ///< floor so shadow sides stay golden
};

/** Dials for the `chrome()` shader — a cool mirror that lives or dies
 *  by how hard the environment is pushed. `contrast` and `exposure`
 *  shape the reflection, `brushed` streaks it anisotropically, and
 *  `fresnel` sets how much brighter the glancing edges read. */
struct ChromeParams {
  SkColor4f tint = {0.92f, 0.95f, 1.0f, 1};  ///< cool steel bias
  float roughness = 0.0f;
  float contrast = 1.6f;  ///< env contrast curve (chrome pops at ~1.6)
  float brushed = 0.0f;   ///< horizontal anisotropic streak, 0..1
  float fresnel = 0.6f;   ///< edge-vs-face reflectivity spread
  /** Env gain before the contrast curve. Procedural bakes are already
   *  display-bright (leave at 1); real HDRIs of dim studios want 2-3. */
  float exposure = 1.0f;
};

/** Dials for the `glass()` shader — a transmissive surface that
 *  displaces the backdrop behind it rather than reflecting an
 *  environment. `refractPx` is how far the bevel bends what is
 *  behind, which is what sells the thickness. */
struct GlassParams {
  SkColor4f tint = {0.82f, 0.93f, 0.96f, 1};  ///< transmission color
  float refractPx = 18;   ///< max backdrop displacement at the bevel
  float reflect = 0.55f;  ///< fresnel reflection strength
  float roughness = 0.05f;
  float edgeGlow = 0.35f;  ///< bright rim where the surface turns away
  float opacity = 1;
};

/** Material shaders over a normal map + environment. @p origin is the
 *  device position of the normal map's (0,0) so shader xy and normals
 *  align. */
sk_sp<SkShader> gold(sk_sp<SkImage> normals, const Environment& env,
                     SkPoint origin, const GoldParams& params = {});
sk_sp<SkShader> chrome(sk_sp<SkImage> normals, const Environment& env,
                       SkPoint origin, const ChromeParams& params = {});
/** Glass additionally samples @p backdrop — an image of what sits
 *  behind the shape, in the same device coordinates. */
sk_sp<SkShader> glass(sk_sp<SkImage> normals, const Environment& env,
                      sk_sp<SkImage> backdrop, SkPoint origin,
                      const GlassParams& params = {});

/** One-call pipeline: bevelNormals over the path's bounds, material
 *  shader, clip to path, fill. */
void drawGold(SkCanvas& canvas, const SkPath& path, const Environment& env,
              float bevelPx = 6, const GoldParams& params = {});
void drawChrome(SkCanvas& canvas, const SkPath& path, const Environment& env,
                float bevelPx = 6, const ChromeParams& params = {});
void drawGlass(SkCanvas& canvas, const SkPath& path, const Environment& env,
               sk_sp<SkImage> backdrop, float bevelPx = 10,
               const GlassParams& params = {});

}  // namespace sigil::shape::materials
