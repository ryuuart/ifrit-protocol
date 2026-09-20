#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeInstancing(pybind11::module_& module) {
  submodule(module, "compose.instancing");
}

}  // namespace sigil::python
