#pragma once

/** @file
 * @ingroup io-publish
 * The door a drawn frame leaves by: the seam a host offers its texture
 * over, and the one factory that answers with whatever this build can
 * publish through.
 */

#include <memory>
#include <string>
#include <string_view>

/** A TEXTURE SHARED WITH ANOTHER APPLICATION ON THIS MACHINE, in both
 *  directions: a frame this process has drawn offered under a name, and
 *  a frame another process is publishing taken as a texture of one's
 *  own. Reach for it to send a drawing into a video mixer, a projection
 *  tool or a compositor, or to bring one of theirs in. The handles are
 *  the graphics API's own and nothing here is anybody's toolkit. */
namespace sigil::io::publish {

/** The graphics API whose native handles a publisher consumes. */
enum class Backend { Metal, Direct3D11 };

/** A FRAME, OFFERED TO WHOEVER IS WATCHING: a host that has just drawn
 * into a texture hands it over and every application subscribed to the
 * name sees that frame the way up it was drawn, with alpha premultiplied
 * as drawn. THE TEXTURE HANDED IN HOLDS ITS FIRST ROW AT THE TOP, which
 * is what a canvas draws; putting it the way round the protocol's own
 * surface is read is this seam's work and not the caller's.
 * THE HANDLES ARE THE GRAPHICS API'S OWN, as opaque pointers — on Metal
 * an `id<MTLTexture>` and an `id<MTLCommandBuffer>` — and the texture is
 * BORROWED FOR THE CALL, the publication owning the copied image.
 * @trap METAL WORK IS APPENDED, NOT SUBMITTED: it runs when the caller
 * commits the buffer, so the drawing must already have been submitted on
 * the same queue. Direct3D11 copies through its immediate context. */
class Publisher {
 public:
  virtual ~Publisher() = default;

  /** Appends the publication of the @p width by @p height region of
   *  @p nativeTexture to the still-open @p nativeCommandBuffer. The newest
   *  image remains available to clients that subscribe after this call.
   *  Direct3D11 uses its immediate context and ignores the command buffer. */
  virtual void publishFrame(void* nativeTexture, void* nativeCommandBuffer,
                            int width, int height) = 0;

  /** The name a subscriber finds this publication under. */
  [[nodiscard]] virtual std::string_view name() const = 0;
};

/** The publisher this build has for @p backend and @p nativeDevice under
 * @p name: Syphon on Metal, Spout on Direct3D11 when that SDK is present.
 * @p nativeDevice is an `id<MTLDevice>` or an `ID3D11Device*`; the caller
 * owns it and outlives the publisher.
 * @trap NULL IS AN ORDINARY ANSWER — no protocol, no device, an empty
 * name — and means a run that does not publish, never another way to. */
std::unique_ptr<Publisher> createPublisher(std::string name, Backend backend,
                                           void* nativeDevice);

}  // namespace sigil::io::publish
