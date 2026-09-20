#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryStructures(pybind11::module_& module) {
  submodule(module, "geometry.path");
}

}  // namespace sigil::python
