#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeKitOrnament(pybind11::module_& module) {
  submodule(module, "compose.kit.ornament");
  submodule(module, "compose.kit.flourish");
}

}  // namespace sigil::python
