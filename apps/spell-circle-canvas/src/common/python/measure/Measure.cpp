#include <sigilpython/Extend.h>
#include <sigilpython/measure/Registration.h>

namespace sigil::python {

void bindMeasure(pybind11::module_& module) {
  submodule(module, "measure");
}

}  // namespace sigil::python
