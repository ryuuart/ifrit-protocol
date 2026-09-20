#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeLines(pybind11::module_& module) {
  submodule(module, "compose.lines");
}

}  // namespace sigil::python
