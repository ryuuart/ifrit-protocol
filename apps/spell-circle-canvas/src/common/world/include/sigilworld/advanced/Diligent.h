#pragma once

/** @file
 * @ingroup world-diligent
 * The device executor's own controls: the compiler a recipe's Slang body
 * is built with, and the bits a program is specialised on. A host that
 * only draws a frame reaches for `world::diligent::runtime()` and never
 * this.
 */

#include <cstdint>

namespace sigil::world::diligent {

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
  /** The shadow stage's build: the body's surface is asked only where it
   *  covers, and the program writes depth and nothing else. */
  kVariantShadow = 1u << 1u,
  /** Beside `kVariantLit`, the kinds of light that reach the body. A lit
   *  program carries the terms of a light with a size only when one of
   *  them has one; it reads a directional light's cascades only when one
   *  of them is read through cascades, and a point light's six faces only
   *  when one of them is read through them. A body under none of these is
   *  drawn by a program that holds none of their code. */
  kVariantAreaLights = 1u << 2u,
  kVariantCascadedShadows = 1u << 3u,
  kVariantPointShadows = 1u << 4u,
};

}  // namespace sigil::world::diligent
