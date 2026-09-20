#include <sigilpython/Extend.h>
#include <sigilpython/scry/Registration.h>

namespace sigil::python {

void bindScry(pybind11::module_& module) {
  submodule(module, "scry");
}

}  // namespace sigil::python
