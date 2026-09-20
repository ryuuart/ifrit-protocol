#include <sigilpython/Extend.h>
#include <sigilpython/geometry/Registration.h>

namespace sigil::python {

void bindGeometryPointOperations(pybind11::module_& module) {
  submodule(module, "geometry.mesh.pop");
}

}  // namespace sigil::python
