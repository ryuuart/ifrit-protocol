#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryShapes(pybind11::module_& module) {
  submodule(module, "geometry.shapes");
}

}  // namespace sigil::python
