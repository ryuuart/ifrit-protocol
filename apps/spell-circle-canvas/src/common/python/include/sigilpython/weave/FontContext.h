#pragma once

/** @file
 * The font context Python holds: one of its own, or a session's, lent.
 */

#include <sigilweave/fonts/FontContext.h>
#include <sigilweave/style/Face.h>

#include <functional>
#include <memory>
#include <thread>

namespace sigil::python {

/** A FONT CONTEXT AS PYTHON'S `weave.FontContext`: one this handle owns,
 *  made over the system's font manager, or a session's, reached through
 *  an access that throws once the session has closed. Either belongs to
 *  the thread that made the handle, as a font context belongs to one
 *  thread. */
class FontContextHandle {
 public:
  /** A context of this handle's own; @p defaultFace serves a style with
   *  none, and no face is the manager's default family. */
  explicit FontContextHandle(weave::Face defaultFace = {});
  /** A handle onto a host's context, which @p access reaches. */
  explicit FontContextHandle(std::function<weave::FontContext&()> access);
  /** The context itself. Throws on another thread than the handle's and
   *  on a session that has closed. */
  weave::FontContext& get() const;

 private:
  std::shared_ptr<weave::FontContext> m_owner;
  std::function<weave::FontContext&()> m_access;
  std::thread::id m_thread;
};

}  // namespace sigil::python
