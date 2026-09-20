#include <sigilpython/Extend.h>
#include <sigilpython/weave/Registration.h>

namespace sigil::python {

void bindWeaveSelectorUnicode(pybind11::module_& module) {
  submodule(module, "weave.unicode");
}

}  // namespace sigil::python
