#pragma once

/** @file
 * The door a drawn frame leaves by: the seam a host offers its texture
 * over, and the one factory that answers with whatever this build can
 * publish through.
 */

#include <memory>
#include <string>
#include <string_view>

namespace sigil::io::publish {

/** The graphics API whose native handles a publisher consumes. */
enum class Backend { Metal, Direct3D11 };

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
 * THE TEXTURE IS BORROWED FOR THE CALL. The submitted command buffer
 * keeps its GPU resources until completion; the publication owns the
 * copied image. A host whose texture is reallocated hands the new one
 * over on the next frame without preserving an old render target.
 *
 * WHAT A SUBSCRIBER RECEIVES is the frame as it was drawn: its rows in
 * the order the texture holds them, the first of them the top of the
 * picture, and its alpha premultiplied the way the canvas wrote it.
 * Nothing here turns the picture over or divides the alpha out. A
 * publication carries a pixel format of its own, so the channels are put
 * in that order on the way across; nothing else about a pixel changes.
 *
 * METAL WORK IS APPENDED, NOT SUBMITTED. The publication rides the buffer
 * the caller is still filling and runs when the caller commits it, so
 * the drawing must already have been submitted on the same queue: work
 * on one queue runs in the order it was committed, which is what lets
 * the copy see a finished frame rather than a half-drawn one. Direct3D11
 * instead copies through its immediate context after the host's draw.
 */
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

/**
 * The publisher this build has for @p backend and @p nativeDevice, under
 * @p name: Syphon on Metal, Spout on Direct3D11 when that SDK is present,
 * and null for unsupported platform/backend combinations.
 *
 * @p nativeDevice is an `id<MTLDevice>` for Metal or `ID3D11Device*` for
 * Direct3D11. Its textures are `id<MTLTexture>` or `ID3D11Texture2D*`
 * respectively. The caller owns the device and outlives the publisher.
 *
 * NULL IS AN ORDINARY ANSWER: this build publishes over no protocol, or
 * there is no device to publish from, or the name is empty and nothing
 * could be found under it. A caller treats a null publisher as a run
 * that does not publish and says so, never as a reason to publish
 * another way.
 */
std::unique_ptr<Publisher> createPublisher(std::string name, Backend backend,
                                           void* nativeDevice);

}  // namespace sigil::io::publish
