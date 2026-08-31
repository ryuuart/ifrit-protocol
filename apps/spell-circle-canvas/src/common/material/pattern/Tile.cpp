/** @file
 * The tile mechanism: the shared bake, its copy-on-write editors, and the
 * repeating texture over it.
 */

#include "sigilmaterial/pattern/Tile.h"

#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>

#include <algorithm>
#include <cmath>

namespace sigil::material::pattern {

Tile Tile::of(SkSize size, Program draw) {
  Tile t;
  t.m_state = std::make_shared<State>();
  t.m_state->size = size;
  t.m_state->draw = std::move(draw);
  return t;
}

// A recipe shared with another Tile is cloned before it is edited. The
// clone keeps the bake — every editor drops it immediately afterwards.
void Tile::detach() {
  if (m_state && m_state.use_count() > 1)
    m_state = std::make_shared<State>(*m_state);
}

Tile& Tile::seed(uint32_t s) {
  if (m_state && m_state->seed != s) {
    detach();
    m_state->seed = s;
    m_state->baked.reset();
  }
  return *this;
}

Tile& Tile::program(Program draw) {
  if (!m_state) m_state = std::make_shared<State>();
  detach();
  m_state->draw = std::move(draw);
  m_state->baked.reset();
  return *this;
}

Tile& Tile::invalidate() {
  if (m_state) {
    detach();
    m_state->baked.reset();
  }
  return *this;
}

uint32_t Tile::currentSeed() const { return m_state ? m_state->seed : 0; }

SkSize Tile::size() const {
  return m_state ? m_state->size : SkSize::MakeEmpty();
}

bool Tile::baked() const { return m_state && m_state->baked; }

SkMatrix Tile::mapping() const {
  SkMatrix local = SkMatrix::RotateDeg(m_rotate);
  local.preScale(m_scale, m_scale);
  local.postTranslate(m_offset.fX, m_offset.fY);
  return local;
}

sk_sp<SkImage> Tile::image() const {
  if (!m_state) return nullptr;
  State& st = *m_state;
  if (st.baked) return st.baked;
  if (!st.draw) return nullptr;
  const int w = std::max(1, (int)std::ceil(st.size.width()));
  const int h = std::max(1, (int)std::ceil(st.size.height()));
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
  if (!surface) return nullptr;
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  st.draw(*canvas, {(float)w, (float)h}, st.seed);
  st.baked = surface->makeImageSnapshot();
  return st.baked;
}

Texture Tile::texture() const {
  return Texture::of(image())
      .tile(SkTileMode::kRepeat)
      .uv(mapping())
      .filter(m_filter);
}

}  // namespace sigil::material::pattern
