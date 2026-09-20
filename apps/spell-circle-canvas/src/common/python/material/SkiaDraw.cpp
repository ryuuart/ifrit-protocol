#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

namespace sigil::python {

void bindMaterialSkiaDraw(pybind11::module_& module) {
  submodule(module, "material.stock");
}

}  // namespace sigil::python
