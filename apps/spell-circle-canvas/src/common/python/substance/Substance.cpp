#include <sigilpython/Extend.h>
#include <sigilpython/substance/Registration.h>

namespace sigil::python {

void bindSubstance(pybind11::module_& module) {
  submodule(module, "substance");
}

}  // namespace sigil::python
