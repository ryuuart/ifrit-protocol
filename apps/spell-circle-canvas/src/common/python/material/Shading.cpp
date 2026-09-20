#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

namespace sigil::python {

void bindMaterialShading(pybind11::module_& module) {
  submodule(module, "material.sdf");
  submodule(module, "material.ocio");
  submodule(module, "material.slang");
}

}  // namespace sigil::python
