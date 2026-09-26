/** @file
 * The one source this library writes itself beyond a document and a
 * picture: a picture baked once by a producer and kept.
 */

#include "sigilmedia/core/PixelSource.h"

#include <mutex>

namespace sigil::media {

struct Produced::State {
  explicit State(std::function<sk_sp<SkImage>()> function)
      : produce(std::move(function)) {}
  std::function<sk_sp<SkImage>()> produce;
  std::once_flag once;
  sk_sp<SkImage> baked;
};

Produced::Produced(std::string key, std::function<sk_sp<SkImage>()> producer)
    : m_key(std::move(key)),
      m_state(std::make_shared<State>(std::move(producer))) {}

Frame Produced::frameAt(std::chrono::duration<double>) const {
  std::call_once(m_state->once, [&] {
    if (m_state->produce) m_state->baked = m_state->produce();
  });
  Frame frame;
  frame.image = m_state->baked;
  return frame;
}

PixelSource::PixelSource(sk_sp<SkImage> picture)
    : m_impl(std::make_shared<Model<Still>>(Still{std::move(picture)})) {}

PixelSource PixelSource::produce(std::string key,
                                 std::function<sk_sp<SkImage>()> producer) {
  return PixelSource(Produced(std::move(key), std::move(producer)));
}

}  // namespace sigil::media
