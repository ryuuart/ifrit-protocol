#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeSheets(pybind11::module_& module) {
  submodule(module, "compose.tiles");
}

}  // namespace sigil::python
