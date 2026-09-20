#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryPolylines(pybind11::module_& module) {
  submodule(module, "geometry.path");
  submodule(module, "geometry.sections");
}

}  // namespace sigil::python
