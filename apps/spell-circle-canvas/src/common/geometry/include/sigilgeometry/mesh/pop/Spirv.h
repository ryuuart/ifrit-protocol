#pragma once

/** @file
 * A compiled kernel's SPIR-V, made to mean what its source says.
 *
 * workaround: THE GAP THIS CLOSES. Slang through 2026.7.1 puts no
 * `NoContraction` decoration in the module it emits — `-fp-mode precise`
 * included, which is a no-op for the SPIR-V target — so nothing in the
 * words tells a driver that a multiply and the add after it are two
 * operations. A driver is then
 * free to fuse them into one, which rounds once where the source rounds
 * twice — and a host build whose contraction is pinned off rounds twice.
 * One decoration per arithmetic result is what makes the two agree, and
 * without it every expression of the shape `a + b * c` disagrees in its
 * last place.
 *
 * It is done here, once, rather than beside each kernel's own words: a
 * module that means one thing in one feature and another in the next is
 * not a single source, and a second kernel would otherwise have to
 * remember to do this.
 */

#include <cstdint>
#include <span>
#include <vector>

namespace sigil::geometry::mesh {

/** @p words with one `NoContraction` decoration per arithmetic result.
 *  Returns the module unchanged when it cannot be read as one, because
 *  a module this cannot parse is one a driver should be given as it
 *  stands rather than one to guess at. */
std::vector<uint32_t> noContraction(std::span<const uint32_t> words);

}  // namespace sigil::geometry::mesh
