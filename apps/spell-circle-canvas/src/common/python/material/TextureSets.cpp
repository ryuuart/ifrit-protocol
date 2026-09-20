#include <sigilpython/Extend.h>
#include <sigilpython/material/Registration.h>

namespace sigil::python {

void bindMaterialTextureSets(pybind11::module_& module) {
  submodule(module, "material.texture");
}

}  // namespace sigil::python
