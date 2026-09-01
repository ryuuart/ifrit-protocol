#pragma once

/** @file
 * A texture the graphics API's own object stands behind, entering the
 * material vocabulary through the device's handle table.
 */

#include <sigilmaterial/texture/Texture.h>
#include <sigilskia/device/GpuDevice.h>

namespace sigil::world::diligent {

class Device;

/**
 * @p native as a `material::Texture` on @p device.
 *
 * THE DOOR A FOREIGN TEXTURE COMES IN BY. Something else on this machine
 * — a video decoder, another engine, a capture — holds a texture as the
 * API's own object. `GpuDevice::importNative` gives the device a handle
 * over it, and the value handed back answers a `DeviceImage` naming that
 * device, so a renderer standing on it binds those pixels where they
 * stand. Nothing is copied in either direction.
 *
 * IT HAS NO HOST IMAGE. `image()` is null, which is not an omission: it
 * is what says the pixels were never read back, so a picture carrying
 * their colour cannot have come from a copy. A renderer holding another
 * device — or none — finds the `DeviceImage` names a device that is not
 * its own, asks for `image()`, gets nothing, and draws the body
 * undressed rather than something it made up.
 *
 * @p takeOwnership hands the texture to the device, which releases it
 * when the last copy of the value handed back goes; without it the
 * caller keeps the texture alive and the device only forgets its handle.
 * Either way the handle is let go when the value is, so a texture
 * imported for one frame does not accumulate a handle per frame.
 *
 * An empty texture — with no source at all — when the device has no
 * adopted `GpuDevice`, or when the import was refused because the
 * texture is missing or belongs to another API.
 */
::sigil::material::Texture importNative(Device& device,
                                        const skia::NativeTexture& native,
                                        bool takeOwnership = false);

}  // namespace sigil::world::diligent
