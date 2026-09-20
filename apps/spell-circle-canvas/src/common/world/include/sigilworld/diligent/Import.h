#pragma once

/** @file
 * @ingroup world-diligent
 * A texture the graphics API's own object stands behind, entering the
 * material vocabulary through the device's handle table.
 */

#include <sigilcore/hardware/GpuDevice.h>
#include <sigilmaterial/texture/Texture.h>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

namespace sigil::world::diligent {

/** @p native as a `material::Texture` on @p device — THE DOOR A FOREIGN
 * TEXTURE COMES IN BY, copying nothing in either direction.
 * @p takeOwnership hands the texture to the device, which then releases
 * it with the last copy of the value; either way the handle is let go
 * when the value is. An empty texture when the device has adopted none,
 * or when the import was refused.
 * @trap IT HAS NO HOST IMAGE, which is what says the pixels were never
 * read back: a renderer holding another device draws the body undressed
 * rather than something it made up.
 */
::sigil::material::Texture importNative(
    ::sigil::geometry::device::Device& device,
    const core::hardware::NativeTexture& native, bool takeOwnership = false);

}  // namespace sigil::world::diligent
