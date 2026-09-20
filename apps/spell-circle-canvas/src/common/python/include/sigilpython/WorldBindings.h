#pragma once

/** @file
 * Binding 3D scenes: the element tree with depth, and the frames
 * drawn from it.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {
/** Registers 3D scenes on @p module. */
void bindWorld(pybind11::module_& module);
}  // namespace sigil::python
