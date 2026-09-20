#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>

namespace sigil::python {

void bindWeavePorts(pybind11::module_& module) {
  submodule(module, "weave.ports");
}

}  // namespace sigil::python
