#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryMeshCurve(pybind11::module_& module) {
  submodule(module, "geometry.mesh.curve");
}

}  // namespace sigil::python
