#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposePixelStyles(pybind11::module_& module) {
  submodule(module, "compose.styles");
}

}  // namespace sigil::python
