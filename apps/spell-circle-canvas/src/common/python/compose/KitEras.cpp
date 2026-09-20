#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeKitEras(pybind11::module_& module) {
  submodule(module, "compose.kit.bevels");
}

}  // namespace sigil::python
