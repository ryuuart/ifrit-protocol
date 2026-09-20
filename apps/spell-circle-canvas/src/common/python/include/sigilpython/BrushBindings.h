#pragma once

/** @file
 * Binding the brush engine: the natural-media tools and the
 * geometry they lay down, under the drawing module.
 */

#include <pybind11/pybind11.h>

namespace sigil::python {

/** Registers the native natural-media tools, geometry and brush engine under
 *  the drawing module. Shared drawing values and the pen must be registered. */
void bindBrush(pybind11::module_& module);

}  // namespace sigil::python
