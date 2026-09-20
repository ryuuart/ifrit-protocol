#pragma once

/** @file
 * @ingroup material-kit
 *
 * Grained surfaces — stone, timber, latten and board — as recipes, each
 * one RAMP of the material's tones, a luminance GRAIN over it and a
 * seeded SPECKLE on top, generated per pixel and never from an image.
 * The SEED offsets every field, so two pieces at two seeds are two
 * pieces of one quarry. The SkSL body reads pixel coordinates; the
 * Slang body reads the surface's uv, so a per-px number there is a
 * per-uv-unit one.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec2.hpp>
#include <memory>

namespace sigil::material::kit {

/** STONE: a quarry's two tones on a diagonal bed, veined with grain and
 *  flecked with a speckle in its own colours. The bed runs hi → lo → hi
 *  over `bedLength` px along `bedAngle`, stated in PX rather than in the
 *  box, because a tessera is cut from a slab and its bed does not scale
 *  with the piece. `grainScale` is features per px. */
struct StoneParameters {
  Color hi = {0.87f, 0.84f, 0.77f, 1};
  Color lo = {0.73f, 0.69f, 0.63f, 1};
  float bedAngle = 24.0f;  ///< degrees
  float bedLength = 52.0f;
  float bedDepth = 1.0f;
  float grainScale = 0.055f;
  float grainContrast = 0.35f;
  float stretch = 1.0f;
  float speckle = 0.35f;
  float speckleCell = 8.0f;
  float speckleAlpha = 0.27f;
  float seed = 0.0f;
};

/** TIMBER: a planed board, not a dowel — a flat face between a narrow
 *  lit arris and a narrow shadowed one, with grain lines along the piece
 *  and a fine tooth over the face. `span` is the face's width across the
 *  grain in px and `grain` is lines per px along it; `along` turns the
 *  piece to run down local y, so one recipe boards a lattice's rails and
 *  its posts.
 *  @trap Keep `toothScale · stretch` under about a tenth, or the tooth
 *  aliases to hash. */
struct TimberParameters {
  Color base = {0.84f, 0.74f, 0.54f, 1};
  Color light = {0.96f, 0.90f, 0.77f, 1};
  Color dark = {0.56f, 0.42f, 0.23f, 1};
  float span = 24.0f;
  float flip = 0.0f;
  float along = 0.0f;
  float grain = 0.19f;
  float figure = 0.26f;
  float tooth = 0.15f;
  float toothScale = 0.05f;
  float stretch = 2.0f;
  float seed = 0.0f;
};

/** LATTEN: sheet brass under one light. Brass has ONE colour and many
 *  lights, so the material is a LADDER of three tones and a piece's
 *  `level` is where on it the piece sits. The light lays a SHEEN across
 *  the run from `from` to `to`, two points in the paint's own
 *  coordinates — a node's px, or the root's when the paint is anchored
 *  there, which is how one light crosses two hundred nodes of one
 *  instrument. */
struct LattenParameters {
  Color shadow = {0.36f, 0.27f, 0.18f, 1};
  Color body = {0.63f, 0.53f, 0.26f, 1};
  Color light = {1.0f, 0.86f, 0.55f, 1};
  glm::vec2 from = {0.0f, 0.0f};
  glm::vec2 to = {1.0f, 1.0f};
  float level = 0.5f;
  float sheen = 0.1f;
  float tooth = 0.08f;
  float toothScale = 0.9f;
  float patina = 0.0f;
  float patinaCell = 26.0f;
  Color patinaColor = {0.18f, 0.35f, 0.27f, 0.09f};
  float seed = 0.0f;
};

/** BOARD: a painted or manila surface — one colour under a fine tooth and
 *  a slow wear, the card a chart is mounted on, the plate a panel is
 *  painted. `tooth` and `toothScale` are the fine grain, `stretch` runs
 *  it one way; `wear` and `wearScale` are the slow blotch that makes one
 *  board differ from the next. */
struct BoardParameters {
  Color paint = {0.91f, 0.89f, 0.84f, 1};
  float tooth = 0.08f;
  float toothScale = 0.045f;
  float stretch = 1.0f;
  float wear = 0.05f;
  float wearScale = 0.006f;
  float seed = 0.0f;
};

/** The recipes, defined once. None declares a slot: every field is
 *  computed from the parameters and the seed. */
const std::shared_ptr<const Recipe>& stoneRecipe();
/** The timber recipe. */
const std::shared_ptr<const Recipe>& timberRecipe();
/** The latten recipe. */
const std::shared_ptr<const Recipe>& lattenRecipe();
/** The board recipe. */
const std::shared_ptr<const Recipe>& boardRecipe();

/** A stone at @p parameters. */
Material stone(const StoneParameters& parameters = {});
/** A board of timber at @p parameters. */
Material timber(const TimberParameters& parameters = {});
/** A sheet of latten at @p parameters. */
Material latten(const LattenParameters& parameters = {});
/** A painted board at @p parameters. */
Material board(const BoardParameters& parameters = {});

/** THE LATTEN LADDER READ ON THE CPU: the colour a sheet of @p parameters
 *  shows at @p along, the position on the run from `from` to `to` where
 *  0 is `from` and 1 is `to`. It is the recipe's own reading — the
 *  level drifted by the sheen, then the three tones — so a caller that
 *  cannot take a material still stands on one ladder with the faces
 *  around it.
 *
 *  Every consumer of a `Fill` is such a caller: a stroke's paint and a
 *  ribbon's fill take a colour and a gradient, so a sheet crossing one
 *  of those is the run sampled at the two ends and the middle. */
[[nodiscard]] Color lattenTone(const LattenParameters& parameters, float along);

}  // namespace sigil::material::kit
