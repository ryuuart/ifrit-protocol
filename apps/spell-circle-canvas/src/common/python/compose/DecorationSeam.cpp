#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeDecorationSeam(pybind11::module_& module) {
  submodule(module, "compose.decorations");
}

}  // namespace sigil::python
