/** @file
 * The font context Python holds, owned or lent.
 */

#include <sigilpython/weave/FontContext.h>
#include <sigilweave/ports/SystemFontManager.h>

#include <stdexcept>
#include <utility>

namespace sigil::python {

FontContextHandle::FontContextHandle(weave::Face defaultFace)
    : m_owner(std::make_shared<weave::FontContext>(
          weave::ports::systemFontManager(), std::move(defaultFace))),
      m_thread(std::this_thread::get_id()) {}

FontContextHandle::FontContextHandle(
    std::function<weave::FontContext&()> access)
    : m_access(std::move(access)), m_thread(std::this_thread::get_id()) {}

weave::FontContext& FontContextHandle::get() const {
  if (m_thread != std::this_thread::get_id())
    throw std::runtime_error("FontContext belongs to its creating thread.");
  return m_owner ? *m_owner : m_access();
}

}  // namespace sigil::python
