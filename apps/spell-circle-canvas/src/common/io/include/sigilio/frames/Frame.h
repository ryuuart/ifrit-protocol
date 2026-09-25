#pragma once

/** @file
 * @ingroup io-frames
 * The values a frame crosses the machine in: the frame itself, the
 * graphics device it lives on, and what `hub.publish()` and
 * `hub.subscribe()` take besides the URI.
 */

#include <string>

namespace sigil::io::frames {

/** The graphics API a device and a frame's handles belong to.
 *  `Default` is the one the URI's protocol is carried over: Metal for
 *  `syphon://`, Direct3D11 for `spout://`. */
enum class GraphicsApi { Default, Metal, Direct3D11 };

/** THE GPU THE FRAMES LIVE ON, as the graphics API's own handle — an
 *  `id<MTLDevice>` or an `ID3D11Device*` as an opaque pointer, which the
 *  caller owns and outlives the publisher or subscription with. A null
 *  handle is this machine's default device where the platform names one. */
struct Device {
  GraphicsApi api = GraphicsApi::Default;
  void* handle = nullptr;
};

/** ONE FRAME, AS THE GRAPHICS API'S OWN HANDLES: on Metal `texture` is an
 *  `id<MTLTexture>` and `commandBuffer` an `id<MTLCommandBuffer>`, both
 *  opaque pointers. What is sent is the `width` by `height` region of the
 *  texture; a frame that arrives carries no command buffer. */
struct Frame {
  void* texture = nullptr;
  void* commandBuffer = nullptr;
  int width = 0;
  int height = 0;
};

/** How `hub.publish(uri, {…})` publishes: the device the frames it is
 *  sent are drawn on. */
struct PublishOptions {
  Device device;
};

/** How `hub.subscribe(uri, {…})` subscribes: only @ref application's
 *  publication of the name when it is not empty, received on
 *  @ref device. */
struct SubscribeOptions {
  std::string application;
  Device device;
};

}  // namespace sigil::io::frames
