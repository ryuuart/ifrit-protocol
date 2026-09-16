#include <pybind11/pybind11.h>
#include <sigilpython/Python.h>

#include "KitBindings.h"

namespace sigil::sketch::python {
void bindRuntime(pybind11::module_& module);
}

// One registration body serves the executable's built-in module and the
// extension imported by an ordinary Python process.
PYBIND11_MODULE(_sigil, module) {
  sigil::python::bindLibraries(module);
  sigil::sketch::python::bindRuntime(module);
  sigil::sketch::python::bindSketchKit(module);
}
