#pragma once

/** @file
 * Tile — a repeating texture baked once from a program: a recipe for one
 * tile plus a mapping (scale, rotation, offset). The tile bakes ONCE into
 * an image memoised on shared state; regeneration is explicit — `seed(n)`
 * drops the bake and the next `image()` re-renders. Scale, rotation and
 * offset act on the sampling matrix only, so a rotated repeat stays
 * seamless and costs no rebake.
 *
 * THE BAKE IS THE IDENTITY, and that decides where a Tile is stored. Hold
 * one where assets are held; re-minting a Tile each frame mints fresh
 * shared state with no bake in it, so every frame re-renders the tile.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkMatrix.h>
#include <include/core/SkPoint.h>
#include <include/core/SkSize.h>
#include <sigilmaterial/texture/Texture.h>

#include <cstdint>
#include <functional>
#include <memory>

namespace sigil::material::pattern {

/** Draws ONE tile into [0,0 .. size); `seed` is the tile's current seed —
 *  same seed, same tile, which is what makes regeneration a choice. */
using Program = std::function<void(SkCanvas&, SkSize, uint32_t seed)>;

/** A repeating fill built from one tile. A value: the mapping is
 *  per-object, the recipe and its bake are shared, and the editors of the
 *  shared part copy-on-write so re-rolling a copy never re-rolls the
 *  original. */
class Tile {
 public:
  Tile() = default;

  /** A tile of @p size drawn by @p draw. */
  static Tile of(SkSize size, Program draw);

  /** Change the seed, drop the bake: the next image() regenerates. */
  Tile& seed(uint32_t s);
  /** Replace the program and drop the bake. */
  Tile& program(Program draw);
  /** Drop the bake alone; the next image() re-runs the program. */
  Tile& invalidate();
  /** Mapping only, no rebake. */
  Tile& scale(float s) {
    m_scale = s;
    return *this;
  }
  Tile& rotate(float degrees) {
    m_rotate = degrees;
    return *this;
  }
  /** Pan the repeat, in the sampled space's pixels. */
  Tile& offset(SkPoint px) {
    m_offset = px;
    return *this;
  }
  /** How the baked tile samples: linear (the default) is right for
   *  organic tiles and wrong for anything on a pixel grid. */
  Tile& filter(SkFilterMode mode) {
    m_filter = mode;
    return *this;
  }

  bool valid() const { return m_state != nullptr; }
  uint32_t currentSeed() const;
  SkSize size() const;
  /** Whether the bake exists now. */
  bool baked() const;
  float scale() const { return m_scale; }
  float rotate() const { return m_rotate; }
  SkPoint offset() const { return m_offset; }
  SkFilterMode filter() const { return m_filter; }
  /** The sampling matrix the mapping composes to: rotate, scale, then
   *  translate. */
  SkMatrix mapping() const;

  /** The baked tile, rendered on first use and kept until the seed or the
   *  program changes. Null when the tile is empty or the bake fails. */
  sk_sp<SkImage> image() const;
  /** The tile as a texture: the bake repeating on both axes through the
   *  mapping, at the filter. */
  Texture texture() const;

  /** Same shared recipe (the same bake) and the same mapping. */
  bool operator==(const Tile& other) const {
    return m_state == other.m_state && m_scale == other.m_scale &&
           m_rotate == other.m_rotate && m_offset == other.m_offset &&
           m_filter == other.m_filter;
  }

 private:
  struct State {
    SkSize size = {32, 32};
    Program draw;
    uint32_t seed = 1;
    sk_sp<SkImage> baked;
  };
  void detach();

  std::shared_ptr<State> m_state;
  float m_scale = 1.0f;
  float m_rotate = 0.0f;
  SkPoint m_offset = {0, 0};
  SkFilterMode m_filter = SkFilterMode::kLinear;
};

}  // namespace sigil::material::pattern
