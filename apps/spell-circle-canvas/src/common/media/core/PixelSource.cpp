/** @file
 * The one source this library writes itself beyond a document and a
 * picture: a picture baked once by a producer and kept.
 */

#include "sigilmedia/core/PixelSource.h"

#include <mutex>

namespace sigil::media {

struct Produced::State {
  explicit State(std::function<Picture()> function)
      : produce(std::move(function)) {}
  std::function<Picture()> produce;
  std::once_flag once;
  Picture baked;
};

Produced::Produced(std::string key, std::function<Picture()> producer)
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

PixelSource::PixelSource(Picture picture)
    : m_impl(std::make_shared<Model<Still>>(Still{std::move(picture)})) {}

PixelSource PixelSource::produce(std::string key,
                                 std::function<Picture()> producer) {
  return PixelSource(Produced(std::move(key), std::move(producer)));
}

}  // namespace sigil::media
