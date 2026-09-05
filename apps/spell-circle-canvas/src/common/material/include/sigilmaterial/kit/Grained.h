#pragma once

/** @file
 * Grained surfaces — stone, timber, latten and board — as recipes: a
 * ramp of the material's tones, a luminance grain over it and a seeded
 * speckle on top, each generated per pixel from its parameters and a
 * seed, so a paving, a lattice, an instrument or a card is drawn from
 * numbers and never from an image.
 *
 * All four share one construction. The RAMP is what the material's tones
 * do across a piece — a stone's bed, a board's lit arris and shaded one,
 * the sheen one light lays across a sheet of brass. The GRAIN is value
 * noise collapsed to one channel and folded into the colour as light
 * rather than as a hue, which is what keeps a coloured surface from
 * reading as rainbow terrazzo. The SPECKLE is a fleck in some fraction of
 * the cells of a lattice, in the material's own tones. The SEED offsets
 * every field, so two pieces at two seeds are two pieces of one quarry;
 * `material::Bank` is what holds N of them for a field of a thousand.
 *
 * Each recipe carries a body in both languages a renderer here speaks.
 * The SkSL body reads pixel coordinates; the Slang body reads the
 * surface's uv, so a per-px number there is a per-uv-unit one.
 */

#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>

#include <glm/vec2.hpp>
#include <memory>

namespace sigil::material::kit {

/** STONE: a quarry's two tones on a diagonal bed, veined with grain and
 *  flecked with a speckle in its own colours — the cut stone of a floor,
 *  the granite of a sett.
 *
 *  The bed runs hi → lo → hi over `bedLength` px along `bedAngle`, and
 *  `bedDepth` says how far into `lo` its middle goes. The bed is stated
 *  in px rather than in the box, because a tessera is cut from a slab
 *  and its bed does not scale with the piece. `grainScale` is features
 *  per px and `stretch` runs the veining lengthwise; the speckle is a
 *  fleck in `speckle` of the cells of a `speckleCell` px lattice, each
 *  either the light tone brightened or the dark one darkened, laid over
 *  at `speckleAlpha`. */
struct StoneParams {
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

/** TIMBER: a planed board, not a dowel — a flat face between a narrow lit
 *  arris and a narrow shadowed one, with grain lines running along the
 *  piece and a fine tooth over the whole face.
 *
 *  `span` is the face's width across the grain in px, `flip` lights the
 *  far edge instead of the near one, and `along` turns the piece to run
 *  down local y, so one recipe boards a lattice's rails and its posts.
 *  `grain` is grain lines per px along the piece and `figure` how hard
 *  they read; `tooth` and `toothScale` are the luminance grain, with
 *  `stretch` running it along the piece. Keep `toothScale · stretch`
 *  under about a tenth or the tooth aliases to hash. */
struct TimberParams {
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
 *  lights, so the material is a LADDER of three tones — shadow, body,
 *  light — and a piece's `level` is where on the ladder it sits: a
 *  recessed plate low, a raised rete high, a pin at the top. Across the
 *  piece the light lays a SHEEN: the ladder position drifts by `sheen`
 *  along the run from `from` to `to`, two points in the paint's own
 *  coordinates — a node's px, or the root's when the paint is anchored
 *  to it, which is how one light crosses two hundred nodes of one
 *  instrument. `tooth` is the tooling's grain; `patina` is the fraction
 *  of `patinaCell` px cells carrying a fleck of `patinaColor`, whose
 *  alpha is its strength. */
struct LattenParams {
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
struct BoardParams {
  Color paint = {0.91f, 0.89f, 0.84f, 1};
  float tooth = 0.08f;
  float toothScale = 0.045f;
  float stretch = 1.0f;
  float wear = 0.05f;
  float wearScale = 0.006f;
  float seed = 0.0f;
};

/** The recipes, defined once. None declares a child slot: every field is
 *  computed from the params and the seed. */
const std::shared_ptr<const Recipe>& stoneRecipe();
const std::shared_ptr<const Recipe>& timberRecipe();
const std::shared_ptr<const Recipe>& lattenRecipe();
const std::shared_ptr<const Recipe>& boardRecipe();

/** A stone at @p params. */
Material stone(const StoneParams& params = {});
/** A board of timber at @p params. */
Material timber(const TimberParams& params = {});
/** A sheet of latten at @p params. */
Material latten(const LattenParams& params = {});
/** A painted board at @p params. */
Material board(const BoardParams& params = {});

}  // namespace sigil::material::kit
