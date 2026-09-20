#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeFeed(pybind11::module_& module) {
  submodule(module, "compose.feed");
}

}  // namespace sigil::python
