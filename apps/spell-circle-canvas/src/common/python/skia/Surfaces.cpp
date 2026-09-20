#include <sigilpython/Extend.h>
#include <sigilpython/skia/Registration.h>

namespace sigil::python {

void bindSkiaSurfaces(pybind11::module_& module) {
  submodule(module, "skia.draw");
}

}  // namespace sigil::python
