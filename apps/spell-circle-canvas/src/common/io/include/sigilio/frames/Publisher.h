#pragma once

/** @file
 * @ingroup io-frames
 * The door a drawn frame leaves by: the handle `hub.publish()` answers,
 * and the seam a backend implements behind it.
 */

#include <memory>
#include <utility>
#include <string>
#include <string_view>

#include "sigilio/frames/Frame.h"

/** A TEXTURE SHARED WITH ANOTHER APPLICATION ON THIS MACHINE, in both
 *  directions: a frame this process has drawn offered under a name, and
 *  a frame another process is publishing taken as a texture of one's
 *  own. Reach for it to send a drawing into a video mixer, a projection
 *  tool or a compositor, or to bring one of theirs in. The handles are
 *  the graphics API's own and nothing here is anybody's toolkit. */
namespace sigil::io::frames {

namespace detail {
/** WHAT A BACKEND IMPLEMENTS behind a `Publisher` handle: the copy of
 *  one frame into the publication, and the name it stands under. */
class PublisherEnd {
 public:
  virtual ~PublisherEnd() = default;

  /** Appends the publication of the @p width by @p height region of
   *  @p nativeTexture to the still-open @p nativeCommandBuffer. The newest
   *  image remains available to clients that subscribe after this call.
   *  Direct3D11 uses its immediate context and ignores the command buffer. */
  virtual void publishFrame(void* nativeTexture, void* nativeCommandBuffer,
                            int width, int height) = 0;

  /** The name a subscriber finds this publication under. */
  [[nodiscard]] virtual std::string_view name() const = 0;
};

}  // namespace detail

/** A FRAME, OFFERED TO WHOEVER IS WATCHING: a copyable handle onto one
 * publication, which ends when the last handle onto it goes. A host that
 * has just drawn into a texture hands it over and every application
 * subscribed to the name sees that frame the way up it was drawn, with
 * alpha premultiplied as drawn. THE TEXTURE HANDED IN HOLDS ITS FIRST
 * ROW AT THE TOP, which is what a canvas draws; putting it the way round
 * the protocol's own surface is read is this seam's work and not the
 * caller's. THE HANDLES ARE THE GRAPHICS API'S OWN, as opaque pointers —
 * on Metal an `id<MTLTexture>` and an `id<MTLCommandBuffer>` — and the
 * texture is BORROWED FOR THE CALL, the publication owning the copied
 * image. A handle made empty publishes nothing.
 * @trap METAL WORK IS APPENDED, NOT SUBMITTED: it runs when the caller
 * commits the buffer, so the drawing must already have been submitted on
 * the same queue. Direct3D11 copies through its immediate context. */
class Publisher {
 public:
  /** A handle onto no publication. */
  Publisher() = default;
  /** A handle onto @p end, which a backend made. */
  explicit Publisher(std::shared_ptr<detail::PublisherEnd> end)
      : m_end(std::move(end)) {}

  /** Appends the publication of @p frame's region of its texture to its
   *  still-open command buffer. The newest image remains available to
   *  clients that subscribe after this call. Direct3D11 uses its
   *  immediate context and ignores the command buffer. False on a handle
   *  onto nothing or a frame with no texture or no area. */
  bool send(const Frame& frame) const {
    if (!m_end || !frame.texture || frame.width <= 0 || frame.height <= 0)
      return false;
    m_end->publishFrame(frame.texture, frame.commandBuffer, frame.width,
                        frame.height);
    return true;
  }

  /** The name a subscriber finds this publication under; empty on a
   *  handle onto nothing. */
  [[nodiscard]] std::string_view name() const {
    return m_end ? m_end->name() : std::string_view();
  }

  /** Whether this handle holds a publication. */
  explicit operator bool() const { return m_end != nullptr; }

 private:
  std::shared_ptr<detail::PublisherEnd> m_end;
};

}  // namespace sigil::io::frames
