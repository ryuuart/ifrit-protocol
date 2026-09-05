#pragma once

/** @file
 * A compiled kernel's SPIR-V, made to mean what its source says.
 *
 * workaround: THE GAP THIS CLOSES. At the Slang this tree pins,
 * 2026.7.1, the SPIR-V emitter writes no `NoContraction` decoration at
 * all: `-fp-mode precise` yields a module byte-identical to the one
 * `default` and `fast` yield, and a `precise` qualifier on a local
 * changes nothing either. Nothing in the words then tells a driver that
 * a multiply and the add after it are two operations, and a driver is
 * free to fuse them into one, which rounds once where the source rounds
 * twice — and a host build whose contraction is pinned off rounds twice.
 * One decoration per arithmetic result is what makes the two agree, and
 * without it every expression of the shape `a + b * c` disagrees in its
 * last place. This pass is what the emitter would do under
 * `-fp-mode precise`, so a Slang that decorates its own arithmetic
 * retires both the pass and the call to it.
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
