#pragma once

/** @file
 * The Metal engine as the cook sees it, named without Metal's own types:
 * the engine library opened once for the process with the device and
 * queue it cooks on, and a result the engine left on that device turned
 * into a frame that stays there. Only `Metal.mm` speaks Objective-C.
 */

#include <sigilmaterial/substance/Substance.h>
#include <sigilmedia/advanced/Device.h>
#include <sigilmedia/core/Frame.h>
#include <substance/framework/inputimage.h>

#include <cstdint>
#include <memory>

namespace sigil::material::sbsar::detail {

/** THE METAL ENGINE, OPENED: the engine library a renderer switches to,
 *  and the device and command queue every Metal cook in the process
 *  shares, as the opaque values the engine's device callback is filled
 *  from. */
struct MetalEngine {
  void* library = nullptr;
  void* device = nullptr;
  void* queue = nullptr;
};

/** The engine opened on first ask and kept for the process; null where
 *  its library does not load or the machine has no Metal device. */
const MetalEngine* metalEngine();

/** Whether @p texture is a result the Metal engine left on its device. */
bool isMetalResult(const SubstanceAir::TextureAgnostic& texture);

/** A Metal result COPIED ON THE DEVICE into a texture of its own and
 *  carried as a frame standing there, so the engine's result can be
 *  released at once. The frame's binding wraps it for the recorder that
 *  draws it and reads it back only for a caller with none, its colours
 *  meaning what @p encoding says. An empty frame when the copy fails. */
media::DeviceFrame deviceFrameOf(const SubstanceAir::TextureAgnostic& texture,
                                 Encoding encoding);

/** AN IMAGE INPUT FOR THE METAL ENGINE, which reads its inputs from
 *  textures on its device: the input, and the texture it names, held for
 *  as long as the input is. */
struct MetalInput {
  SubstanceAir::InputImage::SPtr image;
  std::shared_ptr<void> texture;
};

/** @p frame as an input the Metal engine reads: a frame already standing
 *  on the device as a texture — another cook's output — is named where it
 *  stands, and a picture in host memory is uploaded once. An empty input
 *  when neither can be made. */
MetalInput metalInputOf(const media::Frame& frame);

/** How many times any Metal result has been read back into host memory
 *  in this process — what a test holds at zero across a cook drawn
 *  through a recorder. */
uint64_t metalReadbacks();

}  // namespace sigil::material::sbsar::detail
