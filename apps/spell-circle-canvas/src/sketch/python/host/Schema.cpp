#include <sigilpython/Extend.h>

#include "Registration.h"

namespace sigil::sketch::python {

void bindSketchSchema(pybind11::module_& module) {
  ::sigil::python::submodule(module, "sketch.scry");
}

}  // namespace sigil::sketch::python
