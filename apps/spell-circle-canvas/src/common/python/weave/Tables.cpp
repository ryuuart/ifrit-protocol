#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>

namespace sigil::python {

void bindWeaveTables(pybind11::module_& module) {
  submodule(module, "weave.kit.kinsoku");
  submodule(module, "weave.kit.hanging");
}

}  // namespace sigil::python
