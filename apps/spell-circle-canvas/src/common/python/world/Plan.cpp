#include <sigilpython/Extend.h>
#include <sigilpython/world/Registration.h>

namespace sigil::python {

void bindWorldPlan(pybind11::module_& module) {
  submodule(module, "world.graph");
}

}  // namespace sigil::python
