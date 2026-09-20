#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>

namespace sigil::python {

void bindWeaveCascade(pybind11::module_& module) {
  submodule(module, "weave.features");
}

}  // namespace sigil::python
