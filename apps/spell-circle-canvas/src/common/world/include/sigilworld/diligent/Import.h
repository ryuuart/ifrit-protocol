#pragma once

/** @file
 * @ingroup world-diligent
 * A texture the graphics API's own object stands behind, entering the
 * material vocabulary through the device's handle table.
 */

#include <sigilgpu/GpuDevice.h>
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
 * A texture made with a chain is worn through it, its levels below zero
 * filled from level zero once a frame by whoever wears it, so a body
 * wearing it at a distance reads a level its size calls for; a texture
 * with one level is worn at that level.
 * @trap IT HAS NO HOST IMAGE, which is what says the pixels were never
 * read back: a renderer holding another device draws the body undressed
 * rather than something it made up.
 * @trap A chain the texture's maker filled itself is filled again from
 * level zero by its wearer.
 */
::sigil::material::Texture importNative(
    ::sigil::geometry::device::Device& device, const gpu::NativeTexture& native,
    bool takeOwnership = false);

}  // namespace sigil::world::diligent
