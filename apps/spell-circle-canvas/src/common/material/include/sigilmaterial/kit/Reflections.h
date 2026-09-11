#pragma once

/** @file
 * Reflective surfaces — gold foil, stainless chrome, glass — as recipes
 * over two textures: a normal map saying where the surface points and an
 * environment saying what it reflects. Real reflection models sampled
 * per pixel, not gradient impressions.
 *
 * Both textures encode device-space normals (+y down, +z toward the
 * viewer) as rgb = n * 0.5 + 0.5: `bevelNormals()` derives one from an
 * outline's coverage, a 3D painter's normal pass rasterizes true ones.
 * Roughness picks a pre-blurred level of the same environment. Gold adds
 * foil crinkle and glints, chrome the contrast curve and brushed
 * anisotropy, glass refracts a backdrop through the normal field with a
 * fresnel-weighted reflection on top.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/texture/EnvironmentMap.h>
#include <sigilmaterial/texture/Surface.h>
#include <sigilmaterial/texture/Texture.h>

#include <glm/vec2.hpp>
#include <memory>

namespace sigil::material::kit {

/** Dials for gold — a warm metal whose reflection is broken up by foil
 *  wrinkles. `crinkle` and `crinkleScale` set how coarse that wrinkling
 *  is; at zero the surface is polished and the environment reflects
 *  cleanly. Every field is a uniform of the body except `roughness`,
 *  which picks the environment level when the material is built, and
 *  `envSize`, which the builder fills from the environment. */
struct GoldParams {
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
 *  as for GoldParams. */
struct ChromeParams {
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
 *  thickness. `roughness` and `envSize` as for GoldParams. */
struct GlassParams {
  Color tint = {0.82f, 0.93f, 0.96f, 1};  ///< transmission colour
  float refractPx = 18;      ///< max backdrop displacement at the bevel
  float reflection = 0.55f;  ///< fresnel reflection strength
  float roughness = 0.05f;
  float edgeGlow = 0.35f;  ///< bright rim where the surface turns away
  float opacity = 1;
  glm::vec2 envSize = {1, 1};
};

/** The recipes, defined once. Each declares the child slots `normals`
 *  and `env`; glass also `backdrop`. */
const std::shared_ptr<const Recipe>& goldRecipe();
/** Chrome's, declaring `normals` and `env`. */
const std::shared_ptr<const Recipe>& chromeRecipe();
/** Glass's, which declares `backdrop` on top of those two — what lies
 *  behind the surface is part of what it shades. */
const std::shared_ptr<const Recipe>& glassRecipe();

/** A gold surface over @p normals, reflecting @p env at the params'
 *  roughness. */
Material gold(Texture normals, const EnvironmentMap& env,
              const GoldParams& params = {});
/** A chrome surface over @p normals, reflecting @p env. */
Material chrome(Texture normals, const EnvironmentMap& env,
                const ChromeParams& params = {});
/** A glass surface over @p normals, refracting @p backdrop — an image of
 *  what sits behind the shape in the same device coordinates — and
 *  reflecting @p env. */
Material glass(Texture normals, const EnvironmentMap& env, Texture backdrop,
               const GlassParams& params = {});

}  // namespace sigil::material::kit
