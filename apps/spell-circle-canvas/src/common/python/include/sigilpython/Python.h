#pragma once

#include <pybind11/pybind11.h>

namespace sigil::python {
/** Registers the supported native library types into one extension module.
 *  Call once per interpreter. The caller owns interpreter initialization and
 *  module assembly; this library imports no sketch runtime or host. */
void bindLibraries(pybind11::module_& module);
}  // namespace sigil::python
