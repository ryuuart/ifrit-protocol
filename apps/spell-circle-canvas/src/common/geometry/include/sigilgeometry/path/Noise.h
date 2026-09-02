#pragma once
/** @file
 * Seeded, deterministic noise read at a POSITION.
 *
 * The integer mixers and the unit floats squeezed out of them —
 * `core::noise::hash`, the `pcg` family — are SigilCoreCompute's, because
 * a shader's CPU twin, a text cache and a resource store reach for the
 * same bodies; include `<sigilcore/compute/Noise.h>` and spell them
 * there. What is this library's is the one field built on them.
 */
#include <sigilcore/compute/Noise.h>

#include <cstdint>
#include <glm/vec3.hpp>

namespace sigil::geometry::path {

/** Trilinear value noise over the integer lattice, in [-1, 1], seeded.
 *  A field rather than a mixer: it is read at a POSITION, and points
 *  near each other read near values, which is what a displacement wants
 *  and a per-index jitter does not. */
float valueNoise(glm::vec3 p, uint32_t seed);

}  // namespace sigil::geometry::path
