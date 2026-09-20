#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryMeshPoints(pybind11::module_& module) {
  submodule(module, "geometry.mesh.points");
}

}  // namespace sigil::python
