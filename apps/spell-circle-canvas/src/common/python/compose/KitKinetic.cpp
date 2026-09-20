#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeKitKinetic(pybind11::module_& module) {
  submodule(module, "compose.fx");
}

}  // namespace sigil::python
