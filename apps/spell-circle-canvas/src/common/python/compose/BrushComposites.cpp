#include <sigilpython/Extend.h>
#include <sigilpython/compose/Registration.h>

namespace sigil::python {

void bindComposeBrushComposites(pybind11::module_& module) {
  submodule(module, "compose.brush");
  submodule(module, "compose.brush.strand");
}

}  // namespace sigil::python
