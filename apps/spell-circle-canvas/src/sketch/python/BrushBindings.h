#pragma once

#include <pybind11/pybind11.h>

namespace sigil::sketch::python {

/** Registers the native natural-media tools, geometry and brush engine under
 *  the drawing module. Shared drawing values and the pen must be registered. */
void bindBrush(pybind11::module_& module);

}  // namespace sigil::sketch::python
