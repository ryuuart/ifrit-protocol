#pragma once

#include <pybind11/pybind11.h>

namespace sigil::python {
void bindWeave(pybind11::module_& module);
}  // namespace sigil::python
