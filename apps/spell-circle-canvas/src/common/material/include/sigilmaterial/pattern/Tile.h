#pragma once

/** @file
 * @ingroup material-pattern
 *
 * Tile — a repeating texture baked once from a program: a recipe for one
 * tile plus a mapping. The bake is memoised on shared state and
 * regeneration is explicit; scale, rotation and offset act on the
 * sampling matrix only, so a rotated repeat stays seamless and costs no
 * rebake. A tile made from a RECIPE — every stock generator's is — is
 * the same tile as any other held from an equal recipe at its size, so a
 * pattern rebuilt from the same numbers finds the bake already made. A
 * tile made from a bare program cannot be compared, so its bake is its
 * identity: one re-minted each frame carries no bake and re-renders.
 */

#include <sigilmaterial/texture/Texture.h>
#include <sigilmedia/core/PixelSource.h>

#include <cstdint>
#include <functional>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/** Procedural patterns: one tile baked once from a program and repeated
 *  under a mapping of scale, rotation and offset, the stock tile
 *  generators over it, and the woven cloth a sett and a weave make.
 *  Reach for a pattern when the surface repeats, so the generator runs
 *  once for a tile instead of once per pixel; reach for `field` when it
 *  does not. Regeneration is always explicit — a tile with the same seed
 *  is the same tile. */
namespace sigil::material::pattern {

/** BAKES ONE TILE of @p size whole pixels into the picture the tile
 *  repeats; `seed` is the tile's current seed — same seed, same tile,
 *  which is what makes regeneration a choice. An empty source bakes
 *  nothing. The Skia executor's `painted()` makes one from a painter
 *  that draws into a canvas. */
using Program =
    std::function<media::PixelSource(glm::ivec2 size, uint32_t seed)>;

/** WHAT A TILE IS MADE FROM, AS A VALUE: the generator that draws it and
 *  every number it was handed, in the generator's own order. Equal
 *  recipes at one size draw the same tile, which is what lets a tile
 *  rebuilt from them share the bake of one already held. */
struct TileRecipe {
  std::string generator;
  std::vector<double> numbers;
  bool operator==(const TileRecipe&) const = default;
};

/** A repeating fill built from one tile. A value: the mapping is
 *  per-object, the recipe and its bake are shared, and the editors of the
 *  shared part copy-on-write so re-rolling a copy never re-rolls the
 *  original. */
class Tile {
 public:
  Tile() = default;

  /** A tile of @p size pixels baked by @p draw, at whole pixels rounded
   *  up. */
  static Tile of(glm::vec2 size, Program draw);
  /** A tile of @p size baked by @p draw, which is what @p recipe
   *  describes: while any tile made from an equal recipe at this size is
   *  held, or anything still paints with its bake, this one is that tile —
   *  the same recipe, the same bake, and equal to it. Reseeding or reprogramming either copies it first, so
   *  neither ever changes the other. */
  static Tile of(glm::vec2 size, Program draw, TileRecipe recipe);

  /** Change the seed, drop the bake: the next texture() regenerates. */
  Tile& seed(uint32_t value);
  /** Replace the program and drop the bake. */
  Tile& program(Program draw);
  /** Drop the bake alone; the next texture() re-runs the program. */
  Tile& invalidate();
  /** Mapping only, no rebake. */
  Tile& scale(float factor) {
    m_scale = factor;
    return *this;
  }
  Tile& rotate(float degrees) {
    m_rotate = degrees;
    return *this;
  }
  /** Pan the repeat, in the sampled space's pixels. */
  Tile& offset(glm::vec2 pixels) {
    m_offset = pixels;
    return *this;
  }
  /** How the baked tile is read between pixels: linear (the default) is
   *  right for organic tiles and wrong for anything on a pixel grid. */
  Tile& sampling(Sampling mode) {
    m_sampling = mode;
    return *this;
  }

  bool valid() const { return m_state != nullptr; }
  uint32_t currentSeed() const;
  glm::vec2 size() const;
  /** Whether the bake exists now. */
  bool baked() const;
  float scale() const { return m_scale; }
  float rotate() const { return m_rotate; }
  glm::vec2 offset() const { return m_offset; }
  Sampling sampling() const { return m_sampling; }
  /** The sampling matrix the mapping composes to: rotate, scale, then
   *  translate. */
  glm::mat3 mapping() const;

  /** The tile as a texture: the bake — run on first use and kept until
   *  the seed or the program changes — repeating on both axes through the
   *  mapping, read at the tile's sampling. Empty when the tile is empty or
   *  the bake yields nothing. */
  Texture texture() const;

  /** Same shared recipe (the same bake) and the same mapping. */
  bool operator==(const Tile& other) const {
    return m_state == other.m_state && m_scale == other.m_scale &&
           m_rotate == other.m_rotate && m_offset == other.m_offset &&
           m_sampling == other.m_sampling;
  }

 private:
  struct State {
    glm::vec2 size{32, 32};
    Program draw;
    uint32_t seed = 1;
    media::PixelSource baked;
    /** The recipe this state was made from, while it is the one tiles
     *  made from that recipe share; an edited copy carries none. */
    std::optional<TileRecipe> recipe;
  };
  void detach();

  std::shared_ptr<State> m_state;
  float m_scale = 1.0f;
  float m_rotate = 0.0f;
  glm::vec2 m_offset{0, 0};
  Sampling m_sampling = Sampling::Linear;
};

}  // namespace sigil::material::pattern
