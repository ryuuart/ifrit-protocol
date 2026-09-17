#pragma once

#include <pybind11/pybind11.h>

namespace sigil::python {
void bindWorld(pybind11::module_& module);
}  // namespace sigil::python
