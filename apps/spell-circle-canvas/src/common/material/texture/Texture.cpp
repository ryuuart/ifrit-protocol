/** @file
 * The producer bake, the region cut, and the image shader a texture
 * samples through.
 */

#include "sigilmaterial/texture/Texture.h"

#include <mutex>

namespace sigil::material {

struct ProducerSource::State {
  explicit State(std::function<sk_sp<SkImage>()> function)
      : produce(std::move(function)) {}
  std::function<sk_sp<SkImage>()> produce;
  std::once_flag once;
  sk_sp<SkImage> baked;
};

ProducerSource::ProducerSource(std::string key,
                               std::function<sk_sp<SkImage>()> produce)
    : m_key(std::move(key)),
      m_state(std::make_shared<State>(std::move(produce))) {}

sk_sp<SkImage> ProducerSource::image() const {
  std::call_once(m_state->once, [&] {
    if (m_state->produce) m_state->baked = m_state->produce();
  });
  return m_state->baked;
}

sk_sp<SkImage> Texture::image() const {
  sk_sp<SkImage> full = m_source.image();
  if (!full || !m_region) return full;
  if (m_cut && m_cutFrom.get() == full.get()) return m_cut;
  SkIRect rect = *m_region;
  if (!rect.intersect(SkIRect::MakeWH(full->width(), full->height())))
    return nullptr;
  m_cutFrom = full;
  m_cut = full->makeSubset(nullptr, rect, {});
  return m_cut;
}

SkISize Texture::size() const {
  sk_sp<SkImage> img = image();
  return img ? img->dimensions() : SkISize::MakeEmpty();
}

sk_sp<SkShader> Texture::shader() const {
  sk_sp<SkImage> img = image();
  if (!img) return nullptr;
  return img->makeShader(m_tileX, m_tileY, SkSamplingOptions(m_filter), m_uv);
}

bool Texture::operator==(const Texture& other) const {
  return m_source == other.m_source && m_tileX == other.m_tileX &&
         m_tileY == other.m_tileY && m_uv == other.m_uv &&
         m_region == other.m_region && m_filter == other.m_filter;
}

}  // namespace sigil::material
