#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>

namespace sigil::python {

void bindWeaveChoreography(pybind11::module_& module) {
  submodule(module, "weave.paint");
}

}  // namespace sigil::python
