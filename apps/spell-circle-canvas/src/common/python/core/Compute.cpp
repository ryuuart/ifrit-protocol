#include <sigilpython/Extend.h>
#include <sigilpython/core/Registration.h>

namespace sigil::python {

void bindCoreCompute(pybind11::module_& module) {
  submodule(module, "core.noise");
  submodule(module, "core.hash");
  submodule(module, "core.intervals");
}

}  // namespace sigil::python
