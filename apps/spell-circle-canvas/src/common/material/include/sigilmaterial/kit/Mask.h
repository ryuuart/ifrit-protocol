#pragma once

/** @file
 * Masks — WHERE on a surface something applies, as a material whose
 * output is read as a scalar. A mask is what `over()` reads to decide how
 * much of a stacked material shows at a point, and it is an ordinary
 * material: it compares, animates and resolves like any other.
 *
 * Two shapes cover every source. A CONSTANT mask is a number. A SAMPLED
 * mask reads its `source` slot and turns what it finds into a scalar one
 * of three ways: a channel of it (a painted map, a mesh's colour lane), a
 * direction dotted with an axis after the map's tangent-normal decode
 * (SLOPE — moss on the faces that point up), or the value dotted with an
 * axis with no decode (HEIGHT — a tide line, dust on the top shelf).
 *
 * Both shapes then FIT the raw value: `low` and `high` remap onto 0..1
 * and clamp, and `inverted` flips the result. A slope or height mask is
 * meaningless without a fit, which is why the factories take the range.
 */

#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/core/Recipe.h>
#include <sigilmaterial/texture/Texture.h>

#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string_view>

namespace sigil::material::kit {

/** The child slot a sampled mask reads. */
inline constexpr std::string_view kMaskSourceSlot = "source";

/** How a sampled mask turns its source into a scalar. */
enum class MaskReading : uint8_t {
  Channel,  ///< one channel of the source, chosen by `channel`
  Slope,    ///< a tangent normal (rgb * 2 - 1) dotted with `axis`
  Height,   ///< the source's rgb dotted with `axis`, undecoded
};

/** The uniforms a mask reads. `value` is the constant mask's whole
 *  answer; the rest shape a sampled one. */
struct MaskParams {
  float value = 1;
  float channel = 0;  ///< 0 red .. 3 alpha
  float reading = 0;  ///< the MaskReading, as its enumerator
  glm::vec4 axis = {0, 1, 0, 0};
  float low = 0, high = 1;  ///< the raw range that maps onto 0..1
  float inverted = 0;
};

/** The recipes, defined once. The sampled one declares the `source`
 *  slot; the constant one has no children. */
const std::shared_ptr<const Recipe>& constantMaskRecipe();
/** The sampled one, whose `source` slot holds the material the coverage
 *  is read out of. */
const std::shared_ptr<const Recipe>& sampledMaskRecipe();

/** A mask that is the same everywhere. */
Material maskConstant(float value);
/** One channel of @p map. The map's own sampling — its tiling, its uv
 *  placement, its region — decides where each texel lands. */
Material maskMap(Texture map, int channel = 0);
/** One channel of a painted vertex-colour lane, which the renderer
 *  supplies as @p colors. */
Material maskVertexColor(Texture colors, int channel = 0);
/** 1 where the normal in @p normals points along @p up, falling off as
 *  it turns away: raw = dot(N, up), fitted onto [low, high]. */
Material maskSlope(Texture normals, glm::vec3 up, float low = 0.5f,
                   float high = 0.9f);
/** raw = dot(position, axis) over the positions in @p positions, fitted
 *  onto [low, high]. */
Material maskHeight(Texture positions, float low, float high,
                    glm::vec3 axis = {0, 1, 0});

/** @p mask with the raw range that maps onto 0..1 replaced. */
Material fit(Material mask, float low, float high);
/** @p mask with its answer flipped. */
Material invert(Material mask);
/** Both reshape A MASK and nothing else, and hand back a material they
 *  did not reshape with a report saying so rather than quietly. A
 *  material that is not a mask declares no range to replace and no
 *  answer to flip, so there is no reading of either call that could
 *  change it — and a stack whose coverage silently stayed as it was
 *  looks exactly like a stack whose fit was wrong. */

}  // namespace sigil::material::kit
