#include <pybind11/pybind11.h>

#include "Bindings.h"
#include "BrushBindings.h"
#include "ValueBindings.h"

namespace sigil::sketch::python {
void bindRuntime(pybind11::module_& module);
}

// One registration body serves the executable's built-in module and the
// extension imported by an ordinary Python process.
PYBIND11_MODULE(_sigil, module) {
  sigil::sketch::python::bindValues(module);
  sigil::sketch::python::bindDrawing(module);
  sigil::sketch::python::bindBrush(module);
  sigil::sketch::python::bindGeometry(module);
  sigil::sketch::python::bindRuntime(module);
}
