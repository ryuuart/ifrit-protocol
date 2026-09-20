#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeMasks(pybind11::module_& module) {
  submodule(module, "compose.parts");
  submodule(module, "compose.by");
}

}  // namespace sigil::python
