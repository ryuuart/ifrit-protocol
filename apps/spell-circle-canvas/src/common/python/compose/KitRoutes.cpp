#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeKitRoutes(pybind11::module_& module) {
  submodule(module, "compose.routers");
  submodule(module, "compose.instancing.place");
}

}  // namespace sigil::python
