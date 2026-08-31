#pragma once

/** @file
 * The GPU executor, as a VALUE: the same Frame, the same passes and the
 * same declarations, performed on a device instead of on the CPU.
 */

#include <sigilworld/frame/Runtime.h>

#include <cstdint>
#include <string>

namespace sigil::world::diligent {

class Device;

/**
 * The executor that performs a frame's passes on @p device.
 *
 * WHAT RUNS WHERE. A geometry pass rasterises, depth-tested, into a
 * device texture, from a pipeline built out of the material's own Slang
 * body. A post pass is a shader pass over the textures it reads. A
 * compute pass cooks its chain on the `pop::Runtime` the pass carries,
 * which is the CPU one until a kernel exists for the device — a pass
 * that named a device pop runtime gets it, and one that named none is
 * cooked on the host and uploaded like any other geometry.
 *
 * WHERE THE PIXELS ARE. The frame's resources live on the device for as
 * long as the executor does. Nothing crosses back until something asks:
 * the value installs itself as the `Targets::ImageSource`, so a readback
 * or the presented picture costs one crossing for that one resource and
 * every other resource stays where it was written.
 *
 * Two frames carrying the value made by one call to this compare equal;
 * two separate calls do not, because they hold separate device state.
 * Every value made from one device shares its queue, and every
 * submission this makes is taken under `Device::QueueLock`.
 */
Runtime runtime(Device& device);

/**
 * Registers the compiler that turns a recipe's `Target::Slang` body into
 * a pipeline's shaders, with the shared program cache. Idempotent, and
 * called by `runtime()` — a caller that wants to resolve a material for
 * `Target::Slang` before it has a device calls it itself.
 *
 * A recipe with no Slang body is reported once by the cache, naming the
 * recipe and the target, and the body it would have painted is drawn in
 * the colour the frame extracted instead.
 */
void installSlangCompiler();

/** THE VARIANT BITS this backend specialises a program on.
 *
 *  A recipe compiles once per variant, so a bit belongs here only when
 *  it changes the PROGRAM. `kVariantLit` does: without it the lighting,
 *  the uniforms it reads and the loop over the emitters are not in the
 *  compiled shader at all.
 *
 *  Two things a backend might be expected to specialise on are not here,
 *  because neither changes a program. The mesh vertex layout is one:
 *  every body reaches a pipeline as position, normal, uv and tint, with
 *  the lanes a mesh does not carry filled in on upload, so there is one
 *  layout and nothing to tell apart. The blended build is the other: it
 *  is the blend and depth state a pipeline is created with, which the
 *  pipeline cache keys on beside the program rather than compiling a
 *  second one. */
enum : uint32_t {
  /** The emitters reach the surface. Without it the surface colour
   *  stands as it is, which is what a coverage or a variant re-draw
   *  wants. */
  kVariantLit = 1u << 0u,
};

}  // namespace sigil::world::diligent
