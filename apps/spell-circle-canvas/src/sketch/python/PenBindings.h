#pragma once

#include "Bindings.h"

namespace sigil::sketch::python {
void bindPen(pybind11::module_& module);
void bindConstants(pybind11::module_& module);
}  // namespace sigil::sketch::python
