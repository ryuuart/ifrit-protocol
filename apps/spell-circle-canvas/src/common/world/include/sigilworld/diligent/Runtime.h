#pragma once

/** @file
 * @ingroup world-diligent
 * The GPU executor, as a VALUE: the same Frame, the same passes and the
 * same declarations, performed on a device instead of on the CPU.
 */

#include <sigilworld/frame/Runtime.h>

#include <cstdint>
#include <string>

namespace sigil::geometry::device {
class Device;
}  // namespace sigil::geometry::device

/** THE DEVICE SIDE OF SIGILWORLD, ON DILIGENT ENGINE: the executor that
 *  records a frame's passes on a graphics device, and the import that
 *  brings a mesh across to it. Reach for it to run a frame on real
 *  hardware; everything above it is written against the executor seam
 *  and never names this catalogue. */
namespace sigil::world::diligent {

/** The executor that performs a frame's passes on @p device. The
 * frame's resources live there for as long as the executor does, and
 * nothing crosses back until something asks: the value installs itself
 * as the `Targets::ImageSource`. Every value made from one device shares
 * its queue.
 * @trap Two frames carrying the value ONE call made compare equal; two
 * separate calls do not, holding separate device state.
 */
Runtime runtime(::sigil::geometry::device::Device& device);

/** Registers the compiler that turns a recipe's `Target::Slang` body
 * into a pipeline's shaders, with the shared program cache. Idempotent,
 * and called by `runtime()`.
 * @trap A recipe with no Slang body is reported once by the cache and
 * drawn in the colour the frame extracted instead.
 */
void installSlangCompiler();

/** THE VARIANT BITS this backend specialises a program on. A recipe
 *  compiles once per variant, so a bit belongs here only when it changes
 *  the PROGRAM. Neither the mesh vertex layout nor the blended build
 *  does, so neither is here. */
enum : uint32_t {
  /** The emitters reach the surface. Without it the surface colour
   *  stands as it is, which is what a coverage or a variant re-draw
   *  wants. */
  kVariantLit = 1u << 0u,
};

}  // namespace sigil::world::diligent
