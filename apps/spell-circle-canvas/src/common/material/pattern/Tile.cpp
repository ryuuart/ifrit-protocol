/** @file
 * The tile mechanism: the shared bake, its copy-on-write editors, and the
 * repeating texture over it.
 */

#include "sigilmaterial/pattern/Tile.h"

#include <include/core/SkImage.h>
#include <sigilmaterial/skia/Texture.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <vector>

namespace sigil::material::pattern {

Tile Tile::of(glm::vec2 size, Program draw) {
  Tile t;
  t.m_state = std::make_shared<State>();
  t.m_state->size = size;
  t.m_state->draw = std::move(draw);
  return t;
}

namespace {

/** THE STATES TILES MADE FROM A RECIPE SHARE. One is kept while a tile
 *  holds it or while anything still holds its bake — a material that
 *  paints a pattern holds the image and not the tile, so a pattern
 *  re-described from the same numbers finds the bake the page is still
 *  painting with. Once neither is held, the entry goes the next time the
 *  list is searched. */
struct Shared {
  TileRecipe recipe;
  glm::vec2 size;
  std::shared_ptr<void> state;
};

std::mutex& sharedLock() {
  static std::mutex lock;
  return lock;
}

std::vector<Shared>& sharedStates() {
  static std::vector<Shared> states;
  return states;
}

/** Whether anything but @p baked itself holds the image it bakes to. */
bool bakeHeldElsewhere(const media::PixelSource& baked) {
  if (!baked) return false;
  sk_sp<SkImage> image = skia::image(Texture(baked));
  if (!image) return false;
  const SkImage* raw = image.get();
  image.reset();
  return !raw->unique();
}

}  // namespace

Tile Tile::of(glm::vec2 size, Program draw, TileRecipe recipe) {
  const std::scoped_lock hold(sharedLock());
  std::vector<Shared>& states = sharedStates();
  std::erase_if(states, [](const Shared& entry) {
    const State* state = static_cast<const State*>(entry.state.get());
    return entry.state.use_count() == 1 && !bakeHeldElsewhere(state->baked);
  });
  for (const Shared& entry : states)
    if (entry.size == size && entry.recipe == recipe) {
      Tile t;
      t.m_state = std::static_pointer_cast<State>(entry.state);
      return t;
    }
  Tile t = of(size, std::move(draw));
  t.m_state->recipe = recipe;
  states.push_back({std::move(recipe), size, t.m_state});
  return t;
}

// A recipe shared with another Tile is cloned before it is edited, and so
// is one tiles made from its recipe share, which another such tile may be
// made from later. The clone keeps the bake — every editor drops it
// immediately afterwards — and carries no recipe.
void Tile::detach() {
  if (m_state && (m_state.use_count() > 1 || m_state->recipe)) {
    m_state = std::make_shared<State>(*m_state);
    m_state->recipe.reset();
  }
}

Tile& Tile::seed(uint32_t value) {
  if (m_state && m_state->seed != value) {
    detach();
    m_state->seed = value;
    m_state->baked = {};
  }
  return *this;
}

Tile& Tile::program(Program draw) {
  if (!m_state) m_state = std::make_shared<State>();
  detach();
  m_state->draw = std::move(draw);
  m_state->baked = {};
  return *this;
}

Tile& Tile::invalidate() {
  if (m_state) {
    detach();
    m_state->baked = {};
  }
  return *this;
}

uint32_t Tile::currentSeed() const { return m_state ? m_state->seed : 0; }

glm::vec2 Tile::size() const { return m_state ? m_state->size : glm::vec2(0); }

bool Tile::baked() const {
  return m_state && static_cast<bool>(m_state->baked);
}

glm::mat3 Tile::mapping() const {
  // Rotate, then scale, then translate, applied to a texture-space point
  // in that order: translate * rotate * scale as a column-vector product.
  const float radians = m_rotate * (3.14159265358979323846f / 180.0f);
  // A quarter turn lands exactly on the axes, as Skia's own rotation
  // snaps it, so a rotated repeat of a pixel grid stays on the grid.
  const auto snap = [](float value) {
    return std::abs(value) <= 1.0f / 4096.0f ? 0.0f : value;
  };
  const float cosine = snap(std::cos(radians)), sine = snap(std::sin(radians));
  glm::mat3 local(1.0f);
  local[0][0] = cosine * m_scale;
  local[0][1] = sine * m_scale;
  local[1][0] = -sine * m_scale;
  local[1][1] = cosine * m_scale;
  local[2][0] = m_offset.x;
  local[2][1] = m_offset.y;
  return local;
}

Texture Tile::texture() const {
  if (!m_state) return {};
  State& state = *m_state;
  if (!state.baked) {
    if (!state.draw) return {};
    const glm::ivec2 pixels(std::max(1, (int)std::ceil(state.size.x)),
                            std::max(1, (int)std::ceil(state.size.y)));
    state.baked = state.draw(pixels, state.seed);
    if (!state.baked) return {};
  }
  return Texture(state.baked)
      .tile(Repeat::Repeat)
      .uv(mapping())
      .sampling(m_sampling);
}

}  // namespace sigil::material::pattern
