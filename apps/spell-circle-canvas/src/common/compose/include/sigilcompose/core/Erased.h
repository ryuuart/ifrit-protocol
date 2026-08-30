#pragma once

/** @file
 * SigilCompose's comparable type erasure — SigilCore's Erased under the
 * compose name every seam value on a description is spelled in. Nothing
 * here is defined by this library.
 */

#include <sigilcore/reconcile/Erased.h>

namespace sigil::compose {

/** A VALUE that answers an interface, compared the way the reconciler
 *  asks: copies of one value are equal, comparable models of one type
 *  compare their values, the escape hatch compares equal to nothing but
 *  its own copies. */
using ::sigil::core::Erased;

}  // namespace sigil::compose
