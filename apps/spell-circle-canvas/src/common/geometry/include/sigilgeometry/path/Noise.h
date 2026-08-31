#pragma once
/** @file
 * Seeded, deterministic noise, under the name every geometry tool
 * spells it by.
 *
 * The integer mixers and the unit floats squeezed out of them are
 * SigilCoreCompute's, because a shader's CPU twin, a text cache and a
 * resource store reach for the same bodies; `noise::` here is those
 * functions plus the one field built on them, so a caller inside this
 * library — or a library above it — writes `noise::hash` and gets the
 * one definition.
 *
 * `hash` and `pcgHash` are different mixers with different outputs, kept
 * side by side because each seeds work that is compared byte-for-byte
 * against stored renders. Pick by what the caller already uses; new
 * code takes `pcgHash`.
 */
#include <sigilcore/compute/Noise.h>

#include <cstdint>
#include <glm/vec3.hpp>

namespace sigil::geometry::path::noise {

using core::noise::hash;
using core::noise::pcgAdvance;
using core::noise::pcgHash;
using core::noise::pcgMix;
using core::noise::pcgNext;
using core::noise::pcgUnit;
using core::noise::pcgUnitNext;

/** Trilinear value noise over the integer lattice, in [-1, 1], seeded.
 *  A field rather than a mixer: it is read at a POSITION, and points
 *  near each other read near values, which is what a displacement wants
 *  and a per-index jitter does not. */
float value3(glm::vec3 p, uint32_t seed);

}  // namespace sigil::geometry::path::noise
