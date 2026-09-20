#pragma once

/** @file
 * Binding the retained side of the scene description: the one composer
 * class Python has, over a composer Python owns outright or one a host
 * is lending.
 */

#include <functional>
#include <memory>

class SkCanvas;

namespace sigil::compose {
class Composer;
}

namespace sigil::python {

class TickerHandle;

/** A composer of Python's own, or checked access to the one a host is
 *  lending. The queries, the dials and the describe path are the same
 *  native calls in both cases; what differs is who sizes the composer,
 *  who sets its clock and who draws it.
 *
 *  A composer is not thread-safe, so an owned handle refuses a thread
 *  other than the one that made it, and a borrowed one refuses through
 *  the access it was given, which also reports a session that has
 *  closed.
 *
 *  An owned handle holds everything its composer borrows: the ticker it
 *  was made over and the font context it measures and shapes with. It
 *  also refuses every call made from inside its own draw, because a
 *  paint program that describes or draws the composer painting it
 *  would re-enter a tree that is halfway through a frame. */
class ComposerHandle {
 public:
  /** A composer of this handle's own over a ticker nothing steps, so a
   *  transition it is asked for never runs: what a still picture
   *  needs. */
  ComposerHandle();
  /** A composer of this handle's own whose transitions @p ticker
   *  drives. The handle keeps a ticker Python owns alive; a ticker a
   *  host lends stays the host's, and the composer refuses once the
   *  session that lent it has closed. */
  explicit ComposerHandle(const TickerHandle& ticker);
  /** A handle onto a host's composer: @p access reaches it and throws
   *  when the session is gone or another thread asks. */
  explicit ComposerHandle(std::function<compose::Composer&()> access);

  /** The composer itself: the one this handle owns, or the host's
   *  through the access it was given. Throws on a thread other than the
   *  one that made an owned handle, from inside an owned composer's own
   *  draw, and on a session that has closed. */
  compose::Composer& get() const;
  /** Whether this handle owns its composer, and so may size it, set
   *  its clock and draw it. */
  bool owned() const { return m_owner != nullptr; }
  /** The composer on the same terms as `get`, for a call only the
   *  owner of a composer makes. Throws @p refusal on a borrowed
   *  handle. */
  compose::Composer& ownedComposer(const char* refusal) const;
  /** Lays out and paints onto @p canvas, refusing every other call on
   *  this handle until the paint returns. Throws on a borrowed
   *  handle, whose host draws it. */
  void draw(SkCanvas& canvas) const;

 private:
  struct Owned;
  std::shared_ptr<Owned> m_owner;
  std::function<compose::Composer&()> m_access;
};

}  // namespace sigil::python
