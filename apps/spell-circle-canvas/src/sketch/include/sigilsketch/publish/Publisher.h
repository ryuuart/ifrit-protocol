#pragma once

/** @file
 * @ingroup sketch-publish
 *
 * The door a drawn frame leaves by: the seam a host offers its texture
 * over, and the one factory that answers with whatever this build can
 * publish through.
 */

#include <memory>
#include <string>
#include <string_view>

namespace sigil::sketch {

/**
 * A FRAME, OFFERED TO WHOEVER IS WATCHING. A host that has just drawn
 * into a texture hands it over on a command buffer it has not committed
 * yet, and every application subscribed to the name sees that frame.
 *
 * THE HANDLES ARE THE GRAPHICS API'S OWN, as opaque pointers: on Metal
 * the texture is an `id<MTLTexture>` and the buffer an
 * `id<MTLCommandBuffer>`. Nothing here is Qt's and nothing is Skia's, so
 * a host that owns its device publishes through the same seam as one
 * drawing inside a window someone else owns.
 *
 * THE TEXTURE IS BORROWED FOR THE CALL. No reference to it outlives the
 * call, so a host whose texture is reallocated — by a resize, by a new
 * device, by anything — hands the new one over on the next frame and
 * owes nothing to the last.
 *
 * WHAT A SUBSCRIBER RECEIVES is the frame as it was drawn: its rows in
 * the order the texture holds them, the first of them the top of the
 * picture, and its alpha premultiplied the way the canvas wrote it.
 * Nothing here turns the picture over or divides the alpha out. A
 * publication carries a pixel format of its own, so the channels are put
 * in that order on the way across; nothing else about a pixel changes.
 *
 * THE WORK IS APPENDED, NOT SUBMITTED. The publication rides the buffer
 * the caller is still filling and runs when the caller commits it, so
 * the drawing must already have been submitted on the same queue: work
 * on one queue runs in the order it was committed, which is what lets
 * the copy see a finished frame rather than a half-drawn one.
 */
class Publisher {
 public:
  virtual ~Publisher() = default;

  /** Appends the publication of the @p width by @p height region of
   *  @p nativeTexture to the still-open @p nativeCommandBuffer. Costs
   *  nothing but the check while nobody is watching. */
  virtual void publishFrame(void* nativeTexture, void* nativeCommandBuffer,
                            int width, int height) = 0;

  /** The name a subscriber finds this publication under. */
  [[nodiscard]] virtual std::string_view name() const = 0;
};

/**
 * The publisher this build has for @p mtlDevice, under @p name — a
 * Syphon server on macOS, and nothing anywhere else.
 *
 * @p mtlDevice is an `id<MTLDevice>` as an opaque pointer, the device
 * whose textures will be handed over; the caller stays its owner.
 *
 * NULL IS AN ORDINARY ANSWER: this build publishes over no protocol, or
 * there is no device to publish from, or the name is empty and nothing
 * could be found under it. A caller treats a null publisher as a run
 * that does not publish and says so, never as a reason to publish
 * another way.
 */
std::unique_ptr<Publisher> createPublisher(std::string name, void* mtlDevice);

}  // namespace sigil::sketch
